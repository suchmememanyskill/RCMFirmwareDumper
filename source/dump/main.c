#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../gfx/gfx.h"
#include "../utils/btn.h"
#include "../utils/types.h"
#include "../utils/util.h"
#include "../soc/bpmp.h"
#include "main.h"
#include "gfx.h"
#include "external_utils.h"
#include "../libs/fatfs/ff.h"

extern bool sd_unmount();
static u32 bis_keys[4][8];

menu_item mainmenu[] = {
    {"-- RCMFIRMWAREDUMPER --\n", COLOR_VIOLET, -1, 0},
    {"Dump Firmware\n", COLOR_GREEN, 1, 1},
    {"Reboot to Hekate", COLOR_BLUE, 2, 1},
    {"Reboot to Atmosphere", COLOR_BLUE, 3, 1},
    {"Reboot to RCM", COLOR_ORANGE, 4, 1},
    {"Power off", COLOR_ORANGE, 5, 1}
};

bool checkfile(char* path){
    FRESULT fr;
    FILINFO fno;

    fr = f_stat(path, &fno);

    if (fr & FR_NO_FILE)
        return false;
    else
        return true;
}

void fillmainmenu(){
    for (int i = 0; i < 6; i++)
        switch (mainmenu[i].internal_function){
            case 2:
                if (!checkfile("/bootloader/update.bin"))
                    mainmenu[i].property = -1;
                break;
            case 3:
                if (!checkfile("/atmosphere/reboot_payload.bin"))
                    mainmenu[i].property = -1;
                break;
        }
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

void dumpmenu(){
    int ret;
    //ret = messagebox(INFOMESSAGE);

    fillmainmenu();

    while (1){
        ret = makemenu(mainmenu, 6);

        switch (ret){
            case 1:
                if(!dumpfirmware()){
                    strcpy(mainmenu[0].name, "Dump completed!\nDump is located in /Firmware\n\n");
                    mainmenu[0].color = COLOR_GREEN;
                }
                else {
                    strcpy(mainmenu[0].name, "Dump FAILED!\n\n");
                    mainmenu[0].color = COLOR_RED;
                }

                mainmenu[1].property = -1;
                break;

            case 2:
                bpmp_clk_rate_set(BPMP_CLK_NORMAL);
                launch_payload("bootloader/update.bin", 0);
                break;
            case 3:
                bpmp_clk_rate_set(BPMP_CLK_NORMAL);
                launch_payload("atmosphere/reboot_payload.bin", 0);
                break;
            case 4:
                bpmp_clk_rate_set(BPMP_CLK_NORMAL);
                reboot_rcm();
                break;
            case 5:
                bpmp_clk_rate_set(BPMP_CLK_NORMAL);
                power_off();
                break;
        }
    }
    /*

    if (ret == 1){
            if (dumpfirmware())
                ret = messagebox(DUMPFAILMESSAGE);
            else
                ret = messagebox(DUMPDONEMESSAGE);

            bpmp_clk_rate_set(BPMP_CLK_NORMAL);
            if (ret == 3)
                launch_payload("bootloader/update.bin", 1);
            if (ret == 2)
                launch_payload("atmosphere/reboot_payload.bin", 1);
            reboot_rcm();
    }
    if (ret == 2){
            bpmp_clk_rate_set(BPMP_CLK_NORMAL);
            reboot_rcm();
    }
    else
        bpmp_clk_rate_set(BPMP_CLK_NORMAL);
        launch_payload("atmosphere/reboot_payload.bin", 0);
        launch_payload("bootloader/update.bin", 0);
        reboot_rcm();

    */
}