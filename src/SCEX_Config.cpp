#include "SCEX_Config.h"

#include <cstdio>

#include "esp_log.h"

namespace SCEX {

namespace {
constexpr char kTag[] = "SCEX_Config";
}  // namespace

bool readFileToString(const std::string& path, std::string* out) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    std::string contents;
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), file)) > 0) {
        contents.append(buf, n);
    }
    std::fclose(file);
    *out = std::move(contents);
    return true;
}

void SystemConfig::loadConfig(const std::string& basic_yaml_path, const std::string& secret_yaml_path,
                               const std::string& extend_yaml_path) {
    loadConfig(std::vector<std::string>{basic_yaml_path}, secret_yaml_path, extend_yaml_path);
}

void SystemConfig::loadConfig(const std::vector<std::string>& basic_yaml_candidates,
                               const std::string& secret_yaml_path, const std::string& extend_yaml_path) {
    std::string text;
    std::string found_path;
    for (const std::string& candidate : basic_yaml_candidates) {
        if (readFileToString(candidate, &text)) {
            found_path = candidate;
            break;
        }
    }

    if (!found_path.empty()) {
        YamlValue doc;
        std::string error;
        if (!YamlParser::parse(text, &doc, &error)) {
            ESP_LOGE(kTag, "yaml parse error in %s: %s", found_path.c_str(), error.c_str());
            setDefaultParameters();
        } else {
            ESP_LOGI(kTag, "loaded config from %s", found_path.c_str());
            setSystemConfig(doc);
        }
    } else {
        ESP_LOGI(kTag, "no config found (tried %zu path(s)), using default parameters",
                 basic_yaml_candidates.size());
        setDefaultParameters();
        basicConfigNotFoundCallback();
    }

    if (!secret_yaml_path.empty()) {
        loadSecretConfig(secret_yaml_path);
    }
    if (!extend_yaml_path.empty()) {
        loadExtendConfig(extend_yaml_path);
    }
    printAllParameters();
}

void SystemConfig::setDefaultParameters() {
    // Used only when basic_yaml_path can't be read. A generic two-axis PWM
    // setup (ESP32 PortA-style pins); real deployments should always ship a
    // SCEX_BasicConfig.yaml rather than rely on this fallback.
    servo_axes_.clear();
    ServoAxisConfig x;
    x.name = "x";
    x.driver_type = DriverType::kPwm;
    x.pin_tx = 22;
    x.lower_limit = 0;
    x.upper_limit = 180;
    x.start_degree = 90;
    servo_axes_.push_back(x);

    ServoAxisConfig y;
    y.name = "y";
    y.driver_type = DriverType::kPwm;
    y.pin_tx = 21;
    y.lower_limit = 50;
    y.upper_limit = 90;
    y.start_degree = 90;
    servo_axes_.push_back(y);

    servo_intervals_.clear();
    servo_intervals_.push_back({"normal_mode", 5000, 10000, 500, 1500});
    servo_intervals_.push_back({"sing_mode", 1000, 2000, 500, 1500});

    bluetooth_.device_name = "M5Stack_BTSPK";
    bluetooth_.starting_state = true;
    bluetooth_.start_volume = 150;

    auto_power_off_time_ = 0;
    font_language_ = "JA";
    lyrics_ = {"こんにちは", "Hello", "你好", "Bonjour"};
    led_lr_ = 0;
    led_pin_ = -1;
    takao_base_ = false;
    secret_config_show_ = false;
}

void SystemConfig::setSystemConfig(const YamlValue& doc) {
    const YamlValue& servo = doc["servo"];

    servo_axes_.clear();
    const YamlValue& axes = servo["axes"];
    for (size_t i = 0; i < axes.size(); i++) {
        const YamlValue& item = axes[i];
        ServoAxisConfig axis;
        axis.name = item["name"].asString();
        axis.driver_type = item["driver"].asString(DriverType::kPwm);
        axis.pin_tx = static_cast<int>(item["pin_tx"].asInt(-1));
        axis.pin_rx = static_cast<int>(item["pin_rx"].asInt(-1));
        axis.servo_id = static_cast<int>(item["servo_id"].asInt(static_cast<long>(i) + 1));
        axis.offset = static_cast<int16_t>(item["offset"].asInt(0));
        axis.lower_limit = static_cast<int16_t>(item["lower_limit"].asInt(0));
        axis.upper_limit = static_cast<int16_t>(item["upper_limit"].asInt(180));
        axis.start_degree = static_cast<int16_t>(item["start_degree"].asInt(90));
        axis.easing = easingTypeFromName(item["easing"].asString("quad_in_out").c_str());
        axis.use_io_expander = (axis.driver_type == DriverType::kM5Scs);
        axis.io_expander_addr = static_cast<uint8_t>(item["io_expander_addr"].asInt(0x6F));
        axis.i2c_sda = static_cast<int>(item["i2c_sda"].asInt(-1));
        axis.i2c_scl = static_cast<int>(item["i2c_scl"].asInt(-1));
        servo_axes_.push_back(axis);
        if (axis.name.empty()) {
            ESP_LOGE(kTag, "servo.axes[%zu] is missing a 'name'", i);
        }
    }

    servo_intervals_.clear();
    const YamlValue& speed = servo["speed"];
    for (const auto& kv : speed.entries()) {
        ServoIntervalConfig interval;
        interval.mode_name = kv.first;
        interval.interval_min = static_cast<uint32_t>(kv.second["interval_min"].asInt(0));
        interval.interval_max = static_cast<uint32_t>(kv.second["interval_max"].asInt(0));
        interval.move_min = static_cast<uint32_t>(kv.second["move_min"].asInt(0));
        interval.move_max = static_cast<uint32_t>(kv.second["move_max"].asInt(0));
        servo_intervals_.push_back(interval);
    }

    bluetooth_.device_name = doc["bluetooth"]["device_name"].asString();
    bluetooth_.starting_state = doc["bluetooth"]["starting_state"].asBool();
    bluetooth_.start_volume = static_cast<uint8_t>(doc["bluetooth"]["start_volume"].asInt(100));

    auto_power_off_time_ = static_cast<uint32_t>(doc["auto_power_off_time"].asInt(0));
    font_language_ = doc["balloon"]["font_language"].asString("JA");

    lyrics_.clear();
    const YamlValue& lyrics = doc["balloon"]["lyrics"];
    for (size_t i = 0; i < lyrics.size(); i++) {
        lyrics_.push_back(lyrics[i].asString());
    }

    led_lr_ = static_cast<uint8_t>(doc["led_lr"].asInt(0));
    led_pin_ = static_cast<int>(doc["led_pin"].asInt(-1));
    takao_base_ = doc["takao_base"].asBool(false);
    secret_config_show_ = doc["secret_config_show"].asBool(false);
}

void SystemConfig::loadSecretConfig(const std::string& yaml_path) {
    std::string text;
    if (!readFileToString(yaml_path, &text)) {
        secretConfigNotFoundCallback();
        return;
    }
    YamlValue doc;
    std::string error;
    if (!YamlParser::parse(text, &doc, &error)) {
        ESP_LOGE(kTag, "yaml parse error in %s: %s", yaml_path.c_str(), error.c_str());
        return;
    }
    setSecretConfig(doc);
    if (secret_config_show_) {
        ESP_LOGI(kTag, "=== secret config (do not share this output) ===");
        printSecretParameters();
        ESP_LOGI(kTag, "=== end of secret config ===");
    }
}

void SystemConfig::setSecretConfig(const YamlValue& doc) {
    secret_.wifi.ssid = doc["wifi"]["ssid"].asString();
    secret_.wifi.password = doc["wifi"]["password"].asString();
    secret_.api_key.stt = doc["apikey"]["stt"].asString();
    secret_.api_key.ai_service = doc["apikey"]["aiservice"].asString();
    secret_.api_key.tts = doc["apikey"]["tts"].asString();
}

const ServoIntervalConfig* SystemConfig::servoInterval(const std::string& mode_name) const {
    for (const auto& interval : servo_intervals_) {
        if (interval.mode_name == mode_name) {
            return &interval;
        }
    }
    return nullptr;
}

void SystemConfig::printAllParameters() const {
    for (const auto& axis : servo_axes_) {
        ESP_LOGI(kTag, "axis '%s': driver=%s pin_tx=%d pin_rx=%d offset=%d limits=[%d,%d] start=%d",
                 axis.name.c_str(), axis.driver_type.c_str(), axis.pin_tx, axis.pin_rx, axis.offset,
                 axis.lower_limit, axis.upper_limit, axis.start_degree);
    }
    for (const auto& interval : servo_intervals_) {
        ESP_LOGI(kTag, "speed '%s': interval=[%u,%u] move=[%u,%u]", interval.mode_name.c_str(),
                 interval.interval_min, interval.interval_max, interval.move_min, interval.move_max);
    }
    ESP_LOGI(kTag, "bluetooth: name=%s starting=%d volume=%u", bluetooth_.device_name.c_str(),
             bluetooth_.starting_state, bluetooth_.start_volume);
    ESP_LOGI(kTag, "auto_power_off_time=%u font_language=%s led_lr=%u led_pin=%d takao_base=%d",
             auto_power_off_time_, font_language_.c_str(), led_lr_, led_pin_, takao_base_);
    for (size_t i = 0; i < lyrics_.size(); i++) {
        ESP_LOGI(kTag, "lyric[%zu]=%s", i, lyrics_[i].c_str());
    }
    printExtParameters();
}

void SystemConfig::printSecretParameters() const {
    ESP_LOGI(kTag, "wifi_ssid=%s", secret_.wifi.ssid.c_str());
    ESP_LOGI(kTag, "apikey_stt=%s apikey_aiservice=%s apikey_tts=%s", secret_.api_key.stt.c_str(),
             secret_.api_key.ai_service.c_str(), secret_.api_key.tts.c_str());
}

void SystemConfig::loadExtendConfig(const std::string&) {}
void SystemConfig::setExtendSettings(const YamlValue&) {}
void SystemConfig::printExtParameters() const {}
void SystemConfig::basicConfigNotFoundCallback() {}
void SystemConfig::secretConfigNotFoundCallback() {}

}  // namespace SCEX
