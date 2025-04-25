/*
    Platform Configuration Structure
    This file defines the configuration structure used by the emulator
*/

#ifndef _PLATFORM_CONFIG_H_
#define _PLATFORM_CONFIG_H_

#include <stdbool.h>
#include <stdio.h>

// プラットフォーム設定構造体
struct platform_config_t {
    // サウンド関連設定
    bool sound_noise_fdd;     // FDDノイズ出力
    bool sound_noise_cmt;     // CMTノイズ出力
    bool sound_tape_signal;   // テープ信号音出力
    bool sound_tape_voice;    // テープ音声出力

    int sound_frequency;
    int sound_latency;
    
    // ディスク関連設定
    bool correct_disk_timing[USE_FLOPPY_DISK];  // ディスクタイミング補正
    bool ignore_disk_crc[USE_FLOPPY_DISK];      // ディスクCRCエラー無視
    char current_floppy_disk_path[USE_FLOPPY_DISK][_MAX_PATH]; // 現在のディスクパス
    
    // テープ関連設定
    bool wave_shaper[USE_TAPE];       // 波形整形
    bool baud_high[USE_TAPE];         // 高速通信
    char current_tape_path[USE_TAPE][_MAX_PATH]; // 現在のテープパス
    
    // 表示関連設定
    int monitor_type;         // モニタータイプ
    
    // キーボード関連設定
    int keyboard_type;        // キーボードタイプ

    int fullscreen_stretch_type;  // フルスクリーンストレッチタイプ

    bool full_speed;         // フルスピード
};

#ifdef USE_EXTERNAL_EMU
extern config_t* get_config();
#else
extern platform_config_t* get_config();
#endif

class ConfigConverter;

class PlatformConfig {
    private:
        ConfigConverter* mpConfigConverter;

        void config_write_bool(FILE* fp, const char* key, bool value);
        void config_write_int(FILE* fp, const char* key, int value);
        void config_write_string(FILE* fp, const char* key, const char* value);

        void split_equal(const char* line, char** key, char** value);

        char config_path[_MAX_PATH];
        platform_config_t m_config;
    public:
        PlatformConfig(const char* config_path);
        ~PlatformConfig();

        platform_config_t* get_config() { return &m_config; }

        const char* get_config_path() { return config_path; }

        void ConvertToNative();
        void ConvertFromNative();

        void save_config();
        void load_config();

        bool get_config_bool(const char* config_name);
        int get_config_int(const char* config_name, int* config_value, bool* is_same);
        const char* get_config_string(const char* config_name);
        void apply_config_bool(const char* config_name, bool value);
        void apply_config_int(const char* config_name, int value);
        void apply_config_string(const char* config_name, const char* value);
};

#endif // _PLATFORM_CONFIG_H_ 