

#include "PlatformCommon.h"
#include "PlatformInput.h"

// 特殊リセットに使うGPIOピン番号
#define RESET_GPIO_PIN 26

class EMU;

class platform_config_t;
class PlatformScreen;
class PlatformSound;
class PlatformMenu;
class PlatformConfig;
class ViceSound;
class CGPIOPin;

/// @brief エミュレーターのコントローラー(Facade)
class EmuController: public IPlatformEmulator {
    private:
        // エミュレーターのインスタンスを保持する
        EMU* m_emu;
        // PlatformConfig実体
        platform_config_t* m_config;

        PlatformScreen* m_platform_screen;
        PlatformSound* m_platform_sound;
        PlatformMenu* m_platform_menu;
        PlatformConfig* m_platform_config;

        bool m_resetgpio_pressed;
        CGPIOPin* m_resetgpio;

        CScheduler* m_scheduler;

        const char* get_config_path();
        platform_config_t* get_config() { return m_config; }
    public:
        EmuController();
        ~EmuController();

        void initialize(int host_width, int host_height, int emu_width, int emu_height, int aspect_width, int aspect_height, ViceSound* sound_device, CScheduler* scheduler);
        int emulator_main();

        void set_host_window_size(int width, int height, bool mode);

        void push_key_event(KeyEventType type, uint8_t code, bool extended, bool repeat);
        bool pop_key_event(PlatformKeyEvent* pEvent);
    
        void key_down(int code, bool extended, bool repeat);
        void key_up(int code, bool extended);
        void key_char(char code);
        void press_button(int num);
        void set_auto_key_code(int code);
        void set_auto_key_list(char *buf, int size);

        bool is_auto_key_running();
        void start_auto_key();
        void stop_auto_key();

        void eject_floppy_disk(int drv);
        void insert_floppy_disk(int drv, const char* file_path, int bank = 0);

        void eject_tape(int drv);
        void play_tape(int drv, const char* file_path);
        void record_tape(int drv, const char* file_path);

        void save_config();
        void load_config();
        void update_config();
        void reset();
        void special_reset();

        bool is_floppy_disk_inserted(int drv);
        bool is_tape_inserted(int drv);
        void close_floppy_disk(int drv);
        void open_floppy_disk(int drv, const char* file_path, int bank = 0);
        void close_tape(int drv);
        void push_play(int drv);
        void push_stop(int drv);
        void push_fast_forward(int drv);
        void push_fast_rewind(int drv);
        void push_apss_forward(int drv);
        void push_apss_rewind(int drv);

        u16* create_sound(int* extra_frames);

        bool is_frame_skippable();
        bool is_video_recording();
        bool is_sound_recording();

        int get_frame_rate();
        int get_frame_interval();
        int draw_screen();
        void draw_screen_emu();

        u64 get_time_us();

        PlatformConfig* get_platform_config() { return m_platform_config; }
};