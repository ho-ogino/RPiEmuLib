#ifndef _PLATFORM_EMU_H_
#define _PLATFORM_EMU_H_

// エミュレーターの実VRAMサイズ
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 400

// エミュレーターの画面サイズ(SCREEN_WIDTH x SCREEN_HEIGHTをこのエリアに描画するとアスペクト比があう)
#define WIDTH_ASPECT 640
#define HEIGHT_ASPECT 480

#ifndef WINDOW_WIDTH_ASPECT
#define WINDOW_WIDTH_ASPECT WIDTH_ASPECT
#endif

#ifndef WINDOW_HEIGHT_ASPECT
#define WINDOW_HEIGHT_ASPECT HEIGHT_ASPECT
#endif

#ifndef CONFIG_NAME
#define CONFIG_NAME "emuconf"
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "RPiEmu"
#endif

#ifndef USE_SPECIAL_RESET
#define USE_SPECIAL_RESET
#endif

#ifndef USE_FLOPPY_DISK
#define USE_FLOPPY_DISK 4
#endif

#ifndef USE_TAPE
#define USE_TAPE 1
#endif

#ifndef USE_HARD_DISK
#define USE_HARD_DISK 4
#endif

#include <circle/types.h>

#ifndef _MAX_PATH
#define _MAX_PATH 2048
#endif

// エミュレータークラス(ここを適切に書き換えてください)
#include "vm.h"
#include "emu.h"
// #include "vm6502.h"
// #include "emu6502.h"

#endif
