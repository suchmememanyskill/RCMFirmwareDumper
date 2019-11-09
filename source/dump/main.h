#pragma once

#define INFOMESSAGE "\nPress vol+ to reboot to RCM\n\nPress vol- to reboot to\n    atmosphere/reboot-payload.bin\n\nPress power to dump\n\n\n\nThis project was made possible by:\nshchmue#0472, Dax#5790\n\nWith special thanks to:\ndennthecafebabe#0001\n\nThis program uses the following projects:\nHekate, Lockpick_RCM, Tegraexplorer"
#define DUMPDONEMESSAGE "Dump completed!\nDump is located in /Firmware\n\n\nPress power to reboot to RCM\n\nPress vol+ to reboot to\n    atmosphere/reboot_payload.bin\n\nPress vol- to reboot to bootloader/update.bin"
#define DUMPFAILMESSAGE "Dump FAILED!\n\n\n\nPress power to reboot to RCM\n\nPress vol+ to reboot to\n    atmosphere/reboot_payload.bin\n\nPress vol- to reboot to bootloader/update.bin"
#define BUFFSIZE 32768

int messagebox(char *message);
void mainmenu();