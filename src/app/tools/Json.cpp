// JSON 值：构造、访问与序列化。
#include "app/tools/Json.h"

#include <cmath>
#include <cstdio>

namespace {

const Json::Value kNullValue;

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

Json::Value& Json::Value::operator[](std::string_view key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        object_.clear();
    }
    for (auto& [k, v] : object_) {
        if (k == key) return v;
    }
    object_.emplace_back(std::string(key), Value());
    return object_.back().second;
}

bool Json::Value::Remove(std::string_view key) {
    if (type_ != Type::Object) return false;
    for (auto it = object_.begin(); it != object_.end(); ++it) {
        if (it->first == key) {
            object_.erase(it);
            return true;
        }
    }
    return false;
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
