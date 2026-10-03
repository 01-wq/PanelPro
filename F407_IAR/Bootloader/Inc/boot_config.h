#ifndef __BOOT_CONFIG_H__
#define __BOOT_CONFIG_H__

#define BOOTLOADER_BASE     0x08000000  //bootloader起始地址
#define BOOTLOADER_SIZE     0x00008000  //bootloader大小(32KB)
#define METADATA_BASE       0x08008000  //metadata起始地址(sector 2)
#define METADATA_SIZE       0x00004000  //metadata大小(16KB)
#define APP_BASE            0x0800C000  //应用程序起始地址(sector 3)
#define APP_SIZE            0x00074000  //应用程序大小(464KB)
#define OTA_BOOT_MAGIC      0xB007DA7A  //OTA升级标志
#define BOOTLOADER_VERSION  0x0100      //bootloader版本号(1.0.0)

#endif
