# stackchan-arduino-ex

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
  軸ごとに31種類のイージングタイプを指定できます（デフォルト: `QuadInOut`）。

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
examples/arduino_easing_demo/ ボタンAでイージングパターンを1つずつ確認できるデモ (M5Unified使用)
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

31種類すべてのイージングパターンを1つずつ見比べたい場合は
[examples/arduino_easing_demo](examples/arduino_easing_demo) を使ってください。
CoreS3のボタンAを押すたびに次のパターンに切り替わり、axis:xが
lower_limitとupper_limitの間を往復し、画面中央に現在のパターン名が表示されます。

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

## stackchan-arduino からの主な変更点

- `SCEX_BasicConfig.yaml` の `servo` セクションが `pin.x`/`pin.y` 決め打ちから
  `servo.axes` リストに変更されています（[docs/api.md](docs/api.md) 参照）。
- `StackchanSERVO`/`StackchanSystemConfig` は `SCEX::ServoManager`/`SCEX::SystemConfig` に
  置き換わり、ファイル読み込みは `fs::FS&` ではなくパス文字列（`fopen`）になりました。
- `DYN_XL330`/`RT_DYN_XL330`（Dynamixel）はこのバージョンでは未移植です
  （理由は [docs/porting_notes.md](docs/porting_notes.md)）。
