//
// kernel.h
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
#ifndef _kernel_h
#define _kernel_h

#include <circle_stdlib_app.h>

#include <vc4/sound/vchiqsoundbasedevice.h>
#include <vc4/sound/vchiqsounddevice.h>
#include <vc4/vchiq/vchiqdevice.h>

// USBキーボード対応のためのインクルード
#include <circle/usb/usbkeyboard.h>
// USBゲームパッド対応のためのインクルード
#include <circle/usb/usbgamepad.h>


#include "vicesound.h"

class EmuController;

class CKernel : public CStdlibApp
{
private:
        char const *mpPartitionName;

public:
#define CSTDLIBAPP_LEGACY_DEFAULT_PARTITION "emmc1-1"
#define CSTDLIBAPP_DEFAULT_PARTITION "SD:"

	CKernel (const char *pPartitionName = CSTDLIBAPP_DEFAULT_PARTITION);

    virtual bool Initialize (void)
    {

            if (!CStdlibApp::Initialize ())
            {
                    return false;
            }

            if (!mSerial.Initialize (115200))
            {
                    return false;
            }

            CDevice *pTarget = &mSerial;
            if (!mLogger.Initialize (pTarget))
            {
                    return false;
            }

            mLogger.Write(GetKernelName(), LogNotice, "Timer initialize");
            if (!mTimer.Initialize ())
            {
                    return false;
            }

            mLogger.Write(GetKernelName(), LogNotice, "VCHIQ initialize");
            if (!mVCHIQ.Initialize())
            {
                    return false;
            }

            mLogger.Write(GetKernelName(), LogNotice, "EMMC initialize");
            if (!mEMMC.Initialize ())
            {
                    return false;
            }

            mLogger.Write(GetKernelName(), LogNotice, "FileSystem mount");
            char const *partitionName = mpPartitionName;

            // Recognize the old default partition name
            if (strcmp(partitionName, CSTDLIBAPP_LEGACY_DEFAULT_PARTITION) == 0)
            {
                    partitionName = CSTDLIBAPP_DEFAULT_PARTITION;
            }

            if (f_mount (&mFileSystem, partitionName, 1) != FR_OK)
            {
                    mLogger.Write (GetKernelName (), LogError,
                                     "Cannot mount partition: %s", partitionName);

                    return false;
            }

            mLogger.Write(GetKernelName(), LogNotice, "USBHCI initialize");
            if (!mUSBHCI.Initialize ())
            {
                    return false;
            }

            // if (!mConsole.Initialize ())
            // {
            //         return false;
            // }


			return true;
    }

        virtual void Cleanup (void)
        {
                f_mount(0, "", 0);

                CStdlibApp::Cleanup ();
        }

        const char *GetPartitionName() const
        {
                return mpPartitionName;
        }


	TShutdownMode Run (void);
	
	// キーボードイベントハンドラ
	static void KeyStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6], void* arg);

	// ゲームパッドイベントハンドラ
	static void GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState);

protected:
        CSerialDevice   mSerial;
        CTimer          mTimer;
        CLogger         mLogger;
        CUSBHCIDevice   mUSBHCI;
        CEMMCDevice     mEMMC;
        FATFS           mFileSystem;
        CScheduler      mScheduler;
        CVCHIQDevice    mVCHIQ;
        ViceSound       mSound;
        
        // キーボード入力用の変数
        CUSBKeyboardDevice* mKeyboard;

        // ゲームパッド入力用の変数
        CUSBGamePadDevice* mGamePad;

        EmuController* mEmuController;

        // CConsole        mConsole;
};

#endif

