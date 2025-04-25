#include <stdlib.h>
#include <cstring>
#include <math.h>

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#include "PlatformEmu.h"
#include "PlatformScreen.h"
#include "PlatformSound.h"
#include "PlatformLog.h"

u16 *sound_buffer;
u8* g_memory;

extern uint16_t pc;
extern "C" void reset6502();
extern "C" void exec6502(uint32_t tickcount);
extern "C" void nmi6502();
extern "C" void step6502(int printRegs);

extern PlatformScreen* g_platform_screen;
extern PlatformSound* g_platform_sound;

// $C000からのプログラム($2000〜)
u8 prog[] = {
0xA9, 0x00, 0x8D, 0x00, 0x20, 0xAD, 0x00, 0x20,
0x85, 0x00, 0xA9, 0x00, 0x85, 0x01, 0xA9, 0x20,
0x85, 0x02, 0xA0, 0x00, 0xA5, 0x00, 0x91, 0x01,
0xE6, 0x00, 0xE6, 0x01, 0xD0, 0x02, 0xE6, 0x02,
0xA5, 0x02, 0xC9, 0x40, 0xD0, 0xEC, 0xA5, 0x01,
0xC9, 0x00, 0xF0, 0x03, 0x4C, 0x12, 0xC0, 0xEE,
0x00, 0x20, 0x4C, 0x05, 0xC0
};
// ↑
//     .ORG $C000          ; プログラムの開始アドレス
// 
// START:
//     LDA #$00
//     STA $2000
// TOPLOOP:
//     LDA $2000
//     STA $00
//     
//     ; ポインタを$2000に設定
//     LDA #$00            ; アドレスポインタの下位バイトを$00に設定
//     STA $01             ; $01に下位バイトを格納
//     LDA #$20            ; アドレスポインタの上位バイトを$20に設定
//     STA $02             ; $02に上位バイトを格納（$01/$02で$2000を指す）
// 
//     LDY #$00            ; Yレジスタを常に0に設定（間接アドレッシング用）
// LOOP:
//     LDA $00             ; カウンタ値をロード
//     STA ($01),Y         ; 間接アドレッシングでメモリに書き込み
//     
//     ; カウンタをインクリメント
//     INC $00             ; カウンタ値を増やす
//     
//     ; 次のアドレスに進む
//     INC $01             ; 下位バイトをインクリメント
//     BNE SKIP            ; 下位バイトが0でなければSKIPへ
//     INC $02             ; 下位バイトが0になったら上位バイトをインクリメント
// SKIP:
//     ; $4000に達したか確認
//     LDA $02             ; 上位バイトをロード
//     CMP #$40            ; $40と比較
//     BNE LOOP            ; $40でなければループ継続
//     LDA $01             ; 下位バイトをロード
//     CMP #$00            ; $00と比較（$4000になったか）
//     BEQ DONE            ; $4000になったら終了
//     JMP LOOP            ; まだ$4000になっていなければループ継続
// 
// DONE:
//     INC $2000
//     JMP TOPLOOP

TScreenColor draw_colors[7] = {
    RED_COLOR,
    GREEN_COLOR,
    YELLOW_COLOR,
    BLUE_COLOR,
    MAGENTA_COLOR,
    CYAN_COLOR,
    WHITE_COLOR
};
int draw_color_index = 0;

extern "C" {

uint8_t read6502(uint16_t address) {
    if(address >= 0x2000 && address <= 0x3fff)
    {
        // ここはVRAM領域だがそのまま読む(手抜き)
    }
    return g_memory[address];
}

void write6502(uint16_t address, uint8_t value) {
    if(address >= 0x2000 && address <= 0x3fff)
    {
        // ここはVRAM領域だがそのまま書く(手抜き)
    }
    g_memory[address] = value;
}

}

VM::VM()    
{
	int sound_rate = g_platform_sound->get_sound_rate();
	int sound_samples = g_platform_sound->get_sound_samples();

    // u16のステレオサンプルぶん(4倍)確保
    sound_buffer = (u16*)malloc(sound_samples * sizeof(u16) * 2);
    g_memory = (u8*)malloc(64 * 1024);
}

VM::~VM()
{
    if(sound_buffer) {
        free(sound_buffer);
        sound_buffer = NULL;
    }
    if(g_memory) {
        free(g_memory);
        g_memory = NULL;
    }
}

void VM::draw_screen()
{
    // ここでエミュレーターの画面を描画する
    // $2000〜$3fffがモノクロのVRAMなので、各ドットになおして描画する
    // 横256なので注意
    for(int y = 0; y < g_platform_screen->get_screen_height(); y++) {
        TScreenColor* addr = g_platform_screen->get_screen_buffer(y);
        for(int x = 0; x < 256 / 8; x++) {
            // モノクロのVRAMをドットになおす
            u8 colorbit = g_memory[0x2000 + (y * 256 / 8 + x)];
            // ドットになおした色を設定する(横2倍)
            for(int i = 0; i < 16; i++) {
                bool is_color = (colorbit & (1 << (7 - i/2))) ? true : false;
                addr[x * 16 + i] = is_color ? draw_colors[draw_color_index] : BLACK_COLOR;
            }
        }
    }
}

#define FREQUENCY 261.63
#define AMPLITUDE 16384.0
#define OFFSET    32768.0
#define TWO_PI    (2.0 * M_PI)

u16* VM::create_sound(int* extra_frames)
{
    *extra_frames = 0;

    // サイン波を作ってやる
    int sample_rate = g_platform_sound->get_sound_rate();
    int sample_count = g_platform_sound->get_sound_samples();

    static double phase_value = 0;
    double* phase = &phase_value;

    double phase_increment = TWO_PI * FREQUENCY / sample_rate;

    for (int i = 0; i < sample_count; ++i) {
        double s = sin(*phase);
        sound_buffer[i * 2    ] = (uint16_t)(s * AMPLITUDE + OFFSET);
        sound_buffer[i * 2 + 1] = (uint16_t)(s * AMPLITUDE + OFFSET);
        *phase += phase_increment;

        // Wrap phase to [0, 2π) to prevent floating point drift
        if (*phase >= TWO_PI) {
            *phase -= TWO_PI;
        }
    }

    return sound_buffer;
}

void VM::run()
{
    int clocks_per_frame = (int)((double)CPU_CLOCKS / FRAMES_PER_SEC + 0.5);

    // 1フレームぶん実行させる
    exec6502(clocks_per_frame);
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
    // メモリをクリア(VRAMもクリアされる)
    memset(g_memory, 0, 64 * 1024);

    // $c000からプログラムを書きこむ(手抜き)
    memcpy(g_memory + 0xc000, prog, sizeof(prog));
    reset6502();

    // エントリーポイントはなぜか$c000
    pc = 0xc000;
}

void VM::special_reset()
{
    nmi6502();
}

void VM::key_down(int code, bool repeat)
{
    // キーを押したときの処理(描画色を変える)
    if(code >= 0x31 && code <= 0x38) {
        // 0 〜 7
        draw_color_index = code - 0x31;
    }
}

void VM::key_up(int code)
{
    // キーを離したときの処理
}
