#pragma once

#if !defined(GAME_PATCH_INFO_H)
#define GAME_PATCH_INFO_H

typedef struct
{
    char titleid[16];
    char app_ver[8];
} GamePatchInfo;

#define HDD_PATH "/dev_hdd0"
#define GAME_PATCH_FOLDER "/game_patch"
#define YML_PATH "/yml"
#define BASE_GAME_PATCH_PATH GAME_PATCH_FOLDER YML_PATH
#define GAME_PATCH_DATA_PATH HDD_PATH GAME_PATCH_FOLDER
#define GAME_PATCH_SETTINGS GAME_PATCH_DATA_PATH "/settings" // per title id .bin
#define GAME_PATCH_FILES_PATH HDD_PATH BASE_GAME_PATCH_PATH
#define GAME_PATCH_WORK_PATH GAME_PATCH_DATA_PATH "/work"
#define GAME_INFO_PATH GAME_PATCH_WORK_PATH "/game_patch_data.bin"
#define USB_PATH "/dev_usb%03ld"

#endif // !defined(GAME_PATCH_INFO_H)
