

#include "PlatformCommon.h"
#ifdef USE_EXTERNAL_EMU
#include "../emu.h"
#else
#include "PlatformEmu.h"
#endif

#include <circle/timer.h>
#include <circle/sched/scheduler.h>

#include "PlatformConfig.h"
#include "EmuController.h"
#include "PlatformInterface.h"
#include "PlatformScreen.h"
#include "PlatformSound.h"
#include "PlatformMenu.h"
#include "PlatformInput.h"
#include "PlatformLog.h"
#include "ConfigConverter.h"

#include <string.h>


#ifndef VK_F12
#define VK_F12 0x7B
#endif

// グローバルに参照するための変数(あまり良くないが……)
PlatformScreen* g_platform_screen = nullptr;
PlatformSound* g_platform_sound = nullptr;
PlatformMenu* g_platform_menu = nullptr;
PlatformConfig* g_platform_config = nullptr;

extern const char* create_emu_path(const char *format, ...);

// キーイベントバッファ
#define KEY_BUFFER_SIZE 32
static PlatformKeyEvent keyEvents[KEY_BUFFER_SIZE];
static int keyEventHead = 0;
static int keyEventTail = 0;
static CSpinLock keyBufferLock;  // スレッドセーフのためのロック

EmuController::EmuController()
:
        m_resetgpio_pressed(false),
        m_resetgpio(nullptr)
{
    g_platform_screen = m_platform_screen = new PlatformScreen();
    g_platform_sound = m_platform_sound = new PlatformSound();
    g_platform_menu = m_platform_menu = new PlatformMenu(this);
    g_platform_config = m_platform_config = new PlatformConfig(create_emu_path("%s.ini", CONFIG_NAME));
    m_config = m_platform_config->get_config();

    m_emu = nullptr;
}

EmuController::~EmuController()
{
    if (g_platform_screen) {
        delete g_platform_screen;
        g_platform_screen = nullptr;
    }
    if (g_platform_sound) {
        delete g_platform_sound;
        g_platform_sound = nullptr;
    }
    if (g_platform_menu) {
        delete g_platform_menu;
        g_platform_menu = nullptr;
    }
    if (m_resetgpio) {
        delete m_resetgpio;
        m_resetgpio = nullptr;
    }
    m_emu = nullptr;
}

void EmuController::initialize(int host_width, int host_height, int emu_width, int emu_height, int aspect_width, int aspect_height, ViceSound* sound_device, CScheduler* scheduler)
{
    m_platform_screen->initialize(this, host_width, host_height, emu_width, emu_height, aspect_width, aspect_height, m_platform_menu);
    m_platform_sound->setup(this, sound_device, scheduler);
    m_platform_menu->initialize(this, m_platform_screen);

    m_scheduler = scheduler;

    // 特殊リセット用のGPIOの初期化 (入力モード)
    m_resetgpio = new CGPIOPin(RESET_GPIO_PIN, GPIOModeInput);
    if (!m_resetgpio) {
        get_logger()->Write("GPIO", LogError, "Failed to initialize GPIO%d pin object", RESET_GPIO_PIN);
    } else {
        // GPIO26の内部プルダウン抵抗を有効にする (CGPIOPin::SetPullModeを使用)
        m_resetgpio->SetPullMode(GPIOPullModeDown);
    }

}

/*
void EmuController::set_emulator(EMU* emu)G
{
    this->emu = emu;
}
*/


int EmuController::emulator_main()
{
    get_logger()->Write("EmuController", LogNotice, "Starting emulator main");

    // エミュレーターの設定を読み込む
    load_config();

    // サウンド周波数については固定してやる(スペック的に……どうにかなりそうならそのうち調整)
#if RPI_MODEL==3
    m_config->sound_frequency = 4; // 22.050kHz
#else
    m_config->sound_frequency = 5; // 44.1kHz
#endif
    m_config->sound_latency = 0;

    // Platform側のConfigをEMU側に反映させる
    m_platform_config->ConvertToNative();

    // エミュレータインスタンスを生成(initialize後にしか使えないので注意)
    m_emu = new EMU();

    // configに設定されているディスクを挿入してやる
    for(int drv = 0; drv < USE_FLOPPY_DISK; drv++) {
        if(m_config->current_floppy_disk_path[drv][0] != '\0') {
            insert_floppy_disk(drv, m_config->current_floppy_disk_path[drv], 0);
        }
    }

    // configに設定されているテープを再生してやる
    for(int drv = 0; drv < USE_TAPE; drv++) {
        if(m_config->current_tape_path[drv][0] != '\0') {
            play_tape(drv, m_config->current_tape_path[drv]);
        }
    }

    bool running = true;
    int total_frames = 0, draw_frames = 0, skip_frames = 0;
    u64 next_time_us = 0; // マイクロ秒単位で保持
    bool prev_skip = false;

    while (running) {
        // メインループ
        if (m_emu && running) {
            // timing controls
            int sleep_period_ms = 0;

            // GPIOの状態をチェック (内部プルダウンなのでHIGHで検出)
            bool current_state = m_resetgpio->Read() == HIGH;
            
            // 状態が変化した場合
            if (current_state != m_resetgpio_pressed) {
                m_resetgpio_pressed = current_state;
                
                if (m_resetgpio_pressed) {
                    // ボタンが押された時の処理
                    
                    // FDDの挿入状態を確認
                    bool fdd_inserted[USE_FLOPPY_DISK];
                    for(int drv = 0; drv < USE_FLOPPY_DISK; drv++) {
                        fdd_inserted[drv] = is_floppy_disk_inserted(drv);
                    }
                    
                    // リセット実行(エミュレーター本体)
                    m_emu->reset();

                    // もし挿入されていたら、閉じてファイルに書き込む
                    // (closeするとSDに書くので、セーブなどがここで有効になる)
                    for(int drv = 0; drv < USE_FLOPPY_DISK; drv++) {
                        if (fdd_inserted[drv]) {
                            close_floppy_disk(drv);
                        }
                    }
                    
                    // 必要に応じてディスクを再挿入
                    for(int drv = 0; drv < USE_FLOPPY_DISK; drv++) {
                        if (fdd_inserted[drv]) {
                            open_floppy_disk(drv, m_config->current_floppy_disk_path[drv], 0);
                        }
                    }
                    get_logger()->Write("GPIO", LogNotice, "Reset triggered by GPIO%d (HIGH)", RESET_GPIO_PIN);
                }
            }

            // キーイベントバッファからイベントを取り出して処理
            PlatformKeyEvent keyEvent;
            while (pop_key_event(&keyEvent)) {
                if (keyEvent.type == KEY_EVENT_DOWN) {
                    key_down(keyEvent.code, keyEvent.extended, keyEvent.repeat);
                } else {
                    key_up(keyEvent.code, keyEvent.extended);
                }
            }

            // リセットGPIOが押されていない場合のみエミュレータを実行
            if (!m_resetgpio_pressed) {
                // メニューが表示されている場合はエミュレータの実行をスキップ
                bool menu_enabled = m_platform_menu->is_visible();
                
                // メニューが表示されている場合はVMをスキップし、メニュー描画のみ行う
                if (!menu_enabled) {
                    // drive machine
                    int run_frames = 0;
                    run_frames = m_emu->run();
                    total_frames += run_frames;

                    bool now_skip = (m_config->full_speed || is_frame_skippable()) && !is_video_recording() && !is_sound_recording();

                    if ((prev_skip && !now_skip) || next_time_us == 0) {
                        next_time_us = get_time_us();
                    }

                    if (!now_skip) {
                        static int32_t accum = 0;
                        accum += get_frame_interval();
                        int32_t interval = accum >> 10;     // 1024で割る
                        accum -= interval << 10;           // 1024倍する

                        next_time_us += interval * 1000;
                    }
                    prev_skip = now_skip;
                } else {
                    // メニュー表示中は画面更新だけ行う
                    draw_screen();
                    // メニュー表示中は一定時間のスリープを入れる
                    CTimer::SimpleMsDelay(32); // 約30Hz相当
                }

                // 現在時刻をマイクロ秒単位で取得
                u64 current_time_us = get_time_us();
                
                if (next_time_us > current_time_us) {
                    // メニュー表示中でなければemuのdraw_screenを呼び出す
                    if (!menu_enabled) {
                        draw_frames += draw_screen();
                    }
                    skip_frames = 0;

                    // マイクロ秒の差分から必要なミリ秒のスリープ時間を計算
                    current_time_us = get_time_us();

                    int32_t next_time_ms = (int32_t)(next_time_us / 1000);
                    int32_t current_time_ms = (int32_t)(current_time_us / 1000);
                    
                    if ((int)(next_time_ms - current_time_ms) >= 10) {
                        sleep_period_ms = (int)(next_time_ms - current_time_ms);
                    }
                } else if (++skip_frames > (int)get_frame_rate()) {
                    // update window at least once per 1 sec in virtual machine time
                    // メニュー表示中でなければemuのdraw_screenを呼び出す
                    if (!menu_enabled) {
                        draw_frames += draw_screen();
                    }
                    skip_frames = 0;

                    next_time_us = get_time_us();
                } else {
                    // フレームスキップ中も特別な処理は行わない
                }

                if (sleep_period_ms > 0) {
                    // スリープ時間を計測
                    CTimer::SimpleMsDelay(sleep_period_ms);
                }
            } else {
                // リセトGPIOが押されている間は画面を更新するだけ
                draw_frames += draw_screen();
                CTimer::SimpleMsDelay(32); // 少し待機してCPU負荷を下げる
            }
            
            m_scheduler->Yield();
        }
    }

    return 0;
}

void EmuController::play_tape(int drv, const char* file_path)
{
    get_logger()->Write("EmuController", LogNotice, "play_tape: %s", file_path);
    if (drv < 0 || drv >= USE_TAPE || !file_path) {
        return;
    }

    strncpy(m_config->current_tape_path[drv], file_path, _MAX_PATH - 1);
    m_config->current_tape_path[drv][_MAX_PATH - 1] = '\0'; // NULL終端を保証

    if (m_emu) {
        get_logger()->Write("EmuController", LogNotice, "play_tape(emu): %s", file_path);
        m_emu->play_tape(drv, file_path);
    }
}

void EmuController::record_tape(int drv, const char* file_path)
{
    if (drv < 0 || drv >= USE_TAPE || !file_path) {
        return;
    }
    strncpy(m_config->current_tape_path[drv], file_path, _MAX_PATH - 1);
    m_config->current_tape_path[drv][_MAX_PATH - 1] = '\0'; // NULL終端を保証

    if (m_emu) {
        m_emu->rec_tape(drv, file_path);
    }
}

void EmuController::eject_tape(int drv)
{
    if (drv < 0 || drv >= USE_TAPE) {
        return;
    }

    m_config->current_tape_path[drv][0] = '\0';

    if (m_emu) {
        m_emu->close_tape(drv);
    }
}

void EmuController::insert_floppy_disk(int drv, const char* file_path, int bank)
{
    if (drv < 0 || drv >= USE_FLOPPY_DISK || !file_path) {
        return;
    }
    
    strncpy(m_config->current_floppy_disk_path[drv], file_path, _MAX_PATH - 1);
    m_config->current_floppy_disk_path[drv][_MAX_PATH - 1] = '\0'; // NULL終端を保証

    if (m_emu) {
        m_emu->open_floppy_disk(drv, file_path, bank);
    }
} 

void EmuController::eject_floppy_disk(int drv)
{
    if (drv < 0 || drv >= USE_FLOPPY_DISK) {
        return;
    }

    m_config->current_floppy_disk_path[drv][0] = '\0';

    if (m_emu) {
        m_emu->close_floppy_disk(drv);
    }
}

void EmuController::close_floppy_disk(int drv)
{
    if (m_emu) {
        m_emu->close_floppy_disk(drv);
    }
}

void EmuController::open_floppy_disk(int drv, const char* file_path, int bank)
{
    if (m_emu) {
        m_emu->open_floppy_disk(drv, file_path, bank);
    }
}
void EmuController::push_key_event(KeyEventType type, uint8_t code, bool extended, bool repeat)
{
    keyBufferLock.Acquire();
    
    // 次の書き込み位置を計算
    int nextTail = (keyEventTail + 1) % KEY_BUFFER_SIZE;
    
    // バッファがオーバーフローしないか確認
    if (nextTail != keyEventHead) {
        // イベントをバッファに追加
        keyEvents[keyEventTail].type = type;
        keyEvents[keyEventTail].code = code;
        keyEvents[keyEventTail].extended = extended;
        keyEvents[keyEventTail].repeat = repeat;
        keyEventTail = nextTail;
    }
    
    keyBufferLock.Release();
}

// キーイベントをバッファから取得する関数
bool EmuController::pop_key_event(PlatformKeyEvent* pEvent)
{
    bool result = false;
    
    keyBufferLock.Acquire();
    
    // バッファが空でないか確認
    if (keyEventHead != keyEventTail) {
        // イベントを取得
        *pEvent = keyEvents[keyEventHead];
        keyEventHead = (keyEventHead + 1) % KEY_BUFFER_SIZE;
        result = true;
    }
    
    keyBufferLock.Release();
    return result;
}

void EmuController::key_down(int code, bool extended, bool repeat)
{
    // メニューが開いているか
    if (m_platform_menu->is_visible()) {
        m_platform_menu->process_key(code, true);
        return;
    }

    // F12が押されたらメニュー出しますよ
    if(code == VK_F12) {
        m_platform_menu->toggle_menu();
        return;
    }

    if (m_emu) {
        m_emu->key_down(code, extended, repeat);
    }
}

void EmuController::key_up(int code, bool extended)
{
    // メニューが開いているか
    if (m_platform_menu->is_visible()) {
        m_platform_menu->process_key(code, false);
        return;
    }

    if (m_emu) {
        m_emu->key_up(code, extended);
    }
}

bool EmuController::is_frame_skippable()
{
    if (m_emu) {
        return m_emu->is_frame_skippable();
    }
    return false;
}

bool EmuController::is_video_recording()
{
    if (m_emu) {
        return m_emu->is_video_recording();
    }
    return false;
}

bool EmuController::is_sound_recording()
{
    if (m_emu) {
        return m_emu->is_sound_recording();
    }
    return false;
}

int EmuController::get_frame_interval()
{
    if (m_emu) {
        return m_emu->get_frame_interval();
    }
}

int EmuController::draw_screen()
{
    return m_platform_screen->draw_screen();
}

void EmuController::draw_screen_emu()
{
    m_emu->get_vm()->draw_screen();

}

void EmuController::load_config()
{
    m_platform_config->load_config();
}

void EmuController::save_config()
{
    m_platform_config->save_config();
}

u64 EmuController::get_time_us()
{
    return CTimer::GetClockTicks64();
}

int EmuController::get_frame_rate()
{
    if (m_emu) {
        return m_emu->get_frame_rate();
    }
    return 60;
}

void EmuController::update_config()
{
    // 事前にPlatform側をNative側に変換すべきか？(要検討)
    // m_platform_config->ConvertToNative();
    if (m_emu) {
        m_emu->update_config();
    }
}

void EmuController::push_play(int drv)
{
    if (m_emu) {
        m_emu->push_play(drv);
    }
}

void EmuController::push_stop(int drv)
{
    if (m_emu) {
        m_emu->push_stop(drv);
    }
}

void EmuController::push_fast_forward(int drv)
{
    if (m_emu) {
        m_emu->push_fast_forward(drv);
    }
}

void EmuController::push_fast_rewind(int drv)
{
    if (m_emu) {
        m_emu->push_fast_rewind(drv);
    }
}

void EmuController::push_apss_forward(int drv)
{
    if (m_emu) {
        m_emu->push_apss_forward(drv);
    }
}

void EmuController::push_apss_rewind(int drv)
{
    if (m_emu) {
        m_emu->push_apss_rewind(drv);
    }
}

u16* EmuController::create_sound(int* extra_frames)
{
    if (m_emu) {
        return m_emu->get_vm()->create_sound(extra_frames);
    }
    return 0;
}

void EmuController::reset()
{
    if (m_emu) {
        m_emu->reset();
    }
}

void EmuController::special_reset()
{
#ifdef USE_SPECIAL_RESET
    if (m_emu) {
        m_emu->special_reset();
    }
#endif
}

bool EmuController::is_floppy_disk_inserted(int drv)
{
    if (m_emu) {
        return m_emu->is_floppy_disk_inserted(drv);
    }
}

bool EmuController::is_tape_inserted(int drv)
{
    if (m_emu) {
        return m_emu->is_tape_inserted(drv);
    }
    return false;
}

void EmuController::close_tape(int drv)
{
    if (m_emu) {
        m_emu->close_tape(drv);
    }
}

const char* EmuController::get_config_path()
{
    return m_platform_config->get_config_path();
}

void EmuController::set_host_window_size(int width, int height, bool mode)
{
    m_platform_screen->set_host_window_size(width, height, mode);
}
