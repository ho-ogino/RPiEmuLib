
#ifndef VM_H
#define VM_H

#include <circle/types.h>

// 4MHz
#define CPU_CLOCKS		4000000

// 一秒あたりのフレーム数
#define FRAMES_PER_SEC	60.0

class VM
{
    private:
        u8* m_memory;

        double next_frame_per_sec;
    public:
        VM();
        ~VM();

        void draw_screen();
    	u16* create_sound(int* extra_frames);

        double get_frame_rate() { return next_frame_per_sec; }

        void lock();
        void unlock();
        void run();

        void reset();
        void special_reset();

	    void key_down(int code, bool repeat);
	    void key_up(int code);
};

#endif // VM_H
