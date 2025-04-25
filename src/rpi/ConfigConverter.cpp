
#include <string.h>
#include "PlatformCommon.h"
#ifdef USE_EXTERNAL_EMU
#include "../emu.h"
#include "../config.h"
#else
#include "PlatformEmu.h"
#endif

#include "PlatformConfig.h"
#include "ConfigConverter.h"


ConfigConverter::ConfigConverter(CONFIG_RPI configRPI, CONFIG_NATIVE configNative)
{
    m_config_RPI = configRPI;
    m_config_native = configNative;
}

void ConfigConverter::ConvertToNative()
{
    m_config_native->sound_frequency = m_config_RPI->sound_frequency;
    m_config_native->sound_latency = m_config_RPI->sound_latency;

    for(int drv = 0; drv < USE_FLOPPY_DISK; drv++) {
        strncpy(m_config_native->current_floppy_disk_path[drv], m_config_RPI->current_floppy_disk_path[drv], _MAX_PATH - 1);
        m_config_native->current_floppy_disk_path[drv][_MAX_PATH - 1] = '\0'; // NULL終端を保証
    }
    
    for(int drv = 0; drv < USE_TAPE; drv++) {
        strncpy(m_config_native->current_tape_path[drv], m_config_RPI->current_tape_path[drv], _MAX_PATH - 1);
        m_config_native->current_tape_path[drv][_MAX_PATH - 1] = '\0'; // NULL終端を保証
    }

#if defined(USE_MONITOR_TYPE)
    m_config_native->monitor_type = m_config_RPI->monitor_type;
#endif

#if defined(USE_KEYBOARD_TYPE)
    m_config_native->keyboard_type = m_config_RPI->keyboard_type;
#endif

    m_config_native->fullscreen_stretch_type = m_config_RPI->fullscreen_stretch_type;

    m_config_native->sound_noise_fdd = m_config_RPI->sound_noise_fdd;
    m_config_native->sound_noise_cmt = m_config_RPI->sound_noise_cmt;
    m_config_native->sound_tape_signal = m_config_RPI->sound_tape_signal;
    m_config_native->sound_tape_voice = m_config_RPI->sound_tape_voice;

    for(int drv = 0; drv < USE_FLOPPY_DISK; drv++) {
        m_config_native->correct_disk_timing[drv] = m_config_RPI->correct_disk_timing[drv];
        m_config_native->ignore_disk_crc[drv] = m_config_RPI->ignore_disk_crc[drv];
    }

    for(int drv = 0; drv < USE_TAPE; drv++) {   
        m_config_native->wave_shaper[drv] = m_config_RPI->wave_shaper[drv];
        m_config_native->baud_high[drv] = m_config_RPI->baud_high[drv];
    }
}

void ConfigConverter::ConvertFromNative()
{
    m_config_RPI->sound_frequency = m_config_native->sound_frequency;
    m_config_RPI->sound_latency = m_config_native->sound_latency;

    for(int drv = 0; drv < USE_FLOPPY_DISK; drv++) {
        strncpy(m_config_RPI->current_floppy_disk_path[drv], m_config_native->current_floppy_disk_path[drv], _MAX_PATH - 1);
        m_config_RPI->current_floppy_disk_path[drv][_MAX_PATH - 1] = '\0'; // NULL終端を保証
    }
    
#if defined(USE_TAPE)
    for(int drv = 0; drv < USE_TAPE; drv++) {
        strncpy(m_config_RPI->current_tape_path[drv], m_config_native->current_tape_path[drv], _MAX_PATH - 1);
        m_config_RPI->current_tape_path[drv][_MAX_PATH - 1] = '\0'; // NULL終端を保証
    }
#endif

#if defined(USE_MONITOR_TYPE)
    m_config_RPI->monitor_type = m_config_native->monitor_type;
#endif

#if defined(USE_KEYBOARD_TYPE)
    m_config_RPI->keyboard_type = m_config_native->keyboard_type;
#endif

    m_config_RPI->fullscreen_stretch_type = m_config_native->fullscreen_stretch_type;

#if defined(USE_FLOPPY_DISK)
    m_config_RPI->sound_noise_fdd = m_config_native->sound_noise_fdd;
#endif

#if defined(USE_TAPE)
    m_config_RPI->sound_noise_cmt = m_config_native->sound_noise_cmt;
    m_config_RPI->sound_tape_signal = m_config_native->sound_tape_signal;
    m_config_RPI->sound_tape_voice = m_config_native->sound_tape_voice;
#endif

    for(int drv = 0; drv < USE_FLOPPY_DISK; drv++) {
        m_config_RPI->correct_disk_timing[drv] = m_config_native->correct_disk_timing[drv];
        m_config_RPI->ignore_disk_crc[drv] = m_config_native->ignore_disk_crc[drv];
    }

    for(int drv = 0; drv < USE_TAPE; drv++) {
        m_config_RPI->wave_shaper[drv] = m_config_native->wave_shaper[drv];
        m_config_RPI->baud_high[drv] = m_config_native->baud_high[drv];
    }
}
