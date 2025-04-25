#ifndef EMU_H
#define EMU_H

class VM;

class EMU
{
    private:
        VM* mpVM;

        int sound_rate;
        int sound_samples;
    public:
        EMU();
        ~EMU();

        VM* get_vm() { return mpVM; }

        int run();

        void reset();
        void special_reset();

        void draw_screen();

	    void key_down(int code, bool extended, bool repeat);
	    void key_up(int code, bool extended);
	    void key_char(char code);

        void open_floppy_disk(int drv, const char* file_path, int bank = 0);
        void close_floppy_disk(int drv);
        bool is_floppy_disk_inserted(int drv);

        void play_tape(int drv, const char* file_path);
        void rec_tape(int drv, const char* file_path);
        void close_tape(int drv);
        void push_play(int drv);
        void push_stop(int drv);
        void push_fast_forward(int drv);
        void push_fast_rewind(int drv);
        void push_apss_forward(int drv);
        void push_apss_rewind(int drv);
        bool is_tape_inserted(int drv);

        bool is_frame_skippable();
        bool is_video_recording();
        bool is_sound_recording();

        int get_frame_rate();
        int get_frame_interval();

        void update_config();
};

#endif // EMU_H