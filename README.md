# RPiEmuLib

RPiEmuLibは、Raspberry Pi向けのベアメタル開発環境であるCircleを使用して構築された汎用エミュレーターフレームワークライブラリです。

## 概要

RPiEmuLibは、Raspberry Pi上でエミュレーターを簡単に開発するためのフレームワークを提供します。このライブラリはCircleというRaspberry Pi用のベアメタル環境をベースにしており、キーボード入力、ゲームパッド入力、音声出力、グラフィック表示などのエミュレーターに必要な機能を容易に利用できるようにします。

## ビルド方法

### 必要なもの

- AArch64対応のクロスコンパイラ（例：aarch64-none-elf-） ※32bitでも動くとは思いますが未確認です
- GNUツールチェーン（make, git など）

推奨ツールチェーン：
- [Arm GNU Toolchain Downloads]( https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads )から入手できる gcc 13.2.Rel1
  - 13.2.Rel1 で動作確認をしています(14系ではビルド出来ない断を確認しています)
  - 各プラットフォームの AArch64 bare-metal target（aarch64-none-elf）※現在Macでのビルドのみ確認しています

- Macでビルドする場合は gnu-getopt が必要

Macでの gnu-getopt インストール方法:
```
brew install gnu-getopt
echo 'export PATH="/opt/homebrew/opt/gnu-getopt/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

※ Intel系の場合はパスを調整してください(多分 '/usr/local/opt/gnu-getopt/bin' ？)

### ビルド手順

1. リポジトリをクローンします:
   ```
   git clone https://github.com/h-o-soft/RPiEmuLib.git
   cd RPiEmuLib
   ```

2. Makefileを編集します:

    Makefile を編集し、 RPIの項目について、 Raspberry Piのモデル(Zero 2と3系は「3」、4は「4」)を設定します。

    ```
    RPI = 4
    ```

   エミュレーターについては[ビルド対象エミュレータの設定](#ビルド対象エミュレータの設定)の項目を参照し、設定します。
 
3. ビルドを実行します:

   ```
   make
   ```

   これによりcircle-stdlibのビルドが行われた後に、RPiEmuLibのビルドが行われます。

   ビルドされたものは `image` フォルダにコピーされるので、このimageフォルダをSDカードのルートに全てコピーする事で起動します。

4. Raspberry Piのモデルを切り替えるには

   ```
   make allclean
   ```

   上記にて初期状態に戻したあと、Makefileを編集し、手順2から実行してください。

5. より高度な例として、Common Source Code Project (以下、CSCP) の、Mac / Linux でビルド可能なソースを組み合わせてビルドすることも可能です。

   Mac / Linuxで動作するCSCPのソースコードを本プロジェクトにマージし、Makefile の `TARGET` に `x1` などを指定することでビルドを試みることができますが、これはあくまで使用例であり、動作を保証するものではありません。

   CSCP のライセンスは GPLv2 です。本プロジェクト (RPiEmuLib, GPLv3) と組み合わせた場合、生成されるバイナリ全体には GPLv3 が適用されることになります。CSCP が『GPLv2 only』ではなく『GPLv2 or Later』であればこの組み合わせはライセンス上問題ありませんが、現在著作者と連絡が取れないため、CSCP 側の正確な意向を確認することは困難です。

   CSCP との組み合わせを行う場合は、ご自身の責任において、関連するライセンス (GPLv2 および GPLv3) の条項を十分に理解・遵守してください。CSCP の正確なライセンス意向が不明であるため、本プロジェクトとしては、組み合わせたバイナリの再配布は推奨せず、個人での利用にとどめていただくことを強くお勧めします。ライセンスに関する一般的な注意点については、[注意](#注意)のセクションも参照してください。


## RpiEmuLibの概要

### 全体概要

RPiEmuLibは以下のコンポーネントで構成されています：

- **エントリポイント**: `main.cpp`がエントリポイントとなり、`kernel.cpp / kernel.h`を使ってカーネルが動作します。
- **エミュレーターコントローラー**: `EmuController`クラスの`emulator_main`関数がエミュのメインループとなります。
- **ユーザーレベルインターフェース**: `emu.cpp`のコンストラクタで初期化し、`run`メソッドで1フレームぶんの処理を行い、`vm.cpp`の`draw_screen`メソッドでRGB565のフレームバッファに画面を描画します。
- **主ライブラリ**: 一般的なエミュレーターで使われると思われる描画や入力、サウンドなどの実処理を行う、Circleを使った実装クラスとして「Platform*」クラスが存在します。基本的にはemu.cpp、vm.cppを編集するのみでエミュレータの実装と動作が出来ますが、細かい動作を変えたい場合はこの中身を変更してください。

### ビルド対象エミュレータの設定

- `Makefile`の`TARGET`にエミュレーターの名前を指定します。
- ファイル `Makefile.(TARGET名)`がビルド対象のエミュレーターの実際のMakefileなので、`Makefile.stub`をコピーして独自に作成します。
- `src/rpi/emu.cpp`と`src/rpi/emu.h`および`src/rpi/vm.cpp`と`src/rpi/vm.h`をコピーし、独自で実装します(そのまま編集しても構いません。コピーした場合は、 `Makefile.(TARGET名称)` の中のファイル名も書き換えていただくようお願いします)
- サンプルとして`Makefile.emu6502`があります。これは6502を使った簡素なエミュレータ風の動作をするサンプルです。アドレス0x2000〜0x3fffが1ドット1ビット(256x256)の二値のVRAMになっているので、 vm6502.cpp の「prog」にある(0xc000から配置される想定の)6502のバイナリを書き換えると、それらしく動くと思われます(多分……)。

### 時間調整

- `vm.h`に以下の設定で速度関連の値を設定します：

```cpp
// 4MHz
#define CPU_CLOCKS     4000000

// 一秒あたりのフレーム数
#define FRAMES_PER_SEC  60.0
```

- 実処理は`EmuController`の`emulator_main`のループで処理しています。

### 描画

- エミュレーター自体の解像度は`PlatformEmu.h`で設定します：
  - `SCREEN_WIDTH`と`SCREEN_HEIGHT`でエミュレーターの画面サイズ(いわゆるVRAMで表現されるサイズ)を設定します。
  - `WIDTH_ASPECT`と`HEIGHT_ASPECT`で、実際に表示されるアスペクト比を設定します。
  - `PlatformCommon.h`の`DISPLAY_WIDTH`と`DISPLAY_HEIGHT`にデバイスの解像度を設定します。
  
- 描画は`vm.cpp`の`draw_screen`メソッドで行います：
  - `g_platform_screen->get_screen_width()`と`g_platform_screen->get_screen_height()`でエミュレーターの画面サイズを取得できます。
  - `g_platform_screen->get_screen_buffer(y)`で画面のバッファの指定Y座標位置のアドレスを取得できます。
  - 画面のバッファはRGB565のデータなので、エミュレーターで描画した情報をこのバッファに書き込むと、最終的にストレッチなどが行われ画面に描画されます。

### 入力（キーボード、ゲームパッド）

- キーボード入力の実処理は`kernel.cpp`で行われます。
- キーデータはWindowsのVK_互換のコードに変換され、`emu.cpp`の`key_down`、`key_up`にイベントが届き、最終的に`vm.cpp`の`key_down`、`key_up`が呼ばれます。
- ジョイスティックについては`uint32_t rpi_gamepad_status[4]`というグローバル配列に入力状態が格納されます(手抜きです)。下位ビットから、方向の上、下、左、右、Aボタン、Bボタンとなっています。

#### ジョイスティック設定(config.sys)

SD カードのルートディレクトリに `config.sys` ファイルを配置することでジョイスティックのカスタマイズができます。

- この機能はSUZUKI PLAN氏の[VGS-Zero](https://github.com/suzukiplan/vgszero)と同機能となります。
- 方向キーとA、Bボタンのみの対応となります。

##### Joypad Button Assign

```
#--------------------
# JoyPad settings
#--------------------
A BUTTON_1
B BUTTON_0
SELECT BUTTON_8
START BUTTON_9
UP AXIS_1 < 64
DOWN AXIS_1 > 192
LEFT AXIS_0 < 64
RIGHT AXIS_0 > 192
```

（凡例）

```
# ボタン設定
key_name △ BUTTON_{0-31}

# AXIS設定
key_name △ AXIS_{0-1} △ {<|>} △ {0-255}
```

`key_name`:

- `A` Aボタン
- `B` Bボタン
- `START` STARTボタン
- `SELECT` SELECTボタン
- `UP` 上カーソル
- `DOWN` 下カーソル
- `LEFT` 左カーソル
- `RIGHT` 右カーソル

カーソルに `BUTTON_` を割り当てたり、ボタンに `AXIS_` を割り当てることもできます。

### オーディオ

- `emu.cpp`のコンストラクタで、`sound_rate`と`sound_samples`を設定します。
- `sound_rate`は44.1kHzの場合44100で、`sound_samples`は一度に作るサンプル数で、通常は`sound_rate/20`の値になっています(ここをいじる場合は `ViceSound.h` の各種サイズも編集しないと正しく動作しない可能性があります)
- `vm.cpp`の`create_sound`メソッドで`sound_samples`ぶんのサンプルを生成し、戻り値として返します。
- フォーマットは16ビットのステレオなので、`sound_samples * 2 * 2`バイトのデータが必要になります。
- Raspberry Pi 3系(Zero 2含む)では速度的に厳しい事があるので、22.050kHzで音が作られる想定で実装されています。これは `RPI_MODEL` の定義が3の場合にそのようになる実装になっているので、grepなどで探し、必要に応じて調整してください。

### 設定、エミュレーターメニュー

- `menu.cfg`ファイルを編集する事で汎用的なメニューを作成できます。このファイルはSDカードのルートにコピーします( `menu.cfg.(TARGET名)` というファイルをプロジェクトルートに配置すると、make完了時に imageフォルダにコピーされます )
- 設定データの定義は`PlatformConfig.h`にある`platform_config_t`構造体で定義されています。
- `menu.cfg`に記載されている`config`の文字列を構造体のデータに相互変換するクラスが`PlatformConfig`です。新規で設定を作る場合はここに追加します。
- Raspberry Piでの実行時にF12キーを押すとメニューを表示できます。
- メニューを閉じると`PlatformEmu.h`の`CONFIG_NAME`で指定した名前に`.ini`を付けたファイルを書き込みます。

#### menu.cfgの例(抜粋)

```
"Drive/Save Disk Changes", action, "SaveDiskChanges"
"Drive/FD0/Insert", file, "current_floppy_disk_path[0]"
"Display/Screen Dot By Dot", int_select, "display_stretch[0]"
"Display/Screen Stretch", int_select, "display_stretch[1]"
"Display/Screen Aspect", int_select, "display_stretch[2]"
"Display/Screen Fill", int_select, "display_stretch[3]"
```

上記のようになっている場合「Drive」「Display」という項目がまず表示され、「Drive」を選択すると「Save Disk Changes」という項目と「FD0」という項目が表示されます。

「Save Disk Changes」を選択すると、これは「action」タイプなので「SaveDiskChanges」アクションが実行されます。

そして「FD0」を選択後に「Insert」を選ぶと、これは「file」タイプなので、ファイルセレクタが起動した後に、configの「current_floppy_disk_path[0]」に該当ファイルパスが格納され、更にフロッピー挿入メソッドが呼ばれます(EmuController の insert_floppy_disk メソッド)。このあたりは若干決め打ちで実装されています。

「Display」の項目については、「int_select」というタイプになっており、これは「display_stretch」というintの変数の値が括弧内の数字と一致していたらその項目が選ばれている、という状態を表すためのもので、例えば`display_stretch`が1の場合は「Screen Stretch」の項目にチェックが入り、それ以外のものはチェックがついていない、という状態になります(表示上は簡易的なチェックボックスが表示されます)。

このようにしてmenu.cfgについて自由に編集し、足りないものは各クラスを拡張する事で、比較的簡単にメニューの拡張が可能になっております(とはいえ、そこそこ面倒ですが……)。

## SDカードへの書き込みタイミング

RPiEmuLibで作られたエミュレーターあるいはそれに類するものはRaspberry Piのベアメタル環境で動作するため、基本的には好きな時に電源を切る事が出来ます。

ただし **SDカードへの書き込みがされているタイミングでの電源切断は避けるようお願いします。** SDカードへの書き込みのタイミングは下記のとおりです。

- 起動時(画面が表示される前)
- エミュレーターメニューを閉じた時
- GPIO26と3.3Vのピンを接触させた時(特殊リセット)
- (実装依存)エミュレーターメニュー内で「Save Disk Changes」または「Save Tape Changes」を実行(選択)した時
- (実装依存)エミュレーターメニューからフロッピーまたはテープをイジェクトした時(エミュレーター内で書き込みが行われた時のみ)

起動時と、エミュレーターメニューの開閉時は書き込みが行われていますので注意してください。

「実装依存」となっている部分は、一般的なエミュレーターの実装の場合、ディスクイメージへの読み書きはメモリに対して行われますが、必ずしもどのエミュレーターでもそのような動きにはならないという事を示します。

RPiEmuは、ディスク、テープへの読み書きはメモリに対して行われ、そのイジェクト(クローズ)時にストレージに書き戻される実装であると想定して作られているため、そのように作られているエミュレーターにおいては、上記タイミングでSDカードへの書き込みが行われる事になります。

逆に言うと、SDカードへの書き込みが行われる前、フロッピーなどに(エミュレーター内で)セーブを行ったとしても、それは一般的にはメモリに書き込むだけの挙動になっているため、そのまま電源を切ると **セーブデータが失われます。** それを避ける場合はシステムメニューから「Save Disk Changes」「Save Tape Changes」を明示的に実行すると、実際にSDカードに書かれるはずです(繰り返しますが、実装によります)。


## リファレンス

### EmuController

`EmuController`クラスはエミュレーターのメインコントローラーとして機能し、以下の重要なメソッドを提供します：

- `initialize`: エミュレーターの初期化を行います。
- `emulator_main`: エミュレーターのメインループを実行します。
- 各種入力処理メソッド：`key_down`, `key_up`, `press_button`など
- ディスク操作メソッド：`eject_floppy_disk`, `insert_floppy_disk`など
- テープ操作メソッド：`eject_tape`, `play_tape`など
- 設定操作メソッド：`save_config`, `load_config`など

### PlatformScreen

`PlatformScreen`クラスは画面表示を管理し、以下の機能を提供します：

- 画面バッファへのアクセス
- 画面サイズの取得と設定
- 描画バッファのストレッチやブリット（画像転送）
- フレームバッファの管理

### PlatformSound

`PlatformSound`クラスは音声出力を管理し、以下の機能を提供します：

- 音声デバイスの初期化と設定
- サンプルレートやバッファサイズの管理
- 音声データの出力

### PlatformConfig

`PlatformConfig`クラスは設定の管理を行い、以下の機能を提供します：

- 設定ファイルの読み込みと保存
- 設定値の取得と設定

### PlatformMenu

`PlatformMenu`クラスはエミュレーターのメニューシステムを提供し、以下の機能があります：

- メニュー項目の管理
- キー入力の処理
- ファイルブラウザの実装
- 設定の変更と適用

### ConfigConverter

`ConfigConverter`クラスは設定値の変換を行い、以下の機能を提供します：

- 設定名と値の相互変換
- 型変換の処理

## ライセンス

### 本プロジェクトのライセンス

RPiEmuLibはGPLv3の下で配布されています。

```This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```

→ [LICENSE](LICENSE.md)

### Circleのライセンス

CircleはGPLv3の下で配布されています。ライセンスの詳細は[Circle公式リポジトリ](https://github.com/rsta2/circle)を参照してください。

→ [LICENSE](license/LICENSE-Circle.txt)

### circle-stdlibのライセンス

circle-stdlibはGPLv3の下で配布されています。ライセンスの詳細は[circle-stdlib公式リポジトリ](https://github.com/smuehlst/circle-stdlib)を参照してください。

→ [LICENSE](license/LICENSE-CircleStdlib.txt)

### ViceSoundのライセンス

ViceSoundはGPLv3の下で配布されています。これは[BMC64プロジェクト](https://github.com/randyrossi/bmc64)からのコードです。

→ [LICENSE](license/LICENSE-ViceSound.txt)

### fake6502のライセンス

fake6502はパブリックドメインとして公開されています。オリジナルのソースコードには以下のライセンス表記があります：

```
LICENSE: This source code is released into the public domain, but
if you use it please do give credit. I put a lot of effort into
writing this!
```

→ [LICENSE](license/LICENSE-fake6502.txt)

### VGS-Zeroのライセンス

ジョイスティック設定処理については、SUZUKI PLAN氏の[VGS-Zero](https://github.com/suzukiplan/vgszero)のコードを使用しています。VGS-ZeroはMITライセンスで配布されています。

→ [LICENSE](license/LICENSE-VGS0.txt)

### Raspberry Piブートローダーとデバイスツリーファイルのライセンス

Raspberry Piのブートローダーとデバイスツリーファイルは[LICENSE.broadcom](license/LICENSE.broadcom)の下で配布されています。

→ [LICENSE](license/LICENSE.broadcom)


## 免責

本ソフトウェアは「現状のまま」で提供されます。商品性、特定目的への適合性、第三者の権利を侵害していないことなどを含め、明示的か黙示的かを問わず、いかなる保証もいたしません。

ソフトウェアの使用またはその他の取り扱いに関連して、契約上の責任、不法行為、その他いかなる法的根拠に基づくものであっても、作者または著作権者は、一切の請求、損害、その他の責任を負わないものとします。

## 謝辞

本プロジェクトは以下のプロジェクトの成果物を利用しています：

- [Circle](https://github.com/rsta2/circle) - Raspberry Pi向けのベアメタルC++環境
- [circle-stdlib](https://github.com/smuehlst/circle-stdlib) - CircleのためのC/C++標準ライブラリサポート
- [BMC64](https://github.com/randyrossi/bmc64) - Raspberry Pi向けのベアメタルCommodore 64エミュレータ
- [VGS-Zero](https://github.com/suzukiplan/vgszero) - Raspberry Pi向けのレトロゲームコンソールエミュレータ（ジョイスティック設定処理）
- [fake6502](http://rubbermallet.org/fake6502.c) - 6502 CPUエミュレータ

## 注意

- 本プロジェクトは GPLv3 でライセンスされています。他のライセンス、特に GPLv2 のみ (GPLv2 only) でライセンスされたプロジェクトと組み合わせる場合、ライセンスの互換性に注意が必要です。一般的に、GPLv3 のコードと GPLv2 のみのコードを組み合わせた成果物は、GPLv3 ライセンスが適用されます。
- ソフトウェアを組み合わせたり再配布したりする際は、ご自身の責任において、関連するすべてのライセンス条項を十分に理解し、遵守してください。不明な点がある場合は、ライセンスの専門家にご相談いただくことをお勧めします。
