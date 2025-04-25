
#include "PlatformCommon.h"
#ifdef USE_EXTERNAL_EMU
#include "../emu.h"
#else
#include "PlatformEmu.h"
#endif

#include <circle/logger.h>
#include <stdlib.h>
#include <cstring>
#include <circle/types.h>
#include <circle/sched/scheduler.h>

#include "vicesound.h"

#include "PlatformCommon.h"
#include "PlatformLog.h"
#include "PlatformSound.h"
#include "EmuController.h"
#include "PlatformConfig.h"


#if RPI_MODEL==3
// 非力な環境の場合はこれを定義する
#define SOUND_22050
#endif

// バッファサイズの定義
#define SYSTEMSAMPLES 4410

#ifdef SOUND_22050
#define SOUND_BUFFER_SIZE (SYSTEMSAMPLES * 8)
#define SOUND_BUFFER_HALF (SYSTEMSAMPLES * 4)
#else
#define SOUND_BUFFER_SIZE (m_sound_samples * 8)
#define SOUND_BUFFER_HALF (m_sound_samples * 4)
#endif

#ifdef SOUND_22050
#define SAMPLE_COUNT_MULTIPLIER 2
#else
#define SAMPLE_COUNT_MULTIPLIER 1
#endif

PlatformSound::PlatformSound():
    m_sound_device(nullptr),
    m_scheduler(nullptr),
    m_mix_buffer(nullptr),
    m_emulator(nullptr)
{
}

PlatformSound::~PlatformSound()
{
    release();
}

void PlatformSound::setup(EmuController* emulator, ViceSound* sound_device, CScheduler* scheduler)
{
    get_logger()->Write("PlatformSound", LogNotice, "setup");

    this->m_emulator = emulator;
    this->m_sound_device = sound_device;
    this->m_scheduler = scheduler;
}

void PlatformSound::initialize(int rate, int samples)
{
    get_logger()->Write("PlatformSound", LogNotice, "initialize: rate=%d, samples=%d", rate, samples);

    m_sound_rate = rate;
    m_sound_samples = samples;
    m_sound_available = false;
    m_sound_started = false;
    m_sound_muted = false;

    if (m_mix_buffer) {
        delete[] m_mix_buffer;
        m_mix_buffer = nullptr;
    }

    // バッファを確保
    m_mix_buffer = new u16[SOUND_BUFFER_SIZE / sizeof(u16)];
    memset(m_mix_buffer, 0, SOUND_BUFFER_SIZE);

    if (m_sound_device != nullptr) {
        get_logger()->Write("PlatformSound", LogNotice, "Sound device initialized");

        // 音量を最大に設定
        m_sound_device->SetControl(VCHIQ_SOUND_VOLUME_MAX);

        // 音を止める(やらなくてもいいかも)
        m_sound_device->CancelPlayback();
        while(m_sound_device->PlaybackActive())
        {
            m_scheduler->Yield();
        }

        // ストリーミングで再生開始
        m_sound_device->Playback(100, 2);

        m_sound_available = true;
        m_sound_started = true;
    } else {
        get_logger()->Write("PlatformSound", LogError, "Sound device not initialized");
    }
}

void PlatformSound::release()
{
    if (m_mix_buffer) {
        delete[] m_mix_buffer;
        m_mix_buffer = nullptr;
    }
    m_sound_available = false;
    m_sound_started = false;
    m_sound_muted = false;
}

void PlatformSound::update(int* extra_frames)
{
    *extra_frames = 0;

    m_sound_muted = false;

    if (!m_emulator || !m_sound_available || !m_sound_device) return;

    if (m_sound_available) {
        if (m_sound_device->BufferSpaceSamples() < m_sound_samples * 2 * SAMPLE_COUNT_MULTIPLIER)
        {
            return;
        }

        uint16_t* sound_buffer = m_emulator->create_sound(extra_frames);
        if (sound_buffer) {

            int copy_len = m_sound_samples * 4;
            uint8_t* dst = (uint8_t*)m_mix_buffer;
#ifdef SOUND_22050
            uint16_t* dst16 = (uint16_t*)dst;
            for(int i = 0; i < m_sound_samples; i++) {
                uint16_t l = sound_buffer[i * 2];
                uint16_t r = sound_buffer[i * 2 + 1];

                dst16[i * 4    ] = l;
                dst16[i * 4 + 1] = r;
                dst16[i * 4 + 2] = l;
                dst16[i * 4 + 3] = r;
            }
#else
            memcpy(dst, sound_buffer, copy_len * 2);
#endif

            m_sound_device->AddChunk((int16_t*)dst, m_sound_samples * 2 * SAMPLE_COUNT_MULTIPLIER);
        }
    }
}

void PlatformSound::mute_sound()
{
    if(m_sound_available && !m_sound_muted) {
        memset(m_mix_buffer, 0, SOUND_BUFFER_SIZE);
    }
    m_sound_muted = true;
}

void PlatformSound::stop_sound()
{
    m_sound_started = false;
}
