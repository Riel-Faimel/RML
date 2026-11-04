// JSON 解析器：手写递归下降，容忍 BOM，报告出错位置。
#include "app/tools/Json.h"

#include <cerrno>
#include <cstdlib>

namespace {

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
