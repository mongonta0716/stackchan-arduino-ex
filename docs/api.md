# stackchan-arduino-ex API ドキュメント

## 概要

`stackchan-arduino-ex` は [stackchan-arduino](https://github.com/stack-chan/stackchan-arduino)
を参考にした、サーボ制御・設定ファイル読み込みライブラリです。

- ServoEasing / YAMLDuino / ESP32Servo / ArduinoJson / SCServo に依存せず、
  すべて自前実装（`SCEX_Easing`, `SCEX_Yaml`）または ESP-IDF の
  `driver/ledc.h` / `driver/uart.h` / `driver/i2c_master.h` を直接使用します。
- Arduino（PlatformIO, `framework=arduino`）でも、素の `idf.py` ビルドでも
  同じソースがそのまま動きます（`examples/arduino_basic` と
  `examples/esp-idf_basic` を参照）。
- サーボ軸は `SCEX_BasicConfig.yaml` の `servo.axes` リストで定義される
  可変長構成です。X/Y の2軸決め打ちではありません。
- 詳しくは [porting_notes.md](porting_notes.md) （移植が難しかった/見送った依存ライブラリの一覧）も参照してください。

すべての公開シンボルは `namespace SCEX` の下にあります。

---

## クラス一覧

### 1. `SCEX::SystemConfig` (`SCEX_Config.h`)

`SCEX_BasicConfig.yaml` を読み込み、システム設定を管理するクラス
（旧 `StackchanSystemConfig` 相当）。

```cpp
void loadConfig(const std::string& basic_yaml_path,
                const std::string& secret_yaml_path = "",
                const std::string& extend_yaml_path = "");
```

- 引数はすべて**ファイルパス文字列**です（旧 `fs::FS&` は不要）。
  `fopen()` で読むため、Arduino では事前に `SD.begin()` / `SPIFFS.begin()`
  等でマウントしておいてください。ESP-IDF ネイティブでは
  `esp_vfs_spiffs_register()` 等でマウントします。
- `basic_yaml_path` が読めない場合はデフォルト値（PWM 2軸）にフォールバックし
  `basicConfigNotFoundCallback()` を呼びます。
- `secret_yaml_path` / `extend_yaml_path` は空文字なら読み込みをスキップします。

主なアクセサ:

| メソッド | 内容 |
|---|---|
| `servoAxes()` | `std::vector<ServoAxisConfig>`（yaml の `servo.axes` そのまま） |
| `servoInterval(mode_name)` | `servo.speed.<mode_name>` （見つからなければ `nullptr`） |
| `bluetooth()` / `wifi()` / `apiKeys()` / `secret()` | 各種設定 |
| `lyricsCount()` / `lyric(i)` | 吹き出しセリフ |
| `autoPowerOffTime()` / `fontLanguage()` / `ledLr()` / `ledPin()` / `useTakaoBase()` | その他設定 |

拡張ポイント（`StackchanExConfig` と同じパターン）:

```cpp
virtual void loadExtendConfig(const std::string& yaml_path);
virtual void setExtendSettings(const YamlValue& doc);
virtual void printExtParameters() const;
virtual void basicConfigNotFoundCallback();
virtual void secretConfigNotFoundCallback();
```

### 2. `SCEX::ServoManager` (`SCEX_ServoManager.h`)

可変長のサーボ軸を保持し、バックグラウンドの FreeRTOS タスクで
イージング補間を進める（旧 `StackchanSERVO` 相当）。

```cpp
ServoAxisHandle addAxis(const ServoAxisConfig& cfg, std::unique_ptr<ServoDriver> driver);
ServoAxisHandle findAxis(const std::string& name) const;
void begin(uint32_t tick_hz = 50);

void setEasingType(ServoAxisHandle axis, EasingType type);  // ★要望(2)のAPI
void setNativeTimedMove(ServoAxisHandle axis, bool on);     // シリアルサーボのみ

void moveTo(ServoAxisHandle axis, float degree, uint32_t duration_ms = 0,
            bool wait_for_completion = true);
void moveTo(ServoAxisHandle axis_a, float degree_a, ServoAxisHandle axis_b, float degree_b,
            uint32_t duration_ms, bool wait_for_completion = true);

void setTorque(ServoAxisHandle axis, bool on);
bool isMoving(ServoAxisHandle axis) const;
bool isMoving() const;  // いずれかの軸が動作中か
float currentDegree(ServoAxisHandle axis) const;
```

軸の追加方法（要望1: ServoAxis を増やしやすい構造）:

```cpp
for (const auto& axis_cfg : config.servoAxes()) {
    auto driver = createServoDriver(axis_cfg.driver_type);  // "pwm"/"scs"/"m5_scs"
    servos.addAxis(axis_cfg, std::move(driver));
}
servos.begin();
```

`SCEX_BasicConfig.yaml` の `servo.axes` にエントリを1つ追加するだけで
（例えば口/顎サーボの `jaw` 軸）、コード変更なしに軸が増えます。

### 3. `SCEX::EasingType` / `SCEX::ease()` (`SCEX_Easing.h`)

[easings.net](https://easings.net/) 相当の24種 + `Linear` = 25種類。

```
Linear
SineIn / SineOut / SineInOut
QuadIn / QuadOut / QuadInOut        <- デフォルト (quadraticEaseInOut相当)
CubicIn / CubicOut / CubicInOut
QuartIn / QuartOut / QuartInOut
QuintIn / QuintOut / QuintInOut
ExpoIn / ExpoOut / ExpoInOut
CircIn / CircOut / CircInOut
BackIn / BackOut / BackInOut
```

`ServoManager::setEasingType(axis, EasingType::BackInOut)` のように軸ごとに指定します。
未指定の場合は `kDefaultEasingType`（`QuadInOut`）が使われます。

yaml の `easing:` フィールドはスネークケース名（`quad_in_out`, `back_out` 等）で指定し、
`easingTypeFromName()` で変換されます。

#### ネイティブタイムドムーブ（カクつき対策 / Feetech SCS のみ）

移動量が小さく移動時間が長い場合、20ms ティックごとの書き込み量がサーボの位置分解能
（SCS0009 は約 0.29°）を下回り、「止まる→1段ジャンプ」を繰り返してカクカクします。

`native_timed_move`（デフォルト有効）を使うと、`ServoAxis` は移動をイージング曲線上の
数点（約 150ms ごと、最大 16 分割）のウェイポイントに分割し、各区間を SCS の
「ゴール位置 + ゴール時間」機能でサーボ自身に補間させます。サーボは区間内を内部レートで
細かく等間隔に刻むため、ガタつきが目立たなくなり、冗長なバス書き込みも消えます。

- PWM 軸では無視されます（`ServoDriver::supportsTimedMove()` が `false`）。
- yaml: `servo.axes[].native_timed_move: true|false`、実行時: `setNativeTimedMove(axis, on)`。
- 曲線を厳密に再現したい場合や、区間分割による多角形近似が気になる場合は `false` にすると
  従来のティック補間に戻ります。

### 4. `SCEX::ServoDriver` (`SCEX_ServoDriver.h`) / `createServoDriver()`

新しいサーボ種別を追加する際の拡張ポイント。以下を実装するだけで
`ServoAxis` / `ServoManager` 側は無改修で対応できます。

```cpp
class ServoDriver {
    virtual bool attach(const ServoAxisConfig& cfg) = 0;
    virtual void writeAngle(float degree) = 0;
    virtual float readAngle() { return NAN; }
    virtual void setTorque(bool on) {}
    // ファーム側で時間指定補間できるサーボ（Feetech SCS）は以下を override する。
    // 既定は false = ServoAxis がティックごとに writeAngle() を呼ぶ。
    virtual bool supportsTimedMove() const { return false; }
    virtual void writeTimedMove(float degree, uint32_t duration_ms) { writeAngle(degree); }
};
```

v1 で用意しているドライバ: `pwm`（`driver/ledc.h`）, `scs` / `m5_scs`
（`driver/uart.h` による Feetech SCS プロトコル直叩き）。
`createServoDriver(driver_type)` が文字列からインスタンスを生成します。

### 5. `SCEX::playMotion()` (`SCEX_Motion.h`)

旧 `StackchanSERVO::motion()` のプリセット（`greet`/`laugh`/`nod`/`refuse`/`test`）を移植。

```cpp
void playMotion(ServoManager& manager, MotionPreset preset,
                 const std::string& axis_x_name = "x", const std::string& axis_y_name = "y");
```

### 6. `SCEX::YamlValue` / `SCEX::YamlParser` (`SCEX_Yaml.h`)

独自の依存なし YAML サブセットパーサ。対応範囲はヘッダコメント参照。

```cpp
YamlValue root;
std::string error;
YamlParser::parse(text, &root, &error);
root["servo"]["axes"][0]["name"].asString();
```

---

## SCEX_BasicConfig.yaml

`data/yaml/SCEX_BasicConfig.yaml` を参照。主な変更点（旧 `SC_BasicConfig.yaml` との差分）:

- `servo.pin.x`/`servo.pin.y` のような2軸決め打ちの構造を廃止し、
  `servo.axes` のリストに変更（各要素が `name`/`driver`/`pin_tx`/`pin_rx`/
  `servo_id`/`offset`/`start_degree`/`lower_limit`/`upper_limit`/`easing` を持つ）。
- `servo.baud` は全シリアルサーボ共通の通信速度。省略時は `1000000`。
- `extend_config_filesize`/`secret_config_filesize` は廃止（独自 YAML
  パーサはファイル全体を読むため、ArduinoJson の固定バッファサイズ指定が
  不要になったため）。

---

## 使用例

```cpp
#include "StackchanServoEx.h"
using namespace SCEX;

SystemConfig config;
ServoManager servos;

void setup() {
    config.loadConfig("/spiffs/yaml/SCEX_BasicConfig.yaml");
    for (const auto& axis_cfg : config.servoAxes()) {
        servos.addAxis(axis_cfg, createServoDriver(axis_cfg.driver_type));
    }
    servos.begin();

    ServoAxisHandle x = servos.findAxis("x");
    servos.setEasingType(x, EasingType::BackInOut);
    servos.moveTo(x, 45.0f, 1000);
}
```

完全な例は `examples/arduino_basic`（PlatformIO）と `examples/esp-idf_basic`（idf.py）を参照してください。
