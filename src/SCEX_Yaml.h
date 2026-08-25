// Minimal, dependency-free YAML reader used instead of YAMLDuino (which
// wraps libyaml behind Arduino Stream/File APIs). Supports the subset of
// YAML actually needed to write a Stack-chan config file:
//   - block mappings ("key: value", nested by indentation)
//   - block sequences ("- item", including "- key: value" list-of-maps)
//   - scalars: strings (bare, 'single', "double"), integers, floats,
//     booleans (true/false), null (~ / null / empty)
//   - '#' comments (honoring quotes) and blank lines
// NOT supported: anchors/aliases, flow style ([a, b] / {a: b}), tags,
// multi-document streams. This is intentionally a config-file parser, not a
// general YAML 1.1 implementation.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace SCEX {

enum class YamlType { Null, Bool, Int, Float, String, Sequence, Mapping };

class YamlValue {
public:
    YamlValue() : type_(YamlType::Null) {}

    static YamlValue makeBool(bool v);
    static YamlValue makeInt(long v);
    static YamlValue makeFloat(double v);
    static YamlValue makeString(std::string v);
    static YamlValue makeSequence();
    static YamlValue makeMapping();

    YamlType type() const { return type_; }
    bool isNull() const { return type_ == YamlType::Null; }

    bool asBool(bool default_value = false) const;
    long asInt(long default_value = 0) const;
    double asFloat(double default_value = 0.0) const;
    std::string asString(const std::string& default_value = "") const;

    // Mapping/sequence access. Always safe: returns a null YamlValue if the
    // key/index does not exist or `this` is not the expected container type.
    const YamlValue& operator[](const std::string& key) const;
    const YamlValue& operator[](size_t index) const;
    size_t size() const;

    void setEntry(const std::string& key, YamlValue value);
    void append(YamlValue value);

    const std::vector<std::pair<std::string, YamlValue>>& entries() const { return mapping_; }
    const std::vector<YamlValue>& items() const { return sequence_; }

private:
    YamlType type_;
    bool bool_ = false;
    long int_ = 0;
    double float_ = 0.0;
    std::string string_;
    std::vector<YamlValue> sequence_;
    std::vector<std::pair<std::string, YamlValue>> mapping_;
};

class YamlParser {
public:
    static bool parse(const std::string& text, YamlValue* out, std::string* error = nullptr);
};

}  // namespace SCEX
