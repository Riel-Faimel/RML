// 全局 JSON 对象分配器：同一个路径只存在一个文档对象，调用方只借用代理并表达编辑意图。
#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "app/tools/Json.h"

class JsonArena;

// 借来的 JSON 代理：离开作用域自动提交/回滚，不暴露锁、临时文件和 rename。
class JsonProxy {
public:
    struct Document;

    JsonProxy() = default;
    ~JsonProxy();

    JsonProxy(const JsonProxy&) = delete;
    JsonProxy& operator=(const JsonProxy&) = delete;
    JsonProxy(JsonProxy&& other) noexcept;
    JsonProxy& operator=(JsonProxy&& other) noexcept;

    explicit operator bool() const { return valid_; }

    Json::Value& operator[](const std::string& key);
    const Json::Value& operator[](const std::string& key) const;
    const Json::Value& Get(const std::string& key) const { return (*this)[key]; }

    void create(const std::string& key);
    void remove(const std::string& key);

    bool commit(std::string& error);
    void rollback();
    bool dirty() const;
    const std::string& error() const { return error_; }

private:
    friend class JsonArena;

    explicit JsonProxy(std::shared_ptr<Document> document);
    void Release();

    std::shared_ptr<Document> document_;
    bool valid_ = false;
    std::string error_;
};

class JsonArena {
public:
    static JsonArena& Instance();

    // path 会被规范化；同一路径的代理指向同一个 Document。
    JsonProxy Open(const std::string& path);

private:
    JsonArena() = default;

    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<JsonProxy::Document>> documents_;
};
