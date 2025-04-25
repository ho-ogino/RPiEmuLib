#include "PlatformCommon.h"
#ifdef USE_EXTERNAL_EMU
#include "../emu.h"
#else
#include "PlatformEmu.h"
#endif

#include "PlatformScreen.h"
#include "PlatformMenu.h"
#include "PlatformLog.h"
#include "PlatformConfig.h"
#include <circle/types.h>
#include <circle/screen.h>
#include <string.h>

PlatformScreen::~PlatformScreen()
{
    if(m_xmap) {
        free(m_xmap);
        m_xmap = nullptr;
    }
    if(m_draw_buffer_ptr) {
        free(m_draw_buffer_ptr);
        m_draw_buffer_ptr = nullptr;
    }
}

// 空のメソッド実装
void PlatformScreen::initialize(IPlatformEmulator* emulator, int host_width, int host_height, int emu_width, int emu_height, int aspect_width, int aspect_height, PlatformMenu* menu)
{
    m_emulator = emulator;
    m_host_width = host_width;
    m_host_height = host_height;
    m_emu_width = emu_width;
    m_emu_height = emu_height;
    m_aspect_width = aspect_width;
    m_aspect_height = aspect_height;
    m_menu = menu;

    m_circle_screen = nullptr;
    // DISPLAY_WIDTH, DISPLAY_HEIGHTをデフォルトの画面サイズとして使用
    // ただしフレームバッファは確実にそのサイズにはならないので注意
    init_screen_device(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    int stretch_mode = emulator->get_platform_config()->get_config()->fullscreen_stretch_type;
    set_stretch_mode(stretch_mode, true);

    // 次回描画時にset_stretch_modeを実行する
    //device_invalidate = true;
    // frame_buffer_width = (host_height * aspect_width) / aspect_height;
    // frame_buffer_height = host_height;

    // if (frame_buffer_width > host_width) {
    //     frame_buffer_width = host_width;
    //     frame_buffer_height = (host_width * aspect_height) / aspect_width;
    // }
    // 初回は必ず更新させる
    // set_stretch_mode(emulator->get_platform_config()->get_config()->fullscreen_stretch_type, true);

    // エミュレーターが書き込みを行うバッファを確保
    memset(&m_screen_buffer, 0, sizeof(ScreenBitmap_t));
    initialize_screen_buffer(&m_screen_buffer, m_emu_width, m_emu_height, 0);

    //init_screen_device(frame_buffer_width, frame_buffer_height);
}

void PlatformScreen::initialize_screen_buffer(ScreenBitmap_t* buffer, int width, int height, int mode)
{
    get_logger()->Write("PlatformScreen", LogNotice, "initialize_screen_buffer: width=%d, height=%d, buffer=%p", width, height, buffer->buffer);

    release_screen_buffer(buffer);
    buffer->width = width;
    buffer->height = height;
    buffer->buffer = (TScreenColor*)malloc(width * height * sizeof(TScreenColor));
}

void PlatformScreen::release_screen_buffer(ScreenBitmap_t* buffer)
{
    if (buffer->buffer) {
        free(buffer->buffer);
        buffer->buffer = nullptr;
    }
    buffer->width = 0;
    buffer->height = 0;
}

void PlatformScreen::release_screen()
{
    delete m_circle_screen;
    m_circle_screen = nullptr;
}

void PlatformScreen::set_screen_size(int screen_width, int screen_height)
{
    get_logger()->Write("PlatformScreen", LogNotice, "set_screen_size: screen_width=%d, screen_height=%d", screen_width, screen_height);
    if (screen_width != m_emu_width || screen_height != m_emu_height) {
        m_emu_width = screen_width;
        m_emu_height = screen_height;

        // 画面バッファを再初期化
        initialize_screen_buffer(&m_screen_buffer, m_emu_width, m_emu_height, 0);
        get_logger()->Write("PlatformScreen", LogNotice, "set_screen_size: screen_buffer.buffer=%p, screen_buffer.width=%d, screen_buffer.height=%d", m_screen_buffer.buffer, m_screen_buffer.width, m_screen_buffer.height);
    }
}

TScreenColor* PlatformScreen::get_screen_buffer(int y)
{
    if (y < 0 || y >= m_emu_height) return nullptr;
    if (!m_screen_buffer.buffer) return nullptr;

    TScreenColor* addr = m_screen_buffer.buffer + y * m_screen_buffer.width;
    return addr;
}

int PlatformScreen::draw_screen()
{
    if (!m_circle_screen) return 0;

    if (!m_screen_buffer.buffer) {
        get_logger()->Write("PlatformScreen", LogError, "draw_screen: screen_buffer.buffer is nullptr");
        return 0;
    }

    bool debugdraw = false;

    if(m_device_invalidate) {
        get_logger()->Write("PlatformScreen", LogNotice, "draw_screen: device_invalidate");

        // 画面解像度が変わる可能性がある
        init_screen_device(m_host_width, m_host_height);

        // それに伴いdraw_bufferも更新される事がある
        int stretch_mode = m_emulator->get_platform_config()->get_config()->fullscreen_stretch_type;
        set_stretch_mode(stretch_mode, true);

        m_device_invalidate = false;
        debugdraw = true;
    }

    if (m_menu->is_visible()) {
        // 黒背景でクリア
        // memset(screen_buffer.buffer, 0, screen_buffer.width * screen_buffer.height * sizeof(TScreenColor));
        
        // メニューを描画
        if(debugdraw) {
            get_logger()->Write("PlatformScreen", LogNotice, "draw_screen: menu->draw()");
        }
        m_menu->draw();
        if(debugdraw) {
            get_logger()->Write("PlatformScreen", LogNotice, "draw_screen: menu->draw() end");
        }

        // draw_bufferをフレームバッファに描画
        draw_frame_buffer();
        return 1;
    }

    if(debugdraw) {
        get_logger()->Write("PlatformScreen", LogNotice, "draw_screen: emulator->draw_screen_emu()");
    }
    if(m_emulator) m_emulator->draw_screen_emu();
    if(debugdraw) {
        get_logger()->Write("PlatformScreen", LogNotice, "draw_screen: emulator->draw_screen_emu() end");
    }

    // スクリーンバッファの内容を描画バッファにコピー
    // memcpy(frame_buffer_ptr, screen_buffer.buffer, screen_buffer.width * screen_buffer.height * sizeof(TScreenColor));
    if(debugdraw) {
        get_logger()->Write("PlatformScreen", LogNotice, "draw_screen: stretch_blit");
    }
    stretch_blit(m_screen_buffer.buffer, m_screen_buffer.width, m_screen_buffer.height, m_draw_buffer_ptr, m_draw_width, m_draw_height);
    if(debugdraw) {
        get_logger()->Write("PlatformScreen", LogNotice, "draw_screen: stretch_blit end");
    }
    debugdraw = false;

    draw_frame_buffer();

    return 1;
}

void PlatformScreen::draw_frame_buffer()
{
    // 描画バッファをフレームバッファの中心に描画
    int dst_x = (m_frame_buffer_width - m_draw_width) / 2;
    int dst_y = (m_frame_buffer_height - m_draw_height) / 2;
    for(int y = 0; y < m_draw_height; ++y) {
        TScreenColor* src_ptr = m_draw_buffer_ptr + y * m_draw_width;
        TScreenColor* dst_ptr = m_frame_buffer_ptr + (dst_y + y) * m_frame_buffer_pitch + dst_x;
        memcpy(dst_ptr, src_ptr, m_draw_width * sizeof(TScreenColor));
    }
}

void PlatformScreen::stretch_blit(TScreenColor* src, int src_width, int src_height, TScreenColor* dst, int dst_width, int dst_height)
{
    int src_pitch = m_screen_buffer.width;
    int dst_pitch = m_draw_width;

    // Y方向はループ内で計算
    for (int y = 0; y < dst_height; ++y)
    {
        int srcY = (y * src_height) / dst_height;
        const TScreenColor* srcLine = src + srcY * src_pitch;
        TScreenColor* dstLine = dst + y * dst_pitch;

        for (int x = 0; x < dst_width; ++x)
        {
            dstLine[x] = srcLine[m_xmap[x]];
        }
    }


//    int src_pitch = screen_buffer.width;
//    int dst_pitch = frame_buffer_pitch;
//    for (int y = 0; y < dst_height; ++y)
//    {
//        int srcY = (y * src_height) / dst_height;
//        const TScreenColor* srcLine = (const TScreenColor*)(src + srcY * src_pitch);
//
//        TScreenColor* dstLine = (TScreenColor*)(dst + y * dst_pitch);
//
//        for (int x = 0; x < dst_width; ++x)
//        {
//            int srcX = (x * src_width) / dst_width;
//            dstLine[x] = srcLine[srcX];
//        }
//    }
}


// stretch_mode
// 0: ドットバイドット
// 1: ストレッチ
// 2: アスペクト比維持
// 3: フィル
void PlatformScreen::set_stretch_mode(int mode, bool force_update)
{
    if(m_stretch_mode == mode && !force_update) return;
    m_stretch_mode = mode;

    int width, height;

    // ストレッチモードに応じて画面サイズを計算
    if (m_stretch_mode == 0) {
        // ドットバイドット
        int tmp_pow_x = m_host_width / m_emu_width;
        int tmp_pow_y = m_host_height / m_emu_height;
        int tmp_pow = 1;
        if (tmp_pow_y >= tmp_pow_x && tmp_pow_x > 1) {
            tmp_pow = tmp_pow_x;
        } else if (tmp_pow_x >= tmp_pow_y && tmp_pow_y > 1) {
            tmp_pow = tmp_pow_y;
        }
        width = m_emu_width * tmp_pow;
        height = m_emu_height * tmp_pow;
    } else if (m_stretch_mode == 1) {
        // ストレッチ（アスペクト比無視）
        width = (m_host_height * m_emu_width) / m_emu_height;
        height = m_host_height;
        if (width > m_host_width) {
            width = m_host_width;
            height = (m_host_width * m_emu_height) / m_emu_width;
        }
    } else if (m_stretch_mode == 2) {
        // ストレッチ（アスペクト比維持）
        width = (m_host_height * m_aspect_width) / m_aspect_height;
        height = m_host_height;
        if (width > m_host_width) {
            width = m_host_width;
            height = (m_host_width * m_aspect_height) / m_aspect_width;
        }
    } else if (m_stretch_mode == 3) {
        // フィル（画面全体に引き伸ばし）
        width = m_host_width;
        height = m_host_height;
    }

    // width, heightがフレームバッファサイズと異なる場合は更新処理をする
    if(width != m_draw_width || height != m_draw_height) {
        get_logger()->Write("PlatformScreen", LogNotice, "set_stretch_mode: width=%d, height=%d", width, height);
        // 描画バッファを更新

        update_draw_buffer(width, height);
    }
}

void PlatformScreen:: update_draw_buffer(int width, int height)
{
    if(m_draw_buffer_ptr) {
        free(m_draw_buffer_ptr);
        m_draw_buffer_ptr = nullptr;
    }
    m_draw_buffer_ptr = (TScreenColor*)malloc(width * height * sizeof(TScreenColor));

    m_draw_width = width;
    m_draw_height = height;

    // メニュー側に反映(メニューの描画先情報が更新される)
    m_menu->update_draw_buffer();

    // プリコンピュートXマップ(screen→draw)
    if(m_xmap) {
        free(m_xmap);
        m_xmap = nullptr;
    }
    m_xmap = (int*)malloc(m_draw_width * sizeof(int));
    for (int x = 0; x < m_draw_width; ++x)
    {
        m_xmap[x] = (x * m_emu_width) / m_draw_width;
    }
}

const int resolutionCandidates[][2] = {
    // // KVClab液晶はこれで4:3になる(はず)
    // {1024, 768},
    // // ちょっと重いよなあ
    // {1024*2, 768*2},

    // 元の解像度あるいはその2倍
    {WINDOW_WIDTH_ASPECT, WINDOW_HEIGHT_ASPECT},
    {WINDOW_WIDTH_ASPECT * 2, WINDOW_HEIGHT_ASPECT * 2},
    {SCREEN_WIDTH, SCREEN_HEIGHT},
    {SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2},

    // 4:3の解像度
    {640, 480},
    {800, 600},
    {1024, 768},
    {1280, 960},
    {1400, 1050},
    {1600, 1200},
    {1680, 1050},
    {1920, 1080},
    {1920, 1200},

    // 16:9の解像度
    {1280, 720},
    {1366, 768},
    {1600, 900},
    {1920, 1080},
    {1920, 1200},
    {0, 0},

    {-1, -1}
};

void PlatformScreen::init_screen_device(int width, int height)
{
    if(!m_circle_screen) {
        // まずはデフォルト解像度で初期化
        m_circle_screen = new CScreenDevice(width, height);
        // 初回のみ初期化(でいいのかな)
        //circle_screen->Initialize();
    }

    // 4:3の解像度を優先して探すが、無い場合は16:9の解像度を探す。それも無い場合はデフォルト解像度で初期化
    for(int i = 0; resolutionCandidates[i][0] != -1; i++) {
        int candidate_width = resolutionCandidates[i][0];
        int candidate_height = resolutionCandidates[i][1];
        // 指定された解像度より大きい解像度を探す
        if(candidate_width < width || candidate_height < height) {
            continue;
        }
        if(m_circle_screen->Resize(candidate_width, candidate_height))
        {
            int real_width = m_circle_screen->GetWidth();
            int real_height = m_circle_screen->GetHeight();

            // 完全一致してたらOKなので抜ける
            if(real_width == candidate_width && real_height == candidate_height) {
                get_logger()->Write("PlatformScreen", LogNotice, "init_screen_device: circle_screen->Resize() match width=%d, height=%d, real_width=%d, real_height=%d", candidate_width, candidate_height, real_width, real_height);
                break;
            }

            // アスペクト比がcandidatesと一致していれば抜ける
            if(real_width * candidate_height == real_height * candidate_width) {
                get_logger()->Write("PlatformScreen", LogNotice, "init_screen_device: circle_screen->Resize() aspect match width=%d, height=%d, real_width=%d, real_height=%d", candidate_width, candidate_height, real_width, real_height);
                break;
            }

            // そうでない場合は次に進む

            get_logger()->Write("PlatformScreen", LogNotice, "init_screen_device: circle_screen->Resize() not match width=%d, height=%d, real_width=%d, real_height=%d", candidate_width, candidate_height, real_width, real_height);
            // break;
        }
    }


    // if(!circle_screen) {
    //     get_logger()->Write("PlatformScreen", LogNotice, "init_screen_device: circle_screen is nullptr width=%d, height=%d", width, height);
    //     circle_screen = new CScreenDevice(width, height);
    //     circle_screen->Initialize();
    // } else {
    //     get_logger()->Write("PlatformScreen", LogNotice, "init_screen_device: circle_screen is not nullptr width=%d, height=%d", width, height);
    //     circle_screen->Resize(width, height);
    // }

    get_logger()->Write("PlatformScreen", LogNotice, "init_screen_device: circle_screen->Initialize()");

    // フレームバッファ情報を取得
    m_frame_buffer = m_circle_screen->GetFrameBuffer();
    if (!m_frame_buffer) {
        m_circle_screen = nullptr;
        return;
    }

    get_logger()->Write("PlatformScreen", LogNotice, "init_screen_device: frame_buffer->GetBuffer()");

    // 引数のwidth, heightとは異なるサイズになる事がある様子
    m_host_width = m_frame_buffer_width = m_circle_screen->GetWidth();
    m_host_height = m_frame_buffer_height = m_circle_screen->GetHeight();
    m_frame_buffer_ptr = (TScreenColor*)m_frame_buffer->GetBuffer();
    m_frame_buffer_pitch = m_frame_buffer->GetPitch() / sizeof(TScreenColor);

    // フレームバッファをクリア
    memset(m_frame_buffer_ptr, 0, m_frame_buffer_width * m_frame_buffer_height * sizeof(TScreenColor));

    get_logger()->Write("PlatformScreen", LogNotice, "init_screen_device: frame_buffer_width=%d, frame_buffer_height=%d, frame_buffer_pitch=%d", m_frame_buffer_width, m_frame_buffer_height, m_frame_buffer_pitch);

    get_logger()->Write("PlatformScreen", LogNotice, "init_screen_device: menu->update_framebuffer()");

    get_logger()->Write("PlatformScreen", LogNotice, "init_screen_device: end");

}

void PlatformScreen::set_host_window_size(int width, int height, bool mode)
{
    if(width != -1) {
        m_host_width = width;
    }
    if(height != -1) {
        m_host_height = height;
    }

    m_device_invalidate = true;
}