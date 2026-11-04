// 模块 Json：全局命名空间的类。只做解析与序列化，不掺业务。
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class Json {
public:
    enum class Type : uint8_t { Null, Bool, Int, Double, String, Array, Object };

    class Value {
    public:
        Value() = default;

        static Value Null();
        static Value Bool(bool value);
        static Value Int(int64_t value);
        static Value Double(double value);
        static Value String(std::string value);
        static Value Array();
        static Value Object();

        Type GetType() const { return type_; }
        bool IsNull() const { return type_ == Type::Null; }
        bool IsBool() const { return type_ == Type::Bool; }
        bool IsInt() const { return type_ == Type::Int; }
        bool IsDouble() const { return type_ == Type::Double; }
        bool IsNumber() const { return IsInt() || IsDouble(); }
        bool IsString() const { return type_ == Type::String; }
        bool IsArray() const { return type_ == Type::Array; }
        bool IsObject() const { return type_ == Type::Object; }

        bool AsBool(bool fallback = false) const;
        int64_t AsInt(int64_t fallback = 0) const;
        const std::string& AsString() const;

        size_t Size() const;
        const Value& At(size_t index) const;
        const std::string& KeyAt(size_t index) const;
        const Value& operator[](std::string_view key) const;
        Value& operator[](std::string_view key);
        const Value* Find(std::string_view key) const;
        bool Remove(std::string_view key);

        Value& Push(Value value);
        Value& Set(std::string key, Value value);

        std::string Dump() const;

    private:
        void DumpTo(std::string& out) const;

        Type type_ = Type::Null;
        bool bool_ = false;
        int64_t int_ = 0;
        double double_ = 0.0;
        std::string string_;
        std::vector<Value> array_;
        std::vector<std::pair<std::string, Value>> object_;
    };

    bool Parse(std::string_view text, Value& out, std::string& error) const;
};
