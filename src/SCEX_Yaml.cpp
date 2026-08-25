#include "SCEX_Yaml.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace SCEX {

// ---------------------------------------------------------------------------
// YamlValue
// ---------------------------------------------------------------------------

YamlValue YamlValue::makeBool(bool v) {
    YamlValue val;
    val.type_ = YamlType::Bool;
    val.bool_ = v;
    return val;
}

YamlValue YamlValue::makeInt(long v) {
    YamlValue val;
    val.type_ = YamlType::Int;
    val.int_ = v;
    return val;
}

YamlValue YamlValue::makeFloat(double v) {
    YamlValue val;
    val.type_ = YamlType::Float;
    val.float_ = v;
    return val;
}

YamlValue YamlValue::makeString(std::string v) {
    YamlValue val;
    val.type_ = YamlType::String;
    val.string_ = std::move(v);
    return val;
}

YamlValue YamlValue::makeSequence() {
    YamlValue val;
    val.type_ = YamlType::Sequence;
    return val;
}

YamlValue YamlValue::makeMapping() {
    YamlValue val;
    val.type_ = YamlType::Mapping;
    return val;
}

bool YamlValue::asBool(bool default_value) const {
    switch (type_) {
        case YamlType::Bool: return bool_;
        case YamlType::Int: return int_ != 0;
        case YamlType::String: return string_ == "true" || string_ == "True" || string_ == "1";
        default: return default_value;
    }
}

long YamlValue::asInt(long default_value) const {
    switch (type_) {
        case YamlType::Int: return int_;
        case YamlType::Float: return static_cast<long>(float_);
        case YamlType::Bool: return bool_ ? 1 : 0;
        case YamlType::String: {
            char* end = nullptr;
            long v = std::strtol(string_.c_str(), &end, 10);
            return (end != nullptr && end != string_.c_str()) ? v : default_value;
        }
        default: return default_value;
    }
}

double YamlValue::asFloat(double default_value) const {
    switch (type_) {
        case YamlType::Float: return float_;
        case YamlType::Int: return static_cast<double>(int_);
        case YamlType::String: {
            char* end = nullptr;
            double v = std::strtod(string_.c_str(), &end);
            return (end != nullptr && end != string_.c_str()) ? v : default_value;
        }
        default: return default_value;
    }
}

std::string YamlValue::asString(const std::string& default_value) const {
    switch (type_) {
        case YamlType::String: return string_;
        case YamlType::Bool: return bool_ ? "true" : "false";
        case YamlType::Int: return std::to_string(int_);
        case YamlType::Float: return std::to_string(float_);
        default: return default_value;
    }
}

const YamlValue& YamlValue::operator[](const std::string& key) const {
    static const YamlValue kNull;
    if (type_ != YamlType::Mapping) return kNull;
    for (const auto& kv : mapping_) {
        if (kv.first == key) return kv.second;
    }
    return kNull;
}

const YamlValue& YamlValue::operator[](size_t index) const {
    static const YamlValue kNull;
    if (type_ != YamlType::Sequence || index >= sequence_.size()) return kNull;
    return sequence_[index];
}

size_t YamlValue::size() const {
    if (type_ == YamlType::Sequence) return sequence_.size();
    if (type_ == YamlType::Mapping) return mapping_.size();
    return 0;
}

void YamlValue::setEntry(const std::string& key, YamlValue value) {
    for (auto& kv : mapping_) {
        if (kv.first == key) {
            kv.second = std::move(value);
            return;
        }
    }
    mapping_.emplace_back(key, std::move(value));
}

void YamlValue::append(YamlValue value) { sequence_.push_back(std::move(value)); }

// ---------------------------------------------------------------------------
// YamlParser
// ---------------------------------------------------------------------------

namespace {

struct Line {
    int indent;
    std::string content;
};

std::string trimTrailing(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\r");
    return end == std::string::npos ? std::string() : s.substr(0, end + 1);
}

std::string trim(const std::string& s) {
    size_t begin = s.find_first_not_of(" \t\r");
    if (begin == std::string::npos) return std::string();
    size_t end = s.find_last_not_of(" \t\r");
    return s.substr(begin, end - begin + 1);
}

// Strips a trailing '#' comment, honoring single/double-quoted spans.
std::string stripComment(const std::string& line) {
    bool in_single = false;
    bool in_double = false;
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (c == '\'' && !in_double) in_single = !in_single;
        else if (c == '"' && !in_single) in_double = !in_double;
        else if (c == '#' && !in_single && !in_double) {
            if (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1]))) {
                return line.substr(0, i);
            }
        }
    }
    return line;
}

std::vector<Line> preprocess(const std::string& text) {
    std::vector<Line> lines;
    std::istringstream stream(text);
    std::string raw;
    while (std::getline(stream, raw)) {
        std::string no_comment = trimTrailing(stripComment(raw));
        if (no_comment.empty()) continue;
        size_t indent = no_comment.find_first_not_of(' ');
        if (indent == std::string::npos) continue;
        std::string content = no_comment.substr(indent);
        if (content == "---" || content == "...") continue;
        lines.push_back({static_cast<int>(indent), content});
    }
    return lines;
}

bool isSequenceItem(const std::string& content) {
    return content == "-" || (content.size() >= 2 && content[0] == '-' && content[1] == ' ');
}

// Finds the first ':' that separates a mapping key from its value (i.e. not
// inside a quoted string, and followed by a space or end-of-line).
bool splitKeyValue(const std::string& content, std::string* key, std::string* value) {
    bool in_single = false;
    bool in_double = false;
    for (size_t i = 0; i < content.size(); i++) {
        char c = content[i];
        if (c == '\'' && !in_double) in_single = !in_single;
        else if (c == '"' && !in_single) in_double = !in_double;
        else if (c == ':' && !in_single && !in_double) {
            if (i + 1 == content.size() || content[i + 1] == ' ') {
                *key = trim(content.substr(0, i));
                *value = trim(i + 1 < content.size() ? content.substr(i + 1) : std::string());
                return !key->empty();
            }
        }
    }
    return false;
}

YamlValue parseScalar(const std::string& raw) {
    std::string s = trim(raw);
    if (s.empty() || s == "~" || s == "null" || s == "Null" || s == "NULL") {
        return YamlValue();
    }
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        std::string out;
        for (size_t i = 1; i + 1 < s.size(); i++) {
            if (s[i] == '\\' && i + 2 < s.size() + 1 && i + 1 < s.size()) {
                char next = s[i + 1];
                if (next == 'n') { out += '\n'; i++; continue; }
                if (next == 't') { out += '\t'; i++; continue; }
                if (next == '"' || next == '\\') { out += next; i++; continue; }
            }
            out += s[i];
        }
        return YamlValue::makeString(out);
    }
    if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'') {
        std::string inner = s.substr(1, s.size() - 2);
        std::string out;
        for (size_t i = 0; i < inner.size(); i++) {
            if (inner[i] == '\'' && i + 1 < inner.size() && inner[i + 1] == '\'') {
                out += '\'';
                i++;
            } else {
                out += inner[i];
            }
        }
        return YamlValue::makeString(out);
    }
    if (s == "true" || s == "True" || s == "TRUE") return YamlValue::makeBool(true);
    if (s == "false" || s == "False" || s == "FALSE") return YamlValue::makeBool(false);

    // Integer?
    {
        char* end = nullptr;
        long v = std::strtol(s.c_str(), &end, 10);
        if (end != nullptr && *end == '\0' && end != s.c_str()) {
            return YamlValue::makeInt(v);
        }
    }
    // Float?
    {
        char* end = nullptr;
        double v = std::strtod(s.c_str(), &end);
        if (end != nullptr && *end == '\0' && end != s.c_str()) {
            return YamlValue::makeFloat(v);
        }
    }
    return YamlValue::makeString(s);
}

class Reader {
public:
    explicit Reader(const std::vector<Line>& lines) : lines_(lines) {}

    YamlValue parseNode(int indent) {
        if (idx_ >= lines_.size() || lines_[idx_].indent != indent) {
            return YamlValue();
        }
        if (isSequenceItem(lines_[idx_].content)) {
            return parseSequence(indent);
        }
        return parseMapping(indent);
    }

    size_t remaining() const { return lines_.size() - idx_; }

private:
    // Consumes zero or more "key: value" lines at exactly `indent` into `map`.
    // Shared by parseMapping() and by "- key: value" sequence-item continuations.
    void parseMappingBody(int indent, YamlValue& map) {
        while (idx_ < lines_.size() && lines_[idx_].indent == indent &&
               !isSequenceItem(lines_[idx_].content)) {
            std::string key, value;
            if (!splitKeyValue(lines_[idx_].content, &key, &value)) {
                idx_++;  // malformed line: skip defensively
                continue;
            }
            idx_++;
            addEntry(map, key, value, indent);
        }
    }

    void addEntry(YamlValue& map, const std::string& key, const std::string& value, int indent) {
        if (!value.empty()) {
            map.setEntry(key, parseScalar(value));
            return;
        }
        if (idx_ < lines_.size() && lines_[idx_].indent > indent) {
            map.setEntry(key, parseNode(lines_[idx_].indent));
        } else {
            map.setEntry(key, YamlValue());
        }
    }

    YamlValue parseMapping(int indent) {
        YamlValue map = YamlValue::makeMapping();
        parseMappingBody(indent, map);
        return map;
    }

    YamlValue parseSequence(int indent) {
        YamlValue seq = YamlValue::makeSequence();
        while (idx_ < lines_.size() && lines_[idx_].indent == indent &&
               isSequenceItem(lines_[idx_].content)) {
            const std::string& content = lines_[idx_].content;
            std::string rest = content.size() > 1 ? content.substr(2) : std::string();
            idx_++;
            int child_indent = indent + 2;

            if (rest.empty()) {
                if (idx_ < lines_.size() && lines_[idx_].indent > indent) {
                    seq.append(parseNode(lines_[idx_].indent));
                } else {
                    seq.append(YamlValue());
                }
                continue;
            }
            std::string key, value;
            if (splitKeyValue(rest, &key, &value)) {
                YamlValue map = YamlValue::makeMapping();
                addEntry(map, key, value, indent);
                parseMappingBody(child_indent, map);
                seq.append(map);
            } else {
                seq.append(parseScalar(rest));
            }
        }
        return seq;
    }

    const std::vector<Line>& lines_;
    size_t idx_ = 0;
};

}  // namespace

bool YamlParser::parse(const std::string& text, YamlValue* out, std::string* error) {
    std::vector<Line> lines = preprocess(text);
    if (lines.empty()) {
        *out = YamlValue::makeMapping();
        return true;
    }
    if (lines.front().indent != 0) {
        if (error != nullptr) *error = "document must start at column 0";
        return false;
    }
    Reader reader(lines);
    *out = reader.parseNode(0);
    return true;
}

}  // namespace SCEX
