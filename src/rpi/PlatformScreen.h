
#ifndef _PLATFORM_SCREEN_H_
#define _PLATFORM_SCREEN_H_

#include <circle/types.h>
#include <circle/screen.h>

#include "PlatformInterface.h"

#include "FBConsole.hpp"

typedef struct ScreenBitmap_s {
    int width, height;
    TScreenColor* buffer;
} ScreenBitmap_t;

class PlatformMenu;

class PlatformScreen
{
public:
    PlatformScreen():
        m_circle_screen(nullptr),
        m_frame_buffer(nullptr),
        m_frame_buffer_width(0),
        m_frame_buffer_height(0),
        m_frame_buffer_ptr(nullptr),
        m_draw_buffer_ptr(nullptr),
        m_frame_buffer_pitch(0),
        m_device_invalidate(false),
        m_xmap(nullptr)
    {}
    ~PlatformScreen();
    void initialize(IPlatformEmulator* emulator, int host_width, int host_height, int emu_width, int emu_height, int aspect_width, int aspect_height, PlatformMenu* menu);

    int get_screen_width() { return m_emu_width; }
    int get_screen_height() { return m_emu_height; }

    int get_draw_width() { return m_draw_width; }
    int get_draw_height() { return m_draw_height; }

    int get_frame_buffer_width() { return m_frame_buffer_width; }
    int get_frame_buffer_height() { return m_frame_buffer_height; }
    int get_frame_buffer_pitch() { return m_frame_buffer_pitch; }

    void set_screen_size(int screen_width, int screen_height);
    void initialize_screen_buffer(ScreenBitmap_t* buffer, int width, int height, int mode);
    void release_screen_buffer(ScreenBitmap_t* buffer);
    void release_screen();

    TScreenColor* get_screen_buffer(int y);
    TScreenColor* get_frame_buffer_ptr() { return m_frame_buffer_ptr; }
    TScreenColor* get_draw_buffer_ptr() { return m_draw_buffer_ptr; }

    void set_stretch_mode(int mode, bool force_update = false);
    void set_host_window_size(int width, int height, bool mode);

    int draw_screen();
private:
    void init_screen_device(int width, int height);
    void update_draw_buffer(int width, int height);
    void stretch_blit(TScreenColor* src, int src_width, int src_height, TScreenColor* dst, int dst_width, int dst_height);

    void draw_frame_buffer();

    int m_stretch_mode;

    // 画面サイズ(フルスクリーンのデバイスのサイズ)
    int m_host_width;
    int m_host_height;

    // エミュレーター側からスケールされた描画される画面サイズ(host_width, host_heightにおさまるように調整されているサイズ)
    int m_draw_width;
    int m_draw_height;

    // エミュレーター側の画面サイズ
    int m_emu_width;
    int m_emu_height;

    // エミュレーターのアスペクト比を調整するためのサイズ
    int m_aspect_width;
    int m_aspect_height;

    int* m_xmap;

    bool m_device_invalidate;

    ScreenBitmap_t m_screen_buffer;

    CScreenDevice* m_circle_screen;
    CBcmFrameBuffer* m_frame_buffer;
    int m_frame_buffer_width;
    int m_frame_buffer_height;
    TScreenColor* m_frame_buffer_ptr;
    TScreenColor* m_draw_buffer_ptr;
    size_t m_frame_buffer_pitch;

    IPlatformEmulator* m_emulator;

    PlatformMenu* m_menu;
};

#endif
