//
// kernel.cpp
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "kernel.h"
#include "config.hpp"  // SystemConfigurationクラスのヘッダをインクルード

#include "PlatformCommon.h"

#ifdef USE_EXTERNAL_EMU
#include "osd.h"
#include "../emu.h"
#else
#include "PlatformEmu.h"
#endif

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <circle/devicenameservice.h>


#include "PlatformCommon.h"
#include "PlatformInterface.h"
#include "PlatformScreen.h"
#include "PlatformSound.h"
#include "PlatformMenu.h"
#include "PlatformConfig.h"
#include "EmuController.h"


SystemConfiguration* config_; // SystemConfiguration変数を宣言

EmuController* pEmuController;

uint32_t rpi_gamepad_status[4];

// winmain.cppのemulator_main関数を外部関数として宣言
// CLoggerポインタを引数として追加
extern "C" int emulator_main(CLogger* logger, CTimer* timer);
extern "C" void setup_platform_sound(ViceSound* device, CScheduler* sched);
// OSDにロガーを渡すための関数を宣言
extern "C" void setup_platform_logger(CLogger* logger);


const char* create_emu_path(const char *format, ...)
{
    static char buffer[1024];
    va_list args;
    va_start(args, format);
    
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return (const char*)buffer;
}

CKernel::CKernel (const char *pPartitionName)
:	CStdlibApp ("RPiEmu"),
    mTimer (&mInterrupt),
    mLogger (mOptions.GetLogLevel (), &mTimer),
    mpPartitionName (pPartitionName),
    mUSBHCI (&mInterrupt, &mTimer, TRUE),
    mVCHIQ(CMemorySystem::Get(), &mInterrupt),
    mSound(&mVCHIQ, VCHIQSoundDestinationHDMI),
    mEMMC (&mInterrupt, &mTimer, &mActLED),
    mKeyboard(nullptr),
    mGamePad(nullptr)
{
	mActLED.Blink (5);	// show we are alive
}


// Windows仮想キーコードへの変換テーブル
// ※日本語キーボードを前提としているので注意
static const uint8_t usb_key_to_vk[] = {
    0,    0,    0,    0,   'A',  'B',  'C',  'D',  'E',  'F',  // 0-9
    'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',  'P',  // 10-19
    'Q',  'R',  'S',  'T',  'U',  'V',  'W',  'X',  'Y',  'Z',  // 20-29
    '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  // 30-39
    0x0D, 0x1B, 0x08, 0x09, 0x20, 0xBD, 0xDE, 0xC0, 0xDB, 0xDC, // 40-49 (Enter, Esc, BS, Tab, Space, -=, ^~, @`[JPN], [{[JPN], \|)
    0xDD, 0xBB, 0xBA, 0xC0, 0xBC, 0xBE, 0xBF, 0x14, 0x70, 0x71, // 50-59 (]}[JPN], ;+, :*, @`[US], ,<, .>, /?, CapsLock, F1, F2)
    0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, // 60-69 (F3-F12)
    0x2C, 0x91, 0x13, 0x2D, 0x24, 0x21, 0x2E, 0x23, 0x22, 0x27, // 70-79 (PrtScr, ScrollLock, Pause, Ins, Home, PgUp, Del, End, PgDn, Right)
    0x25, 0x28, 0x26, 0x00, 0x6F, 0x6A, 0x6D, 0x6B, 0x6C, 0x61, // 80-89 (Left, Down, Up, Reserved, NumLock, KP_/, KP_*, KP_-, KP_+, KP_Enter)
    0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x60, 0x6E, // 90-99 (KP_1-9, KP_0)
    0x6E, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 100-109
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 110-119
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 120-129
    0x00, 0x00, 0x00, 0x00, 0x00, 0xE2, 0x15, 0xDC             // 130-137 (135=\[JPN右側], 136=かな[JPN], 137=¥|[JPN])
};

CLogger*         g_pLogger = nullptr;
CLogger*         get_logger() { return g_pLogger; }

// USBキーボードからのRAWキー入力に対するハンドラ - キーイベントをバッファに追加するだけの軽量実装
void CKernel::KeyStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6], void* arg)
{
    static unsigned char s_PreviousRawKeys[6] = {0};
    static unsigned char s_ucPrevModifiers = 0;

    // モディファイアキーの変化を処理し、バッファに追加
    if (ucModifiers != s_ucPrevModifiers) {
        // 左Ctrl
        if ((ucModifiers & 0x01) && !(s_ucPrevModifiers & 0x01)) {
            pEmuController->push_key_event(KEY_EVENT_DOWN, 0x11, false, false); // VK_CONTROL
        } else if (!(ucModifiers & 0x01) && (s_ucPrevModifiers & 0x01)) {
            pEmuController->push_key_event(KEY_EVENT_UP, 0x11, false, false);
        }
        
        // 左Shift
        if ((ucModifiers & 0x02) && !(s_ucPrevModifiers & 0x02)) {
            pEmuController->push_key_event(KEY_EVENT_DOWN, 0xA0, false, false); // VK_LSHIFT
        } else if (!(ucModifiers & 0x02) && (s_ucPrevModifiers & 0x02)) {
            pEmuController->push_key_event(KEY_EVENT_UP, 0xA0, false, false);
        }
        
        // 左Alt
        if ((ucModifiers & 0x04) && !(s_ucPrevModifiers & 0x04)) {
            pEmuController->push_key_event(KEY_EVENT_DOWN, 0x12, false, false); // VK_MENU
        } else if (!(ucModifiers & 0x04) && (s_ucPrevModifiers & 0x04)) {
            pEmuController->push_key_event(KEY_EVENT_UP, 0x12, false, false);
        }
        
        // 右Ctrl
        if ((ucModifiers & 0x10) && !(s_ucPrevModifiers & 0x10)) {
            pEmuController->push_key_event(KEY_EVENT_DOWN, 0xA3, false, false); // VK_RCONTROL
        } else if (!(ucModifiers & 0x10) && (s_ucPrevModifiers & 0x10)) {
            pEmuController->push_key_event(KEY_EVENT_UP, 0xA3, false, false);
        }
        
        // 右Shift
        if ((ucModifiers & 0x20) && !(s_ucPrevModifiers & 0x20)) {
            pEmuController->push_key_event(KEY_EVENT_DOWN, 0xA1, false, false); // VK_RSHIFT
        } else if (!(ucModifiers & 0x20) && (s_ucPrevModifiers & 0x20)) {
            pEmuController->push_key_event(KEY_EVENT_UP, 0xA1, false, false);
        }
        
        // 右Alt (AltGr)
        if ((ucModifiers & 0x40) && !(s_ucPrevModifiers & 0x40)) {
            pEmuController->push_key_event(KEY_EVENT_DOWN, 0xA5, false, false); // VK_RMENU
        } else if (!(ucModifiers & 0x40) && (s_ucPrevModifiers & 0x40)) {
            pEmuController->push_key_event(KEY_EVENT_UP, 0xA5, false, false);
        }
        
        s_ucPrevModifiers = ucModifiers;
    }
    
    // 通常キーの状態変化を検出してバッファに追加
    for (int i = 0; i < 6; i++) {
        unsigned char Key = RawKeys[i];
        if (Key != 0) {
        	// pLogger->Write ("KERNEL", LogNotice, "KeyStatusHandlerRaw: Key=%d", Key);
            // このキーが前回のリストになければ、新たに押下されたと判断
            bool Found = false;
            for (int j = 0; j < 6; j++) {
                if (s_PreviousRawKeys[j] == Key) {
                    Found = true;
                    break;
                }
            }
            
            if (!Found) {
                // 新しく押されたキー
                if (Key < sizeof(usb_key_to_vk) / sizeof(usb_key_to_vk[0])) {
                    uint8_t vk = usb_key_to_vk[Key];
                    if (vk != 0) {
                        // pLogger->Write ("KERNEL", LogNotice, "KeyStatusHandlerRaw: USB Key=%d, VK=0x%02X", Key, vk);
                        pEmuController->push_key_event(KEY_EVENT_DOWN, vk, false, false);
                    }
                }
            }
        }
    }
    
    // 離されたキーを検出してバッファに追加
    for (int i = 0; i < 6; i++) {
        unsigned char Key = s_PreviousRawKeys[i];
        if (Key != 0) {
            // このキーが現在のリストになければ、離されたと判断
            bool Found = false;
            for (int j = 0; j < 6; j++) {
                if (RawKeys[j] == Key) {
                    Found = true;
                    break;
                }
            }
            
            if (!Found) {
                // 離されたキー
                if (Key < sizeof(usb_key_to_vk) / sizeof(usb_key_to_vk[0])) {
                    uint8_t vk = usb_key_to_vk[Key];
                    if (vk != 0) {
                        pEmuController->push_key_event(KEY_EVENT_UP, vk, false, false);
                    }
                }
            }
        }
    }
    
    // 現在の状態を保存
    memcpy(s_PreviousRawKeys, RawKeys, sizeof(s_PreviousRawKeys));
}

// ジョイスティック/ゲームパッド入力に対するハンドラ
void CKernel::GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState)
{
    // ジョイスティックの状態をエミュレータに伝える
    uint32_t joy_state = 0;
    
    // config_を使用して各ボタンの状態をチェック
    if (config_) {
        // 方向ボタンをチェック
        if (config_->buttonLeft->check(pState)) {
            joy_state |= 0x00000004;  // LEFT - ビット2
        }
        if (config_->buttonRight->check(pState)) {
            joy_state |= 0x00000008;  // RIGHT - ビット3
        }
        if (config_->buttonUp->check(pState)) {
            joy_state |= 0x00000001;  // UP - ビット0
        }
        if (config_->buttonDown->check(pState)) {
            joy_state |= 0x00000002;  // DOWN - ビット1
        }
        
        // アクションボタンをチェック
        if (config_->buttonA->check(pState)) {
            joy_state |= (1 << 4);  // A - ビット4
        }
        if (config_->buttonB->check(pState)) {
            joy_state |= (1 << 5);  // B - ビット5
        }
        
        // スタート・セレクトボタンをチェック
        if (config_->buttonStart->check(pState)) {
            joy_state |= (1 << 9);  // START - ビット9
        }
        if (config_->buttonSelect->check(pState)) {
            joy_state |= (1 << 8);  // SELECT - ビット8
        }
    } else {
        // config_がない場合は従来の方法で処理
        // デジタル方向ボタンの状態をチェック
        if (pState->buttons & GamePadButtonUp) {
            joy_state |= 0x00000001;  // UP - ビット0
        }
        if (pState->buttons & GamePadButtonDown) {
            joy_state |= 0x00000002;  // DOWN - ビット1
        }
        if (pState->buttons & GamePadButtonLeft) {
            joy_state |= 0x00000004;  // LEFT - ビット2
        }
        if (pState->buttons & GamePadButtonRight) {
            joy_state |= 0x00000008;  // RIGHT - ビット3
        }
        
        // アナログスティックの状態をチェック（閾値を超えたら方向入力とみなす）
        if (pState->naxes > 0) {
            if (pState->axes[0].value < 64) {    // X軸左方向
                joy_state |= 0x00000004;  // LEFT - ビット2
            }
            if (pState->axes[0].value > 192) {   // X軸右方向
                joy_state |= 0x00000008;  // RIGHT - ビット3
            }
        }
        
        if (pState->naxes > 1) {
            if (pState->axes[1].value < 64) {    // Y軸上方向
                joy_state |= 0x00000001;  // UP - ビット0
            }
            if (pState->axes[1].value > 192) {   // Y軸下方向
                joy_state |= 0x00000002;  // DOWN - ビット1
            }
        }
        
        // ボタンの状態をチェック
        if (pState->buttons & GamePadButtonA) {
            joy_state |= (1 << 4);  // A - ビット4
        }
        if (pState->buttons & GamePadButtonB) {
            joy_state |= (1 << 5);  // B - ビット5
        }
        if (pState->buttons & GamePadButtonStart) {
            joy_state |= (1 << 9);  // START - ビット9
        }
        if (pState->buttons & GamePadButtonSelect) {
            joy_state |= (1 << 8);  // SELECT - ビット8
        }
    }

    // ジョイスティックの状態を更新（エミュレータが後で参照するためのグローバル変数を更新）
    extern uint32_t rpi_gamepad_status[4];  // osd_input.cppで定義されている変数
    if (nDeviceIndex < 4) {
        // 念のため配列の0番目の要素にも設定する - 互換性のため
        rpi_gamepad_status[0] = joy_state;
        rpi_gamepad_status[nDeviceIndex] = joy_state;
    }
}

CStdlibApp::TShutdownMode CKernel::Run (void)
{
	mLogger.Write (GetKernelName (), LogNotice, "Computer Emulator for Raspberry Pi");
	mLogger.Write (GetKernelName (), LogNotice, "Starting emulator...");

	// ロガーを設定し、OSDクラスにロガーへのアクセスを提供
	g_pLogger = &mLogger;
	
	// サウンド周りの初期化
	mSound.SetControl(VCHIQ_SOUND_VOLUME_DEFAULT);  // サウンドボリュームをデフォルト値に設定
	
	// config.sys(ジョイスティック設定)の読み込み
	mLogger.Write(GetKernelName(), LogNotice, "Loading config.sys...");
	FILE* configFile = fopen("config.sys", "r");
	if (configFile) {
	    // ファイルサイズを取得
	    fseek(configFile, 0, SEEK_END);
	    long fileSize = ftell(configFile);
	    fseek(configFile, 0, SEEK_SET);
	    
	    // バッファを確保してファイルを読み込む
	    char* buffer = new char[fileSize + 1];
	    size_t readSize = fread(buffer, 1, fileSize, configFile);
	    buffer[readSize] = 0; // NULL終端
	    
	    // SystemConfigurationオブジェクトを作成
	    config_ = new SystemConfiguration(buffer);
	    
	    // バッファを解放
	    delete[] buffer;
	    fclose(configFile);
	    
	    mLogger.Write(GetKernelName(), LogNotice, "config.sys loaded successfully");
	} else {
	    // ファイルが存在しない場合は、デフォルト設定で初期化
	    config_ = new SystemConfiguration("");
	    mLogger.Write(GetKernelName(), LogNotice, "config.sys not found, using default settings");
	}

    // 各種Platform系インスタンスの生成
    pEmuController = mEmuController = new EmuController();
    mEmuController->initialize(DISPLAY_WIDTH, DISPLAY_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, WINDOW_WIDTH_ASPECT, WINDOW_HEIGHT_ASPECT, &mSound, &mScheduler);

	// USBデバイス検出処理
	if (mUSBHCI.UpdatePlugAndPlay()) {
		// キーボードデバイスを検索
		mKeyboard = static_cast<CUSBKeyboardDevice *>(mDeviceNameService.GetDevice("ukbd1", FALSE));
		
		if (mKeyboard != nullptr) {
			mLogger.Write (GetKernelName (), LogNotice, "USB keyboard found");
			
			// キーボードイベントハンドラを登録
			mKeyboard->RegisterKeyStatusHandlerRaw(KeyStatusHandlerRaw, TRUE, this);
			mLogger.Write (GetKernelName (), LogNotice, "Keyboard handler registered");
		} else {
			mLogger.Write (GetKernelName (), LogWarning, "No USB keyboard found");
		}

		// ゲームパッドデバイスを検索
		mGamePad = static_cast<CUSBGamePadDevice *>(mDeviceNameService.GetDevice("upad1", FALSE));
		
		if (mGamePad != nullptr) {
			mLogger.Write (GetKernelName (), LogNotice, "USB gamepad found");
			
			// ゲームパッドイベントハンドラを登録
			mGamePad->RegisterStatusHandler(GamePadStatusHandler);
			mLogger.Write (GetKernelName (), LogNotice, "Gamepad handler registered");
		} else {
			mLogger.Write (GetKernelName (), LogWarning, "No USB gamepad found");
		}
	}

    // load config
    mEmuController->load_config();

	int result = mEmuController->emulator_main();
	
	// 終了コードに応じた処理（エラー時の特別処理も可能）
	if (result != 0) {
		mLogger.Write (GetKernelName (), LogError, "Emulator exited with error code: %d", result);
	} else {
		mLogger.Write (GetKernelName (), LogNotice, "Emulator finished successfully");
	}
	
	// 設定を解放
	if (config_) {
	    delete config_;
	    config_ = nullptr;
	}

    delete mEmuController;

	return ShutdownHalt;
}
