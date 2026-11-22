#include "core/Json.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace cmdpal::core {

namespace {

constexpr int kMaxDepth = 64;

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s), pos_(0) {}

    std::optional<JsonValue> parseRoot(std::string* err) {
        skipWs();
        auto v = parseValue(0, err);
        if (!v) return std::nullopt;
        skipWs();
        if (pos_ != s_.size()) {
            if (err) *err = "Unexpected trailing characters at position " + std::to_string(pos_);
            return std::nullopt;
        }
        return v;
    }

private:
    const std::string& s_;
    size_t pos_;

    void skipWs() {
        while (pos_ < s_.size()) {
            char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    std::optional<JsonValue> parseValue(int depth, std::string* err) {
        if (depth > kMaxDepth) {
            if (err) *err = "Maximum JSON nesting depth exceeded";
            return std::nullopt;
        }
        skipWs();
        if (pos_ >= s_.size()) {
            if (err) *err = "Unexpected end of JSON input";
            return std::nullopt;
        }
        char c = s_[pos_];
        if (c == '{') return parseObject(depth + 1, err);
        if (c == '[') return parseArray(depth + 1, err);
        if (c == '"') {
            auto str = parseString(err);
            if (!str) return std::nullopt;
            return JsonValue(*str);
        }
        if (c == 't') {
            if (s_.compare(pos_, 4, "true") == 0) {
                pos_ += 4;
                return JsonValue(true);
            }
            if (err) *err = "Invalid token near position " + std::to_string(pos_);
            return std::nullopt;
        }
        if (c == 'f') {
            if (s_.compare(pos_, 5, "false") == 0) {
                pos_ += 5;
                return JsonValue(false);
            }
            if (err) *err = "Invalid token near position " + std::to_string(pos_);
            return std::nullopt;
        }
        if (c == 'n') {
            if (s_.compare(pos_, 4, "null") == 0) {
                pos_ += 4;
                return JsonValue(nullptr);
            }
            if (err) *err = "Invalid token near position " + std::to_string(pos_);
            return std::nullopt;
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return parseNumber(err);
        }
        if (err) *err = std::string("Unexpected character '") + c + "' at position " + std::to_string(pos_);
        return std::nullopt;
    }

    std::optional<JsonValue> parseObject(int depth, std::string* err) {
        ++pos_;  // '{'
        JsonObject obj;
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == '}') {
            ++pos_;
            return JsonValue(std::move(obj));
        }
        while (true) {
            skipWs();
            if (pos_ >= s_.size() || s_[pos_] != '"') {
                if (err) *err = "Expected string key at position " + std::to_string(pos_);
                return std::nullopt;
            }
            auto key = parseString(err);
            if (!key) return std::nullopt;
            skipWs();
            if (pos_ >= s_.size() || s_[pos_] != ':') {
                if (err) *err = "Expected ':' after key at position " + std::to_string(pos_);
                return std::nullopt;
            }
            ++pos_;
            auto val = parseValue(depth, err);
            if (!val) return std::nullopt;
            obj[*key] = std::move(*val);
            skipWs();
            if (pos_ >= s_.size()) {
                if (err) *err = "Unterminated JSON object";
                return std::nullopt;
            }
            if (s_[pos_] == '}') {
                ++pos_;
                return JsonValue(std::move(obj));
            }
            if (s_[pos_] != ',') {
                if (err) *err = "Expected ',' or '}' at position " + std::to_string(pos_);
                return std::nullopt;
            }
            ++pos_;
        }
    }

    std::optional<JsonValue> parseArray(int depth, std::string* err) {
        ++pos_;  // '['
        JsonArray arr;
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == ']') {
            ++pos_;
            return JsonValue(std::move(arr));
        }
        while (true) {
            auto val = parseValue(depth, err);
            if (!val) return std::nullopt;
            arr.push_back(std::move(*val));
            skipWs();
            if (pos_ >= s_.size()) {
                if (err) *err = "Unterminated JSON array";
                return std::nullopt;
            }
            if (s_[pos_] == ']') {
                ++pos_;
                return JsonValue(std::move(arr));
            }
            if (s_[pos_] != ',') {
                if (err) *err = "Expected ',' or ']' at position " + std::to_string(pos_);
                return std::nullopt;
            }
            ++pos_;
        }
    }

    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::optional<uint32_t> parseHex4(std::string* err) {
        if (pos_ + 4 > s_.size()) {
            if (err) *err = "Incomplete unicode escape";
            return std::nullopt;
        }
        uint32_t val = 0;
        for (int i = 0; i < 4; ++i) {
            char c = s_[pos_++];
            val <<= 4;
            if (c >= '0' && c <= '9') val |= (c - '0');
            else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
            else {
                if (err) *err = "Invalid hex digit in unicode escape";
                return std::nullopt;
            }
        }
        return val;
    }

    std::optional<std::string> parseString(std::string* err) {
        ++pos_;  // '"'
        std::string out;
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) {
                if (err) *err = "Unescaped control character in string";
                return std::nullopt;
            }
            if (c == '\\') {
                if (pos_ >= s_.size()) {
                    if (err) *err = "Unterminated escape sequence";
                    return std::nullopt;
                }
                char esc = s_[pos_++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        auto u1 = parseHex4(err);
                        if (!u1) return std::nullopt;
                        uint32_t cp = *u1;
                        if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 2 <= s_.size() &&
                            s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            auto u2 = parseHex4(err);
                            if (!u2) return std::nullopt;
                            if (*u2 >= 0xDC00 && *u2 <= 0xDFFF) {
                                cp = 0x10000 + (((cp - 0xD800) << 10) | (*u2 - 0xDC00));
                            }
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default:
                        if (err) *err = "Invalid escape sequence";
                        return std::nullopt;
                }
            } else {
                out.push_back(c);
            }
        }
        if (err) *err = "Unterminated string literal";
        return std::nullopt;
    }

    std::optional<JsonValue> parseNumber(std::string* err) {
        size_t start = pos_;
        if (s_[pos_] == '-') ++pos_;
        if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
            if (err) *err = "Invalid number at position " + std::to_string(start);
            return std::nullopt;
        }
        if (s_[pos_] == '0') {
            ++pos_;
        } else {
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        if (pos_ < s_.size() && s_[pos_] == '.') {
            ++pos_;
            if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                if (err) *err = "Invalid fraction in number";
                return std::nullopt;
            }
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                if (err) *err = "Invalid exponent in number";
                return std::nullopt;
            }
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        std::string numStr = s_.substr(start, pos_ - start);
        char* endPtr = nullptr;
        double val = std::strtod(numStr.c_str(), &endPtr);
        if (endPtr != numStr.c_str() + numStr.size() || !std::isfinite(val)) {
            if (err) *err = "Number out of range";
            return std::nullopt;
        }
        return JsonValue(val);
    }
};

void escapeString(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

}  // namespace

std::optional<JsonValue> JsonValue::parse(const std::string& text, std::string* errorOut) {
    Parser p(text);
    return p.parseRoot(errorOut);
}

std::string JsonValue::dump(int indent) const {
    std::string out;
    dumpImpl(out, indent, 0);
    return out;
}

void JsonValue::dumpImpl(std::string& out, int indent, int level) const {
    if (isNull()) {
        out += "null";
    } else if (isBool()) {
        out += std::get<bool>(data_) ? "true" : "false";
    } else if (isNumber()) {
        double d = std::get<double>(data_);
        if (std::floor(d) == d && std::abs(d) < 9007199254740992.0) {
            out += std::to_string(static_cast<int64_t>(d));
        } else {
            std::ostringstream oss;
            oss << d;
            out += oss.str();
        }
    } else if (isString()) {
        escapeString(out, std::get<std::string>(data_));
    } else if (isArray()) {
        const auto& arr = std::get<JsonArray>(data_);
        if (arr.empty()) {
            out += "[]";
            return;
        }
        out += "[\n";
        for (size_t i = 0; i < arr.size(); ++i) {
            out.append(static_cast<size_t>((level + 1) * indent), ' ');
            arr[i].dumpImpl(out, indent, level + 1);
            if (i + 1 < arr.size()) out.push_back(',');
            out.push_back('\n');
        }
        out.append(static_cast<size_t>(level * indent), ' ');
        out.push_back(']');
    } else if (isObject()) {
        const auto& obj = std::get<JsonObject>(data_);
        if (obj.empty()) {
            out += "{}";
            return;
        }
        out += "{\n";
        size_t idx = 0;
        for (const auto& [k, v] : obj) {
            out.append(static_cast<size_t>((level + 1) * indent), ' ');
            escapeString(out, k);
            out += ": ";
            v.dumpImpl(out, indent, level + 1);
            if (++idx < obj.size()) out.push_back(',');
            out.push_back('\n');
        }
        out.append(static_cast<size_t>(level * indent), ' ');
        out.push_back('}');
    }
}

}  // namespace cmdpal::core
