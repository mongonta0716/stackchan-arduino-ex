// Loads SCEX_BasicConfig.yaml (+ optional secret/extend yaml files), the
// replacement for stackchan-arduino's StackchanSystemConfig. Uses
// SCEX_Yaml (not YAMLDuino) and plain filesystem paths + fopen (not
// fs::FS&) so this is usable unchanged from Arduino or ESP-IDF-native code
// -- mount SD/SPIFFS/LittleFS yourself first, same precondition the
// original library had.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SCEX_ServoTypes.h"
#include "SCEX_Yaml.h"

namespace SCEX {

// Reads the whole file at `path` into `out`. Returns false if it cannot be
// opened (out is left untouched).
bool readFileToString(const std::string& path, std::string* out);

struct ServoIntervalConfig {
    std::string mode_name;
    uint32_t interval_min = 0;
    uint32_t interval_max = 0;
    uint32_t move_min = 0;
    uint32_t move_max = 0;
};

struct BluetoothConfig {
    std::string device_name;
    bool starting_state = false;
    uint8_t start_volume = 100;
};

struct WifiConfig {
    std::string ssid;
    std::string password;
};

struct ApiKeysConfig {
    std::string stt;
    std::string ai_service;
    std::string tts;
};

struct SecretConfig {
    WifiConfig wifi;
    ApiKeysConfig api_key;
};

class SystemConfig {
public:
    virtual ~SystemConfig() = default;

    // basic_yaml_path not found -> falls back to setDefaultParameters() and
    // calls basicConfigNotFoundCallback(). secret/extend paths are skipped
    // entirely when empty.
    void loadConfig(const std::string& basic_yaml_path, const std::string& secret_yaml_path = "",
                     const std::string& extend_yaml_path = "");

    // Same as above, but tries each path in basic_yaml_candidates in order
    // and uses the first one that can be opened -- e.g. an SD card path
    // before a SPIFFS fallback, so a config on SD overrides the one baked
    // into the SPIFFS image without any code change. Still assumes the
    // caller has already mounted every filesystem a candidate path lives on
    // (SD.begin() / SPIFFS.begin() / etc.) -- this only tries fopen() on
    // each path in turn.
    void loadConfig(const std::vector<std::string>& basic_yaml_candidates,
                     const std::string& secret_yaml_path = "", const std::string& extend_yaml_path = "");

    void printAllParameters() const;

    const std::vector<ServoAxisConfig>& servoAxes() const { return servo_axes_; }
    // Returns nullptr if mode_name has no servo.speed entry.
    const ServoIntervalConfig* servoInterval(const std::string& mode_name) const;

    const BluetoothConfig& bluetooth() const { return bluetooth_; }
    const WifiConfig& wifi() const { return secret_.wifi; }
    const ApiKeysConfig& apiKeys() const { return secret_.api_key; }
    const SecretConfig& secret() const { return secret_; }

    size_t lyricsCount() const { return lyrics_.size(); }
    const std::string& lyric(size_t index) const { return lyrics_.at(index); }

    uint32_t autoPowerOffTime() const { return auto_power_off_time_; }
    const std::string& fontLanguage() const { return font_language_; }
    uint8_t ledLr() const { return led_lr_; }
    int ledPin() const { return led_pin_; }
    bool useTakaoBase() const { return takao_base_; }

    // Extension points, mirroring StackchanSystemConfig's virtual hooks so a
    // subclass can add application-specific settings the same way
    // StackchanExConfig did.
    virtual void loadExtendConfig(const std::string& yaml_path);
    virtual void setExtendSettings(const YamlValue& doc);
    virtual void printExtParameters() const;
    virtual void basicConfigNotFoundCallback();
    virtual void secretConfigNotFoundCallback();

protected:
    void setDefaultParameters();
    void setSystemConfig(const YamlValue& doc);
    void loadSecretConfig(const std::string& yaml_path);
    void setSecretConfig(const YamlValue& doc);
    void printSecretParameters() const;

    std::vector<ServoAxisConfig> servo_axes_;
    std::vector<ServoIntervalConfig> servo_intervals_;
    BluetoothConfig bluetooth_;
    uint32_t auto_power_off_time_ = 0;
    std::string font_language_ = "JA";
    std::vector<std::string> lyrics_;
    uint8_t led_lr_ = 0;
    int led_pin_ = -1;
    bool takao_base_ = false;
    SecretConfig secret_;
    bool secret_config_show_ = false;
};

}  // namespace SCEX
