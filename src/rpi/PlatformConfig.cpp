
#include <circle/logger.h>
#include <circle/util.h>
#include "PlatformCommon.h"
#ifdef USE_EXTERNAL_EMU
#include "../emu.h"
#include "PlatformConfig.h"
#include "PlatformLog.h"

// USE_EXTERNAL_EMUを使う場合はNative側configはconfig_t型になる
extern config_t* get_config();

#else
#include "PlatformEmu.h"
#include "PlatformConfig.h"
#include "PlatformLog.h"

extern platform_config_t* get_config();
#endif

#include "ConfigConverter.h"


#ifdef USE_EXTERNAL_EMU
config_t* get_config()
{
    return &config;
}
#else
// USE_EXTERNAL_EMUを使わない場合はNative側configもplatform_config_t型になる
platform_config_t config;
platform_config_t* get_config()
{
    return &config;
}
#endif

PlatformConfig::PlatformConfig(const char* config_path)
{
    strncpy(this->config_path, config_path, _MAX_PATH);

    memset(&m_config, 0, sizeof(platform_config_t));
    mpConfigConverter = new ConfigConverter(&m_config, ::get_config() );

}

PlatformConfig::~PlatformConfig()
{
    if (mpConfigConverter) {
        delete mpConfigConverter;
        mpConfigConverter = nullptr;
    }
}

void PlatformConfig::ConvertToNative()
{
    mpConfigConverter->ConvertToNative();
}

void PlatformConfig::ConvertFromNative()
{
    mpConfigConverter->ConvertFromNative();
}

void PlatformConfig::save_config()
{
    ConvertToNative();
#ifdef USE_EXTERNAL_EMU
    ::save_config(config_path);
#else
	char tmp_path[_MAX_PATH];
    // config_pathの拡張子に$$$を追加したパスを作成 
    strncpy(tmp_path, config_path, _MAX_PATH);
    strncat(tmp_path, ".$$$", _MAX_PATH);

    char key[128];

    // ファイルを作成
    FILE* fp = fopen(tmp_path, "w");
    if (fp) {
        config_write_bool(fp, "sound_noise_fdd", m_config.sound_noise_fdd);
        config_write_bool(fp, "sound_noise_cmt", m_config.sound_noise_cmt);
        config_write_bool(fp, "sound_tape_signal", m_config.sound_tape_signal);
        config_write_bool(fp, "sound_tape_voice", m_config.sound_tape_voice);
        config_write_int(fp, "sound_frequency", m_config.sound_frequency);
        config_write_int(fp, "sound_latency", m_config.sound_latency);
        
        for (int i = 0; i < USE_FLOPPY_DISK; i++) {
            snprintf(key, sizeof(key), "correct_disk_timling[%d]", i);
            config_write_bool(fp, key, m_config.correct_disk_timing[i]);
            snprintf(key, sizeof(key), "ignore_disk_crc[%d]", i);
            config_write_bool(fp, key, m_config.ignore_disk_crc[i]);
            snprintf(key, sizeof(key), "current_floppy_disk_path[%d]", i);
            config_write_string(fp, key, m_config.current_floppy_disk_path[i]);
        }
        
        for (int i = 0; i < USE_TAPE; i++) {
            snprintf(key, sizeof(key), "wave_shaper[%d]", i);
            config_write_bool(fp, key, m_config.wave_shaper[i]);
            snprintf(key, sizeof(key), "baud_high[%d]", i);
            config_write_bool(fp, key, m_config.baud_high[i]);
            snprintf(key, sizeof(key), "current_tape_path[%d]", i);
            config_write_string(fp, key, m_config.current_tape_path[i]);
        }

        config_write_int(fp, "monitor_type", m_config.monitor_type);
        config_write_int(fp, "keyboard_type", m_config.keyboard_type);
        config_write_bool(fp, "full_speed", m_config.full_speed);

        fclose(fp);

        // 削除して$$$をリネームする
        remove(config_path);
        rename(tmp_path, config_path);
    }
#endif
}

void PlatformConfig::load_config()
{
#ifdef USE_EXTERNAL_EMU
    ::load_config(config_path);
#else
    // ファイルを読み込む
    FILE* fp = fopen(config_path, "r");
    if (fp) {
        char line[_MAX_PATH];
        while (fgets(line, sizeof(line), fp)) {
            char* key;
            char* value;
            int int_value;
            split_equal(line, &key, &value);

            // とりあえず雑にintに変換してみる
            if(value != nullptr && *value != '\0') {
                int_value = atoi(value);
            }

            // Keyを認識するのがどれか1つなので、どれか当たるだろうという雑な実装
            apply_config_bool(key, value);
            apply_config_int(key, int_value);
            apply_config_string(key, value);
        }

        fclose(fp);
    }
#endif
    ConvertFromNative();
}


void PlatformConfig::config_write_bool(FILE* fp, const char* key, bool value)
{
    fprintf(fp, "%s=%d\n", key, value ? 1 : 0);
}


void PlatformConfig::config_write_int(FILE* fp, const char* key, int value)
{
    fprintf(fp, "%s=%d\n", key, value);
}

void PlatformConfig::config_write_string(FILE* fp, const char* key, const char* value)
{
    fprintf(fp, "%s=%s\n", key, value);
}

void PlatformConfig::split_equal(const char* line, char** key, char** value)
{
    static char buffer[_MAX_PATH];
    strncpy(buffer, line, _MAX_PATH);
    char* equal = strchr(buffer, '=');
    if (equal) {
        *equal = '\0';
    }
    *key = buffer;
    *value = equal + 1;
}

bool PlatformConfig::get_config_bool(const char* config_name)
{
    int index = -1;
    char* bracket = strchr(config_name, '[');
    if (bracket) {
        index = atoi(bracket + 1);
    }

    if(strcmp(config_name, "sound_noise_fdd") == 0) {
        return m_config.sound_noise_fdd;
    }
    else if(strcmp(config_name, "sound_noise_cmt") == 0) {
        return m_config.sound_noise_cmt;
    }
    else if(strcmp(config_name, "sound_tape_signal") == 0) {
        return m_config.sound_tape_signal;
    }
    else if(strcmp(config_name, "sound_tape_voice") == 0) {
        return m_config.sound_tape_voice;
    }
    else if(strncmp(config_name, "correct_disk_timing[", 20) == 0 && index >= 0) {
        return m_config.correct_disk_timing[index];
    }
    else if(strncmp(config_name, "ignore_disk_crc[", 16) == 0 && index >= 0) {
        return m_config.ignore_disk_crc[index];
    }
    else if(strncmp(config_name, "wave_shaper[", 12) == 0 && index >= 0) {
        return m_config.wave_shaper[index];
    }
    else if(strncmp(config_name, "baud_high[", 10) == 0 && index >= 0) {
        return m_config.baud_high[index];
    }

    return false; // デフォルト値
}

int PlatformConfig::get_config_int(const char* config_name, int* config_value, bool* is_same)
{
    // デフォルト値を設定
    *config_value = 0;
    *is_same = false;

    // config_nameの末尾に [(数字)] というのがあれば、その数字を事前に取得しておく
    int index = -1;
    char* bracket = strchr(config_name, '[');
    if (bracket) {
        index = atoi(bracket + 1);
        *config_value = index;
    }

    extern CLogger* get_logger();
    get_logger()->Write("PlatformMenu", LogNotice, "get_config_int: %s, index: %d", config_name, index);

    if(strncmp(config_name, "monitor_type[", 13) == 0 && index >= 0) {
        int result = 0;
#if defined(USE_MONITOR_TYPE)
        result = m_config.monitor_type;
        get_logger()->Write("PlatformMenu", LogNotice, "get_config_int: %s, index: %d, result: %d", config_name, index, result);
        if (is_same) {
            *is_same = index == result;
        }
#endif
        return result;
    } else if(strncmp(config_name, "keyboard_type[", 14) == 0 && index >= 0) {
        int result = 0;
#if defined(USE_KEYBOARD_TYPE)
        result = m_config.keyboard_type;
        get_logger()->Write("PlatformMenu", LogNotice, "get_config_int: %s, index: %d, result: %d", config_name, index, result);
        if (is_same) {
            *is_same = index == result;
        }
#endif
        return result;
    }
    else if(strncmp(config_name, "full_speed", 10) == 0) {
        int result = 0;
        result = m_config.full_speed;
        get_logger()->Write("PlatformMenu", LogNotice, "get_config_int: %s, index: %d, result: %d", config_name, index, result);
        return result;
    }
    else if(strncmp(config_name, "display_stretch[", 16) == 0 && index >= 0) {
        int result = 0;
        result = m_config.fullscreen_stretch_type;
        get_logger()->Write("PlatformMenu", LogNotice, "get_config_int: %s, index: %d, result: %d", config_name, index, result);
        if (is_same) {
            *is_same = index == result;
        }
        return result;
    }
    return 0;
}

const char* PlatformConfig::get_config_string(const char* config_name)
{
    // config_nameの末尾に [(数字)] というのがあれば、その数字を事前に取得しておく
    int index = -1;
    char* bracket = strchr(config_name, '[');
    if (bracket) {
        index = atoi(bracket + 1);
    }
    
    if(strncmp(config_name, "current_floppy_disk_path[", 25) == 0 && index >= 0) {

        get_logger()->Write("PlatformMenu", LogNotice, "fetch : current_floppy_disk_path[%d] = %s / config = %s", index, m_config.current_floppy_disk_path[index], m_config.current_floppy_disk_path[index]);
        return m_config.current_floppy_disk_path[index];
    }
    else if(strncmp(config_name, "current_tape_path[", 18) == 0 && index >= 0) {
        return m_config.current_tape_path[index];
    }
    else if(strncmp(config_name, "record_tape_path[", 17) == 0 && index >= 0) {
        // テープは再生も録音も同じパスを返す(手抜き)
        return m_config.current_tape_path[index];
    }
    return ""; // デフォルト値
}

void PlatformConfig::apply_config_bool(const char* config_name, bool value)
{
    int index = -1;
    char* bracket = strchr(config_name, '[');
    if (bracket) {
        index = atoi(bracket + 1);
    }

    if(strcmp(config_name, "sound_noise_fdd") == 0) {
        m_config.sound_noise_fdd = value;
    }
    else if(strcmp(config_name, "sound_noise_cmt") == 0) {
        m_config.sound_noise_cmt = value;
    }
    else if(strcmp(config_name, "sound_tape_signal") == 0) {
        m_config.sound_tape_signal = value;
    }
    else if(strcmp(config_name, "sound_tape_voice") == 0) {
        m_config.sound_tape_voice = value;
    }
    else if(strncmp(config_name, "correct_disk_timing[", 20) == 0 && index >= 0) {
        m_config.correct_disk_timing[index] = value;
    }
    else if(strncmp(config_name, "ignore_disk_crc[", 16) == 0 && index >= 0) {
        m_config.ignore_disk_crc[index] = value;
    }
    else if(strncmp(config_name, "wave_shaper[", 12) == 0 && index >= 0) {
        m_config.wave_shaper[index] = value;
    }
    else if(strncmp(config_name, "baud_high[", 10) == 0 && index >= 0) {
        m_config.baud_high[index] = value;
    }
    else if(strncmp(config_name, "full_speed", 10) == 0) {
        m_config.full_speed = value;
    }
}

void PlatformConfig::apply_config_int(const char* config_name, int value)
{
    if(strncmp(config_name, "keyboard_type", 13) == 0) {
#if defined(USE_KEYBOARD_TYPE)  
        m_config.keyboard_type = value;
#endif
    }
    else if(strncmp(config_name, "monitor_type", 12) == 0) {
        m_config.monitor_type = value;
    }
    else if(strncmp(config_name, "display_stretch", 15) == 0) {
        m_config.fullscreen_stretch_type = value;
    }
}

void PlatformConfig::apply_config_string(const char* config_name, const char* value)
{
    // config_nameの末尾に [(数字)] というのがあれば、その数字を事前に取得しておく
    int index = -1;
    char* bracket = strchr(config_name, '[');
    if (bracket) {
        index = atoi(bracket + 1);
    }

    if(strncmp(config_name, "current_floppy_disk_path[", 25) == 0 && index >= 0) {
        strncpy(m_config.current_floppy_disk_path[index], value, _MAX_PATH);
    }
    else if(strncmp(config_name, "current_tape_path[", 18) == 0 && index >= 0) {
        strncpy(m_config.current_tape_path[index], value, _MAX_PATH);
    }
    else if(strncmp(config_name, "record_tape_path[", 17) == 0 && index >= 0) {
        // 録音パスは再生パスと同じなので注意
        strncpy(m_config.current_tape_path[index], value, _MAX_PATH);
    }
}