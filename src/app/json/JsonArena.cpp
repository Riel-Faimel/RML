#include "app/json/JsonArena.h"

#include <chrono>
#include <cctype>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

#include "app/tools/tools.h"

struct JsonProxy::Document {
    std::string path;
    std::string key;

    Json::Value root = Json::Value::Object();
    Json::Value original = Json::Value::Object();

    std::mutex mutex;
    std::condition_variable cv;
    bool busy = false;
    bool loaded = false;
    bool valid = false;
    std::string error;
    std::filesystem::file_time_type mtime{};
    void* lock_handle = nullptr;
};

namespace {

#ifdef _WIN32
void CloseProcessLock(JsonProxy::Document& doc) {
    if (doc.lock_handle == nullptr) return;
    CloseHandle(static_cast<HANDLE>(doc.lock_handle));
    doc.lock_handle = nullptr;
}
#endif

bool AcquireProcessLock(JsonProxy::Document& doc) {
#ifdef _WIN32
    const std::filesystem::path path = tools::ToPath(doc.path + ".lock");
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        const auto parent_u8 = parent.u8string();
        const std::string parent_text(parent_u8.begin(), parent_u8.end());
        std::string dir_error;
        if (!tools::EnsureDir(parent_text, dir_error)) {
            doc.error = dir_error;
            return false;
        }
    }
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        doc.error = "JSON 文件正在被另一个实例使用: " + doc.path;
        return false;
    }
    doc.lock_handle = handle;
#endif
    return true;
}

std::string NormalizeKey(const std::string& path) {
    std::filesystem::path input = tools::ToPath(path);
    std::error_code ec;
    auto absolute = std::filesystem::absolute(input, ec);
    if (ec) absolute = input;
    ec.clear();
    auto canonical = std::filesystem::weakly_canonical(absolute, ec);
    if (!ec) absolute = canonical;

    const auto u8 = absolute.generic_u8string();
    std::string key(u8.begin(), u8.end());
#ifdef _WIN32
    for (char& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
#endif
    return key;
}

std::string ActualPath(const std::string& path) {
    std::filesystem::path input = tools::ToPath(path);
    std::error_code ec;
    auto absolute = std::filesystem::absolute(input, ec);
    if (ec) absolute = input;
    ec.clear();
    auto canonical = std::filesystem::weakly_canonical(absolute, ec);
    if (!ec) absolute = canonical;
    const auto u8 = absolute.generic_u8string();
    return std::string(u8.begin(), u8.end());
}

}  // namespace

JsonProxy::JsonProxy(std::shared_ptr<Document> document) : document_(std::move(document)) {
    if (!document_) {
        error_ = "JSON 文档为空";
        return;
    }

    Document& doc = *document_;
    std::unique_lock<std::mutex> lock(doc.mutex);
    if (!doc.cv.wait_for(lock, std::chrono::seconds(10), [&doc] { return !doc.busy; })) {
        error_ = "JSON 文件正在被其他操作使用: " + doc.path;
        return;
    }
    doc.busy = true;

    if (!AcquireProcessLock(doc)) {
        doc.busy = false;
        doc.cv.notify_one();
        error_ = doc.error;
        return;
    }

    std::error_code ec;
    const bool exists = std::filesystem::exists(tools::ToPath(doc.path), ec);
    const auto current_mtime = exists ? std::filesystem::last_write_time(tools::ToPath(doc.path), ec)
                                      : std::filesystem::file_time_type{};
    if (doc.loaded && !ec && current_mtime == doc.mtime) {
        valid_ = doc.valid;
        error_ = doc.error;
        return;
    }

    doc.root = Json::Value::Object();
    doc.original = Json::Value::Object();
    doc.loaded = true;
    doc.valid = true;
    doc.error.clear();
    doc.mtime = current_mtime;

    if (exists) {
        std::ifstream in(tools::ToPath(doc.path), std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        Json parser;
        std::string parse_error;
        if (!parser.Parse(text, doc.root, parse_error)) {
            doc.valid = false;
            doc.error = "解析 JSON 失败: " + doc.path + "（" + parse_error + "）";
            doc.busy = false;
#ifdef _WIN32
            CloseProcessLock(doc);
#endif
            doc.cv.notify_one();
            error_ = doc.error;
            return;
        }
        doc.original = doc.root;
    }

    valid_ = doc.valid;
    error_ = doc.error;
}

JsonProxy::~JsonProxy() { Release(); }

JsonProxy::JsonProxy(JsonProxy&& other) noexcept
    : document_(std::move(other.document_)),
      valid_(other.valid_),
      error_(std::move(other.error_)) {
    other.valid_ = false;
}

JsonProxy& JsonProxy::operator=(JsonProxy&& other) noexcept {
    if (this == &other) return *this;
    Release();
    document_ = std::move(other.document_);
    valid_ = other.valid_;
    error_ = std::move(other.error_);
    other.valid_ = false;
    other.document_.reset();
    return *this;
}

Json::Value& JsonProxy::operator[](const std::string& key) {
    static Json::Value null_value;
    if (!document_ || !valid_) return null_value;
    return document_->root[key];
}

const Json::Value& JsonProxy::operator[](const std::string& key) const {
    static Json::Value null_value;
    if (!document_ || !valid_) return null_value;
    const Json::Value& root = document_->root;
    return root[key];
}

void JsonProxy::create(const std::string& key) {
    if (!document_ || !valid_) return;
    if (document_->root.Find(key) == nullptr) {
        document_->root.Set(key, Json::Value::Object());
    }
}

void JsonProxy::remove(const std::string& key) {
    if (!document_ || !valid_) return;
    document_->root.Remove(key);
}

bool JsonProxy::commit(std::string& error) {
    if (!document_ || !valid_) {
        error = error_.empty() ? "JSON 代理无效" : error_;
        return false;
    }

    Document& doc = *document_;
    std::lock_guard<std::mutex> lock(doc.mutex);
    if (!doc.valid) {
        error = doc.error;
        return false;
    }
    if (doc.root.Dump() == doc.original.Dump()) return true;

    const std::string text = doc.root.Dump();
    const std::filesystem::path parent_path = tools::ToPath(doc.path).parent_path();
    const auto parent_u8 = parent_path.u8string();
    const std::string parent(parent_u8.begin(), parent_u8.end());
    std::string dir_error;
    if (!tools::EnsureDir(parent, dir_error)) {
        error = dir_error;
        return false;
    }

    const std::string temp = doc.path + ".tmp";
    {
        std::ofstream out(tools::ToPath(temp), std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "写 JSON 临时文件失败: " + temp;
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) {
            error = "写 JSON 临时文件失败: " + temp;
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tools::ToPath(temp), tools::ToPath(doc.path), ec);
    if (ec) {
        ec.clear();
        std::filesystem::copy_file(tools::ToPath(temp), tools::ToPath(doc.path),
                                   std::filesystem::copy_options::overwrite_existing, ec);
        std::error_code ignored;
        std::filesystem::remove(tools::ToPath(temp), ignored);
    }
    if (ec) {
        error = "替换 JSON 文件失败: " + doc.path;
        return false;
    }

    std::error_code time_ec;
    doc.mtime = std::filesystem::last_write_time(tools::ToPath(doc.path), time_ec);
    doc.original = doc.root;
    return true;
}

void JsonProxy::rollback() {
    if (!document_ || !valid_) return;
    document_->root = document_->original;
}

bool JsonProxy::dirty() const {
    if (!document_ || !valid_) return false;
    return document_->root.Dump() != document_->original.Dump();
}

void JsonProxy::Release() {
    if (!document_) return;
    Document& doc = *document_;
    std::unique_lock<std::mutex> lock(doc.mutex);
    if (!doc.busy) {
        document_.reset();
        return;
    }
    const bool need_commit =
        doc.valid && doc.root.Dump() != doc.original.Dump();
    lock.unlock();
    if (need_commit) {
        std::string ignored;
        commit(ignored);
    }
    lock.lock();
    doc.busy = false;
#ifdef _WIN32
    CloseProcessLock(doc);
#endif
    doc.cv.notify_one();
    document_.reset();
    valid_ = false;
}

JsonArena& JsonArena::Instance() {
    static JsonArena arena;
    return arena;
}

JsonProxy JsonArena::Open(const std::string& path) {
    const std::string key = NormalizeKey(path);
    std::shared_ptr<JsonProxy::Document> document;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& slot = documents_[key];
        if (!slot) {
            slot = std::make_shared<JsonProxy::Document>();
            slot->path = ActualPath(path);
            slot->key = key;
        }
        document = slot;
    }
    return JsonProxy(std::move(document));
}
