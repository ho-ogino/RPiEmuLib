#include "PlatformEmu.h"
#include "PlatformScreen.h"
#include "PlatformSound.h"
#include "PlatformLog.h"

extern PlatformScreen* g_platform_screen;
extern PlatformSound* g_platform_sound;

EMU::EMU()
{
    // 基本的には44.1kHzで2205サンプルを作成するのを基本とする
    // 場合によっては(レイテンシーによってサンプル数は変える事(その場合はViceSoundのFRAG_SIZE、NUM_FRAGS、CHUNK_SIZEを変える))
	sound_rate = 44100;
	sound_samples = 2205;

    // ここでサウンドの初期化が必要
    g_platform_sound->initialize(sound_rate, sound_samples);

    mpVM = new VM();

    mpVM->reset();
}

EMU::~EMU()
{
    delete mpVM;
}

int EMU::run()
{
    int extra_frames = 0;
    // printf("[EMU::run] osd->update_sound()\n");
    g_platform_sound->update(&extra_frames);

    if(extra_frames == 0) {
        mpVM->lock();
        mpVM->run();
        mpVM->unlock();
        extra_frames = 1;
    }

    return extra_frames;
}

void EMU::reset()
{
    get_logger()->Write("EMU", LogNotice, "Resetting emulator");
}

void EMU::special_reset()
{
    get_logger()->Write("EMU", LogNotice, "Special reset");
}

void EMU::draw_screen()
{
    g_platform_screen->draw_screen();
}

void EMU::key_down(int code, bool extended, bool repeat)
{
    get_logger()->Write("EMU", LogNotice, "Key down: %d, extended: %d, repeat: %d", code, extended, repeat);

    // 中間コードで加工してから送ってやるのが良いがとりあえずここではこのまま送る
    mpVM->key_down(code, repeat);
}

void EMU::key_up(int code, bool extended)
{
    get_logger()->Write("EMU", LogNotice, "Key up: %d, extended: %d", code, extended);

    // 中間コードで加工してから送ってやるのが良いがとりあえずここではこのまま送る
    mpVM->key_up(code);
}

void EMU::key_char(char code)
{
    get_logger()->Write("EMU", LogNotice, "Key char: %c", code);
}

void EMU::open_floppy_disk(int drv, const char* file_path, int bank)
{
    get_logger()->Write("EMU", LogNotice, "Open floppy disk: %d, file_path: %s, bank: %d", drv, file_path, bank);
}

void EMU::close_floppy_disk(int drv)
{
    get_logger()->Write("EMU", LogNotice, "Close floppy disk: %d", drv);
}

bool EMU::is_floppy_disk_inserted(int drv)
{
    get_logger()->Write("EMU", LogNotice, "Is floppy disk inserted: %d", drv);
    return false;
}

void EMU::play_tape(int drv, const char* file_path)
{
    get_logger()->Write("EMU", LogNotice, "Play tape: %d, file_path: %s", drv, file_path);
}

void EMU::rec_tape(int drv, const char* file_path)
{
    get_logger()->Write("EMU", LogNotice, "Rec tape: %d, file_path: %s", drv, file_path);
}

void EMU::close_tape(int drv)
{
    get_logger()->Write("EMU", LogNotice, "Close tape: %d", drv);
}

void EMU::push_play(int drv)
{
    get_logger()->Write("EMU", LogNotice, "Push play: %d", drv);
}

void EMU::push_stop(int drv)
{
    get_logger()->Write("EMU", LogNotice, "Push stop: %d", drv);
}

void EMU::push_fast_forward(int drv)
{
    get_logger()->Write("EMU", LogNotice, "Push fast forward: %d", drv);
}

void EMU::push_fast_rewind(int drv)
{
    get_logger()->Write("EMU", LogNotice, "Push fast rewind: %d", drv);
}

void EMU::push_apss_forward(int drv)
{
    get_logger()->Write("EMU", LogNotice, "Push apss forward: %d", drv);
}

void EMU::push_apss_rewind(int drv)
{
    get_logger()->Write("EMU", LogNotice, "Push apss rewind: %d", drv);
}

bool EMU::is_tape_inserted(int drv)
{
    get_logger()->Write("EMU", LogNotice, "Is tape inserted: %d", drv);
    return false;
}

bool EMU::is_frame_skippable()
{
    // get_logger()->Write("EMU", LogNotice, "Is frame skippable");
    return true;
}

bool EMU::is_video_recording()
{
    // get_logger()->Write("EMU", LogNotice, "Is video recording");
    return false;
}

bool EMU::is_sound_recording()
{
    // get_logger()->Write("EMU", LogNotice, "Is sound recording");
    return false;
}

int EMU::get_frame_rate()
{
    // get_logger()->Write("EMU", LogNotice, "Get frame rate");
    return 60;
}

int EMU::get_frame_interval()
{
	static int prev_interval = 0;
	static double prev_fps = -1;
	double fps = mpVM->get_frame_rate();
	if(prev_fps != fps) {
		prev_interval = (int)(1024. * 1000. / fps + 0.5);
		prev_fps = fps;
	}
	return prev_interval;
}

void EMU::update_config()
{
    get_logger()->Write("EMU", LogNotice, "Update config");
}
