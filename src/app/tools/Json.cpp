#include "app/tools/Json.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

const Json::Value kNullValue;

void AppendUtf8(std::string& out, uint32_t code_point) {
    if (code_point <= 0x7F) {
        out.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else if (code_point <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
}

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

struct Parser {
    std::string_view text;
    size_t pos = 0;
    std::string error;
    int depth = 0;

    void SkipWhitespace() {
        while (pos < text.size()) {
            char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos;
            } else {
                break;
            }
        }
    }

    bool Fail(const char* message) {
        if (error.empty()) error = std::string(message) + " at " + std::to_string(pos);
        return false;
    }

    bool Match(std::string_view word) {
        if (text.substr(pos, word.size()) != word) return Fail("bad literal");
        pos += word.size();
        return true;
    }

    bool ParseHex4(uint32_t& out) {
        if (pos + 4 > text.size()) return Fail("short \\u escape");
        out = 0;
        for (int i = 0; i < 4; ++i) {
            char c = text[pos++];
            out <<= 4;
            if (c >= '0' && c <= '9') {
                out |= static_cast<uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                out |= static_cast<uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                out |= static_cast<uint32_t>(c - 'A' + 10);
            } else {
                return Fail("bad hex digit");
            }
        }
        return true;
    }

    bool ParseString(std::string& out) {
        if (pos >= text.size() || text[pos] != '"') return Fail("expected string");
        ++pos;
        out.clear();
        while (pos < text.size()) {
            char c = text[pos++];
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos >= text.size()) return Fail("bad escape");
            char escape = text[pos++];
            switch (escape) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    uint32_t code_point = 0;
                    if (!ParseHex4(code_point)) return false;
                    // emoji 在 JSON 里是代理对，必须拼成一个码点
                    if (code_point >= 0xD800 && code_point <= 0xDBFF && pos + 6 <= text.size() &&
                        text[pos] == '\\' && text[pos + 1] == 'u') {
                        size_t saved = pos;
                        pos += 2;
                        uint32_t low = 0;
                        if (ParseHex4(low) && low >= 0xDC00 && low <= 0xDFFF) {
                            code_point = 0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);
                        } else {
                            pos = saved;
                        }
                    }
                    AppendUtf8(out, code_point);
                    break;
                }
                default:
                    return Fail("unknown escape");
            }
        }
        return Fail("unterminated string");
    }

    bool ParseNumber(Json::Value& out) {
        size_t start = pos;
        bool is_float = false;
        if (text[pos] == '-') ++pos;
        while (pos < text.size() && IsDigit(text[pos])) ++pos;
        if (pos < text.size() && text[pos] == '.') {
            is_float = true;
            ++pos;
            while (pos < text.size() && IsDigit(text[pos])) ++pos;
        }
        if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
            is_float = true;
            ++pos;
            if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) ++pos;
            while (pos < text.size() && IsDigit(text[pos])) ++pos;
        }
        std::string token(text.substr(start, pos - start));
        if (token.empty() || token == "-") return Fail("bad number");
        if (is_float) {
            out = Json::Value::Double(std::strtod(token.c_str(), nullptr));
            return true;
        }
        errno = 0;
        long long value = std::strtoll(token.c_str(), nullptr, 10);
        if (errno == ERANGE) {
            out = Json::Value::Double(std::strtod(token.c_str(), nullptr));
        } else {
            out = Json::Value::Int(static_cast<int64_t>(value));
        }
        return true;
    }

    bool ParseValue(Json::Value& out) {
        if (++depth > 256) return Fail("nesting too deep");
        SkipWhitespace();
        if (pos >= text.size()) return Fail("unexpected end");
        bool ok = false;
        switch (text[pos]) {
            case '{': ok = ParseObject(out); break;
            case '[': ok = ParseArray(out); break;
            case '"': {
                std::string value;
                ok = ParseString(value);
                if (ok) out = Json::Value::String(std::move(value));
                break;
            }
            case 't':
                ok = Match("true");
                if (ok) out = Json::Value::Bool(true);
                break;
            case 'f':
                ok = Match("false");
                if (ok) out = Json::Value::Bool(false);
                break;
            case 'n':
                ok = Match("null");
                break;
            default:
                if (text[pos] == '-' || IsDigit(text[pos])) {
                    ok = ParseNumber(out);
                } else {
                    return Fail("unexpected character");
                }
        }
        --depth;
        return ok;
    }

    bool ParseArray(Json::Value& out) {
        ++pos;
        out = Json::Value::Array();
        SkipWhitespace();
        if (pos < text.size() && text[pos] == ']') {
            ++pos;
            return true;
        }
        while (true) {
            Json::Value item;
            if (!ParseValue(item)) return false;
            out.Push(std::move(item));
            SkipWhitespace();
            if (pos >= text.size()) return Fail("unterminated array");
            if (text[pos] == ',') {
                ++pos;
                continue;
            }
            if (text[pos] == ']') {
                ++pos;
                return true;
            }
            return Fail("expected , or ]");
        }
    }

    bool ParseObject(Json::Value& out) {
        ++pos;
        out = Json::Value::Object();
        SkipWhitespace();
        if (pos < text.size() && text[pos] == '}') {
            ++pos;
            return true;
        }
        while (true) {
            SkipWhitespace();
            std::string key;
            if (!ParseString(key)) return false;
            SkipWhitespace();
            if (pos >= text.size() || text[pos] != ':') return Fail("expected :");
            ++pos;
            Json::Value item;
            if (!ParseValue(item)) return false;
            out.Set(std::move(key), std::move(item));
            SkipWhitespace();
            if (pos >= text.size()) return Fail("unterminated object");
            if (text[pos] == ',') {
                ++pos;
                continue;
            }
            if (text[pos] == '}') {
                ++pos;
                return true;
            }
            return Fail("expected , or }");
        }
    }
};

}  // namespace

Json::Value Json::Value::Null() { return {}; }

Json::Value Json::Value::Bool(bool value) {
    Value out;
    out.type_ = Type::Bool;
    out.bool_ = value;
    return out;
}

Json::Value Json::Value::Int(int64_t value) {
    Value out;
    out.type_ = Type::Int;
    out.int_ = value;
    return out;
}

Json::Value Json::Value::Double(double value) {
    Value out;
    out.type_ = Type::Double;
    out.double_ = value;
    return out;
}

Json::Value Json::Value::String(std::string value) {
    Value out;
    out.type_ = Type::String;
    out.string_ = std::move(value);
    return out;
}

Json::Value Json::Value::Array() {
    Value out;
    out.type_ = Type::Array;
    return out;
}

Json::Value Json::Value::Object() {
    Value out;
    out.type_ = Type::Object;
    return out;
}

bool Json::Value::AsBool(bool fallback) const {
    if (type_ == Type::Bool) return bool_;
    if (type_ == Type::Int) return int_ != 0;
    return fallback;
}

int64_t Json::Value::AsInt(int64_t fallback) const {
    if (type_ == Type::Int) return int_;
    if (type_ == Type::Double) return static_cast<int64_t>(double_);
    if (type_ == Type::Bool) return bool_ ? 1 : 0;
    return fallback;
}

double Json::Value::AsDouble(double fallback) const {
    if (type_ == Type::Double) return double_;
    if (type_ == Type::Int) return static_cast<double>(int_);
    return fallback;
}

const std::string& Json::Value::AsString() const {
    static const std::string kEmpty;
    return type_ == Type::String ? string_ : kEmpty;
}

size_t Json::Value::Size() const {
    if (type_ == Type::Array) return array_.size();
    if (type_ == Type::Object) return object_.size();
    return 0;
}

const Json::Value& Json::Value::At(size_t index) const {
    if (type_ == Type::Array && index < array_.size()) return array_[index];
    if (type_ == Type::Object && index < object_.size()) return object_[index].second;
    return kNullValue;
}

const std::string& Json::Value::KeyAt(size_t index) const {
    static const std::string kEmpty;
    if (type_ != Type::Object || index >= object_.size()) return kEmpty;
    return object_[index].first;
}

const Json::Value& Json::Value::operator[](std::string_view key) const {
    const Value* found = Find(key);
    return found ? *found : kNullValue;
}

const Json::Value* Json::Value::Find(std::string_view key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& [k, v] : object_) {
        if (k == key) return &v;
    }
    return nullptr;
}

Json::Value& Json::Value::Push(Value value) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        array_.clear();
    }
    array_.push_back(std::move(value));
    return array_.back();
}

Json::Value& Json::Value::Set(std::string key, Value value) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        object_.clear();
    }
    for (auto& [k, existing] : object_) {
        if (k == key) {
            existing = std::move(value);
            return existing;
        }
    }
    object_.emplace_back(std::move(key), std::move(value));
    return object_.back().second;
}

std::string Json::Value::Dump() const {
    std::string out;
    DumpTo(out);
    return out;
}

void Json::Value::DumpTo(std::string& out) const {
    switch (type_) {
        case Type::Null:
            out += "null";
            break;
        case Type::Bool:
            out += bool_ ? "true" : "false";
            break;
        case Type::Int:
            out += std::to_string(int_);
            break;
        case Type::Double: {
            char buffer[64];
            if (std::isfinite(double_)) {
                std::snprintf(buffer, sizeof(buffer), "%.17g", double_);
            } else {
                std::snprintf(buffer, sizeof(buffer), "null");
            }
            out += buffer;
            break;
        }
        case Type::String: {
            out.push_back('"');
            for (unsigned char c : string_) {
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
                            char buffer[8];
                            std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                            out += buffer;
                        } else {
                            out.push_back(static_cast<char>(c));
                        }
                }
            }
            out.push_back('"');
            break;
        }
        case Type::Array: {
            out.push_back('[');
            for (size_t i = 0; i < array_.size(); ++i) {
                if (i) out.push_back(',');
                array_[i].DumpTo(out);
            }
            out.push_back(']');
            break;
        }
        case Type::Object: {
            out.push_back('{');
            for (size_t i = 0; i < object_.size(); ++i) {
                if (i) out.push_back(',');
                Value::String(object_[i].first).DumpTo(out);
                out.push_back(':');
                object_[i].second.DumpTo(out);
            }
            out.push_back('}');
            break;
        }
    }
}

bool Json::Parse(std::string_view text, Value& out, std::string& error) const {
    // 记事本 / PowerShell 存出来的 JSON 常带 UTF-8 BOM，解析器得先吃掉
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }

    Parser parser{text, 0, {}, 0};
    Value result;
    if (!parser.ParseValue(result)) {
        error = parser.error.empty() ? "parse failed" : parser.error;
        return false;
    }
    parser.SkipWhitespace();
    if (parser.pos != text.size()) {
        error = "trailing characters at " + std::to_string(parser.pos);
        return false;
    }
    out = std::move(result);
    return true;
}
