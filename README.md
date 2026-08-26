# stackchan-arduino-ex

[English](README_en.md) | 日本語

[stackchan-arduino](https://github.com/stack-chan/stackchan-arduino) を参考にした、拡張APIを持つ Stack-chan サーボ/設定ライブラリです。

- **依存ゼロ**: ServoEasing / YAMLDuino / ESP32Servo / ArduinoJson / SCServo に依存しません。
  独自の Easing エンジン（`SCEX_Easing`）と独自の YAML パーサ（`SCEX_Yaml`）を実装し、
  サーボ・UART・I2C は ESP-IDF の `driver/ledc.h` / `driver/uart.h` / `driver/i2c_master.h` を直接使用します。
- **Arduino / ESP-IDF 両対応**: 同じソースが PlatformIO(`framework=arduino`) でも
  素の `idf.py` ビルドでもそのまま動きます（`arduino-esp32` はESP-IDFの上に構築されており、
  ESP-IDFのドライバAPIをArduinoスケッチから直接呼び出せることを利用しています）。
- **サーボ軸を増やしやすい構造**: `enum {AXIS_X, AXIS_Y}` の2軸決め打ちをやめ、
  `SCEX_BasicConfig.yaml` の `servo.axes` リストで軸を定義します。軸を増やすには
  yaml にエントリを1つ足すだけです。
- **easings.net 相当のイージング**: `ServoManager::setEasingType(axis, EasingType)` で
  軸ごとに25種類のイージングタイプを指定できます（デフォルト: `QuadInOut`）。

詳細は [docs/api.md](docs/api.md)（日本語）/ [docs/api_en.md](docs/api_en.md)（English）、
移植方針の詳細は [docs/porting_notes.md](docs/porting_notes.md) を参照してください。

## ディレクトリ構成

```
src/                   ライブラリ本体
  SCEX_Easing.*         独自Easingエンジン (easings.net相当)
  SCEX_Yaml.*           独自YAMLサブセットパーサ
  SCEX_Config.*         SCEX_BasicConfig.yaml 読み込み
  SCEX_ServoAxis.*      1軸ぶんの状態・イージング適用
  SCEX_ServoManager.*   可変長軸レジストリ + バックグラウンド補間タスク
  SCEX_ServoDriver*.*   サーボ種別ごとのドライバ (pwm / scs / m5_scs)
  SCEX_Motion.*         プリセットモーション
  SCEX_I2CBus.* / SCEX_IOExpander.*   m5_scs 用の電源制御
data/yaml/                    SCEX_BasicConfig.yaml など設定ファイルのサンプル
examples/arduino_basic/       PlatformIO (Arduino) サンプル
examples/arduino_easing_demo/ ボタンAで再生、ボタンCでパターンを選択するデモ (M5Unified使用)
examples/esp-idf_basic/       素のESP-IDF (idf.py) サンプル
test/                         Easing/YAMLパーサのホスト単体テスト (test/run_native_tests.sh)
```

## 使い方（PlatformIO / Arduino）

```ini
; platformio.ini
; SCEX_BasicConfig.yamlのデフォルトはm5_scs（M5StackChanのCoreS3/ESP32-S3向け）です。
; driver: pwm に切り替える場合は任意のESP32ボードで構いません。
[env:m5stack-cores3]
platform = espressif32
board = m5stack-cores3
framework = arduino
lib_deps = symlink://../path/to/stackchan-arduino-ex
```

```cpp
#include "StackchanServoEx.h"
using namespace SCEX;

SystemConfig config;
ServoManager servos;

void setup() {
    SPIFFS.begin(true);
    config.loadConfig("/spiffs/yaml/SCEX_BasicConfig.yaml");
    for (const auto& axis_cfg : config.servoAxes()) {
        servos.addAxis(axis_cfg, createServoDriver(axis_cfg.driver_type));
    }
    servos.begin();

    ServoAxisHandle x = servos.findAxis("x");
    servos.setEasingType(x, EasingType::BackInOut);  // ★イージングタイプの指定
    servos.moveTo(x, 45.0f, 1000);
}
```

完全な例は [examples/arduino_basic](examples/arduino_basic) を参照してください。

25種類すべてのイージングパターンを1つずつ見比べたい場合は
[examples/arduino_easing_demo](examples/arduino_easing_demo) を使ってください。
ボタンCで画面に表示するパターンを切り替え、ボタンAで現在表示されているパターンを実行します。
設定されたすべての軸がlower_limitとupper_limitの間を往復してstart_degreeに戻ります。
ボタンBを2秒間長押しすると、現在表示されているパターンから最後まで連続実行します。

## 使い方（素の ESP-IDF）

```
cd examples/esp-idf_basic
idf.py set-target esp32s3
idf.py build flash monitor
```

ESP-IDF v5.5 / v6.0 でビルド確認済みです。詳細は
[examples/esp-idf_basic](examples/esp-idf_basic) を参照してください。

## テスト

```
test/run_native_tests.sh
```

ハードウェアに依存しない `SCEX_Easing` / `SCEX_Yaml` をホストの g++ でビルド・実行します。

## ライセンス

本ライブラリは [MIT License](LICENSE) で公開されています。

Copyright (c) 2026 Takao Akaki

## 参考・謝辞

本ライブラリは、以下のプロジェクトおよび資料の設計、API、動作を参考にしています。
各ライブラリを依存関係としてリンクまたは同梱しているものではなく、
`SCEX_Easing` と `SCEX_Yaml` は本プロジェクト独自の実装です。

- [stackchan-arduino](https://github.com/stack-chan/stackchan-arduino) —
  Stack-chanのサーボ制御・設定APIと設定ファイル構成（MIT License）
- [ServoEasing](https://github.com/ArminJo/ServoEasing) —
  サーボ補間APIとイージング動作（GPL-3.0-or-later）
- [YAMLDuino](https://github.com/tobozo/YAMLDuino) —
  Arduino環境でYAML設定を読み込む設計（MIT License）
- [SCServo（mongonta0716 fork）](https://github.com/mongonta0716/SCServo) —
  Feetech SCS(CL)プロトコルのパケット形式とレジスタ構成（MIT License）
- [easings.net](https://easings.net/) —
  イージング曲線の名称、数式および挙動

## stackchan-arduino からの主な変更点

- `SCEX_BasicConfig.yaml` の `servo` セクションが `pin.x`/`pin.y` 決め打ちから
  `servo.axes` リストに変更されています（[docs/api.md](docs/api.md) 参照）。
- `StackchanSERVO`/`StackchanSystemConfig` は `SCEX::ServoManager`/`SCEX::SystemConfig` に
  置き換わり、ファイル読み込みは `fs::FS&` ではなくパス文字列（`fopen`）になりました。
- `DYN_XL330`/`RT_DYN_XL330`（Dynamixel）はこのバージョンでは未移植です
  （理由は [docs/porting_notes.md](docs/porting_notes.md)）。
