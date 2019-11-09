#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../utils/types.h"
#include "../utils/util.h"
#include "../soc/bpmp.h"
#include "main.h"
#include "external_utils.h"
#include "../libs/fatfs/ff.h"

extern bool sd_unmount();
static u32 bis_keys[4][8];

void clearscreen(){
    gfx_clear_grey(0x1B);
    gfx_con_setpos(0, 0);
    gfx_box(0, 0, 719, 15, COLOR_WHITE);
    gfx_printf("%k%pRCMFirmwareDumper - By SuchMemeManySkill\n%k%p", COLOR_DEFAULT, COLOR_WHITE, COLOR_WHITE, COLOR_DEFAULT);
}

int messagebox(char *message){
    int ret = -1;
    clearscreen();
    gfx_printf("%s", message);
    msleep(1000);
    u8 res = btn_wait();
        if (res & BTN_POWER) ret = 1;
        else if (res & BTN_VOL_UP) ret = 2;
        else ret = 3;
    clearscreen();
    return ret;
}

int copy(const char *src, const char *dst){
    FIL in;
    FIL out;
    unsigned int res = 0;
    if (strcmp(src, dst) == 0){
        //in and out are the same, aborting!
        return 1;
    }
    res = f_open(&in, src, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK){
        return 2;
    }
    if (f_open(&out, dst, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK){
        return 3;
    }

    u64 size = f_size(&in);
    unsigned long totalsize = size;
    void *buff = malloc(BUFFSIZE);
    while(size > BUFFSIZE){
        f_read(&in, buff, BUFFSIZE, NULL);
        f_write(&out, buff, BUFFSIZE, NULL);

        size = size - BUFFSIZE;
    }
    
    if(size != 0){
        f_read(&in, buff, size, NULL);
        f_write(&out, buff, size, NULL);
    }

    f_close(&in);
    f_close(&out);

    free(buff);

    return 0;
}

bool dumpfirmware(){
    DIR dir;
    FILINFO fno;
    bool fail = false;
    int ret, fsver, amount = 0;
    char path[100] = "emmc:/Contents/registered";
    char sdfolderpath[100] = "";
    char syspath[100] = "";
    char sdpath[100] = "";

    ret = dump_biskeys(bis_keys);
    gfx_printf("PKG1 version: %d\n", ret);
    fsver = ret;

    gfx_printf("Biskeys:\n");
    gfx_hexdump(0, bis_keys[0], 0x20 * sizeof(u8));
    gfx_hexdump(0, bis_keys[1], 0x20 * sizeof(u8));
    gfx_hexdump(0, bis_keys[2], 0x20 * sizeof(u8));

    /*
    ret = f_getfree("sd:/", &clustersize, voidvar);
    sdspace = ((clustersize / 512) / 1024) / 1024;
    gfx_printf("Free space: %d MB (%d)", sdspace, ret); This does not work properly
    */

    ret = f_mkdir("sd:/Firmware");
    gfx_printf("Result making sd:/Firmware %d\n", ret);

    sprintf(sdfolderpath, "sd:/Firmware/%d", fsver);
    ret = f_mkdir(sdfolderpath);
    gfx_printf("Result making %s %d\n", sdfolderpath, ret);

    ret = f_opendir(&dir, path);
    gfx_printf("Result opening system:/ %d\n\n%k", ret, COLOR_GREEN);

    while(!f_readdir(&dir, &fno) && fno.fname[0] && !fail){
        sprintf(sdpath, "%s/%s", sdfolderpath, fno.fname);

        if (fno.fattrib & AM_DIR)
            sprintf(syspath, "%s/%s/00", path, fno.fname);
        else
            sprintf(syspath, "%s/%s", path, fno.fname);

        ret = copy(syspath, sdpath);

        gfx_printf("%d %s\r", ++amount, fno.fname);

        if (ret != 0)
            fail = true;
    }

    if (fail)
        gfx_printf("%k\n\nDump failed! Aborting (%d)", COLOR_RED, ret);

    gfx_printf("%k\n\nPress any button to continue...", COLOR_WHITE);

    btn_wait();

    return fail;
}

void mainmenu(){
    int ret;
    ret = messagebox(INFOMESSAGE);
    if (ret == 1){
            if (dumpfirmware())
                ret = messagebox(DUMPFAILMESSAGE);
            else
                ret = messagebox(DUMPDONEMESSAGE);

            bpmp_clk_rate_set(BPMP_CLK_NORMAL);
            if (ret == 1){
                launch_payload("atmosphere/reboot_payload.bin", 1);

            }
            reboot_rcm();
    }
    if (ret == 2){
            bpmp_clk_rate_set(BPMP_CLK_NORMAL);
            reboot_rcm();
    }
    else
        bpmp_clk_rate_set(BPMP_CLK_NORMAL);
        launch_payload("atmosphere/reboot_payload.bin", 0);
        reboot_rcm();
}