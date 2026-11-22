#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace cmdpal::core {

class JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

class JsonValue {
public:
    using Variant = std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject>;

    JsonValue() : data_(nullptr) {}
    JsonValue(std::nullptr_t) : data_(nullptr) {}
    JsonValue(bool b) : data_(b) {}
    JsonValue(int i) : data_(static_cast<double>(i)) {}
    JsonValue(int64_t i) : data_(static_cast<double>(i)) {}
    JsonValue(double d) : data_(d) {}
    JsonValue(const char* s) : data_(std::string(s ? s : "")) {}
    JsonValue(std::string s) : data_(std::move(s)) {}
    JsonValue(JsonArray a) : data_(std::move(a)) {}
    JsonValue(JsonObject o) : data_(std::move(o)) {}

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(data_); }
    bool isBool() const { return std::holds_alternative<bool>(data_); }
    bool isNumber() const { return std::holds_alternative<double>(data_); }
    bool isString() const { return std::holds_alternative<std::string>(data_); }
    bool isArray() const { return std::holds_alternative<JsonArray>(data_); }
    bool isObject() const { return std::holds_alternative<JsonObject>(data_); }

    bool asBool(bool fallback = false) const {
        return isBool() ? std::get<bool>(data_) : fallback;
    }
    double asNumber(double fallback = 0.0) const {
        return isNumber() ? std::get<double>(data_) : fallback;
    }
    int64_t asInt64(int64_t fallback = 0) const {
        return isNumber() ? static_cast<int64_t>(std::get<double>(data_)) : fallback;
    }
    int asInt(int fallback = 0) const {
        return isNumber() ? static_cast<int>(std::get<double>(data_)) : fallback;
    }
    const std::string& asString() const {
        static const std::string empty;
        return isString() ? std::get<std::string>(data_) : empty;
    }
    std::string asString(const std::string& fallback) const {
        return isString() ? std::get<std::string>(data_) : fallback;
    }
    const JsonArray& asArray() const {
        static const JsonArray empty;
        return isArray() ? std::get<JsonArray>(data_) : empty;
    }
    const JsonObject& asObject() const {
        static const JsonObject empty;
        return isObject() ? std::get<JsonObject>(data_) : empty;
    }

    const JsonValue* get(const std::string& key) const {
        if (!isObject()) return nullptr;
        const auto& obj = std::get<JsonObject>(data_);
        auto it = obj.find(key);
        return it == obj.end() ? nullptr : &it->second;
    }

    std::string dump(int indent = 2) const;

    static std::optional<JsonValue> parse(const std::string& text, std::string* errorOut = nullptr);

private:
    Variant data_;
    void dumpImpl(std::string& out, int indent, int level) const;
};

}  // namespace cmdpal::core
