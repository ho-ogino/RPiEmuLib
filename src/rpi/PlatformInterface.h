
#ifndef _PLATFORM_INTERFACE_H_
#define _PLATFORM_INTERFACE_H_

#include <circle/types.h>

class PlatformConfig;

class IPlatformEmulator {
    public:
        virtual int draw_screen() = 0;
        virtual void draw_screen_emu() = 0;

        virtual void eject_floppy_disk(int drv) = 0;
        virtual void insert_floppy_disk(int drv, const char* file_path, int bank = 0) = 0;

        virtual void eject_tape(int drv) = 0;
        virtual void play_tape(int drv, const char* file_path) = 0;
        virtual void record_tape(int drv, const char* file_path) = 0;
        virtual void push_play(int drv) = 0;
        virtual void push_stop(int drv) = 0;
        virtual void push_fast_forward(int drv) = 0;
        virtual void push_fast_rewind(int drv) = 0;
        virtual void push_apss_forward(int drv) = 0;
        virtual void push_apss_rewind(int drv) = 0;

        virtual void save_config() = 0;
        virtual void load_config() = 0;
        virtual void update_config() = 0;
        virtual void reset() = 0;
        virtual void special_reset() = 0;

        virtual bool is_floppy_disk_inserted(int drv) = 0;
        virtual bool is_tape_inserted(int drv) = 0;
        virtual void close_floppy_disk(int drv) = 0;
        virtual void open_floppy_disk(int drv, const char* file_path, int bank = 0) = 0;
        virtual void close_tape(int drv) = 0;

        virtual u16* create_sound(int* extra_frames) = 0;

        virtual PlatformConfig* get_platform_config() = 0;
};

#endif