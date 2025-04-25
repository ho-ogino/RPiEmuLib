
#ifndef _PLATFORM_INPUT_H_
#define _PLATFORM_INPUT_H_

#include <circle/types.h>

enum KeyEventType {
    KEY_EVENT_DOWN,
    KEY_EVENT_UP
};

// キーイベント構造体
struct PlatformKeyEvent {
    KeyEventType type;    // イベントタイプ (押された/離された)
    u8 code;              // 仮想キーコード
    bool extended;        // 拡張キーフラグ
    bool repeat;          // リピートフラグ
};

#endif
