#include <stdlib.h>
#include <cstring>
#include <math.h>

#include "PlatformEmu.h"
#include "PlatformScreen.h"
#include "PlatformSound.h"
#include "PlatformLog.h"

u16 *sound_buffer;

extern PlatformScreen* g_platform_screen;
extern PlatformSound* g_platform_sound;

VM::VM()    
{
	int sound_rate = g_platform_sound->get_sound_rate();
	int sound_samples = g_platform_sound->get_sound_samples();

    // u16のステレオサンプルぶん(4倍)確保
    sound_buffer = (u16*)malloc(sound_samples * sizeof(u16) * 2);

    // 誤差により調整など
    next_frame_per_sec = FRAMES_PER_SEC;
}

VM::~VM()
{
    if(sound_buffer) {
        free(sound_buffer);
        sound_buffer = NULL;
    }
}

void VM::draw_screen()
{
    // 画面を赤色で塗りつぶす
    for(int y = 0; y < g_platform_screen->get_screen_height(); y++) {
        TScreenColor* addr = g_platform_screen->get_screen_buffer(y);
        for(int x = 0; x < g_platform_screen->get_screen_width(); x++) {
            addr[x] = RED_COLOR;
        }
    }
}

u16* VM::create_sound(int* extra_frames)
{
    *extra_frames = 0;

    // 無音を送る
    memset(sound_buffer, 0, sizeof(sound_buffer));

    return sound_buffer;
}

void VM::run()
{
    int clocks_per_frame = (int)((double)CPU_CLOCKS / FRAMES_PER_SEC + 0.5);
    // ここでCPUを進行させる(この計算で正しいか……？)
}

void VM::lock()
{
    // ロックが必要な場合はここでロックする
}

void VM::unlock()
{
    // ロックが必要な場合はここでアンロックする
}

void VM::reset()
{
    // CPUのリセットなど
}

void VM::special_reset()
{
    // NMIなど
}

void VM::key_down(int code, bool repeat)
{
    // キーを押したときの処理
}

void VM::key_up(int code)
{
    // キーを離したときの処理
}
