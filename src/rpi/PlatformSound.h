#ifndef PLATFORM_SOUND_H
#define PLATFORM_SOUND_H

#include <circle/types.h>

class ViceSound;
class CScheduler;
class EmuController;

class PlatformSound {
    private:
        ViceSound* m_sound_device;
        CScheduler* m_scheduler;

        int m_sound_rate;
        int m_sound_samples;
        bool m_sound_available;
        bool m_sound_started;
        bool m_sound_muted;

        u16* m_mix_buffer;

        EmuController* m_emulator;
    public:
        PlatformSound();
        ~PlatformSound();

        void setup(EmuController* emulator, ViceSound* sound_device, CScheduler* scheduler);
        void initialize(int rate, int samples);
        void release();

        void update(int* extra_frames);

        void mute_sound();
        void stop_sound();

        int get_sound_rate() { return m_sound_rate; }
        int get_sound_samples() { return m_sound_samples; }
};

#endif
