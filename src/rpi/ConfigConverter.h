#ifndef CONFIG_CONVERTER_H
#define CONFIG_CONVERTER_H

class platform_config_t;

#define CONFIG_RPI platform_config_t*

#include "PlatformCommon.h"

#ifdef USE_EXTERNAL_EMU
#include "../config.h"
#define CONFIG_NATIVE config_t*
#else
#include "PlatformEmu.h"
#define CONFIG_NATIVE platform_config_t*
#endif


class ConfigConverter
{
    private:
        CONFIG_RPI m_config_RPI;
        CONFIG_NATIVE m_config_native;
    public:
        ConfigConverter(CONFIG_RPI configRPI, CONFIG_NATIVE configNative);
        ~ConfigConverter(){}

        // Raspberry Pi の設定をネイティブの設定に変換
        void ConvertToNative();

        // ネイティブの設定を Raspberry Pi の設定に変換
        void ConvertFromNative();
};




#endif
