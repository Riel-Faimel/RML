#include "app/download/VersionSource/Sources/Sources.h"

#include <utility>

#include "app/directory/Directory.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace download {
namespace versionsource {

namespace {

struct DefaultSource {
    const char* kind;
    const char* name;
    const char* url;
};

constexpr DefaultSource kDefaults[] = {
    {"mojang", "Mojang 版本清单",
     "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"},
    {"assets", "Mojang 资源", "https://resources.download.minecraft.net"},
    {"forge", "Forge maven", "https://maven.minecraftforge.net"},
    {"neoforge", "NeoForge maven", "https://maven.neoforged.net/releases"},
    {"fabric", "Fabric meta", "https://meta.fabricmc.net"},
};

const DefaultSource* FindDefault(const std::string& kind) {
    for (const auto& item : kDefaults) {
        if (kind == item.kind) return &item;
    }
    return nullptr;
}

}  // namespace

Sources::Sources(std::string path) : path_(std::move(path)) { Reload(); }

std::vector<std::string> Sources::Kinds() {
    std::vector<std::string> out;
    for (const auto& item : kDefaults) out.emplace_back(item.kind);
    return out;
}

bool Sources::KnownKind(const std::string& kind) { return FindDefault(kind) != nullptr; }

std::string Sources::DefaultUrl(const std::string& kind) {
    const DefaultSource* item = FindDefault(kind);
    return item != nullptr ? item->url : std::string();
}

void Sources::ResetDefaults() {
    records_.clear();
    for (const auto& item : kDefaults) {
        records_.push_back(Record{item.kind, item.name, item.url});
    }
}

void Sources::Reload() {
    records_.clear();

    const std::string text = Directory::ReadTextFile(path_);
    Json::Value root;
    Json json;
    std::string parse_error;
    if (text.empty() || !json.Parse(text, root, parse_error)) {
        ResetDefaults();
        return;
    }

    const Json::Value& list = root["sources"];
    if (!list.IsArray()) {
        ResetDefaults();
        return;
    }

    for (size_t i = 0; i < list.Size(); ++i) {
        const Json::Value& item = list.At(i);
        Record record;
        record.kind = item["kind"].AsString();
        record.name = item["name"].AsString();
        record.url = item["url"].AsString();
        // 坏记录直接丢掉；剩下的全部为空时也回退默认源
        if (record.kind.empty() || record.url.empty()) continue;
        records_.push_back(std::move(record));
    }
    if (records_.empty()) ResetDefaults();
}

bool Sources::Save(std::string& error) const {
    if (!tools::EnsureDir(Directory::DataRoot(), error)) return false;

    Json::Value root = Json::Value::Object();
    Json::Value list = Json::Value::Array();
    for (const auto& record : records_) {
        Json::Value item = Json::Value::Object();
        item.Set("kind", Json::Value::String(record.kind));
        item.Set("name", Json::Value::String(record.name));
        item.Set("url", Json::Value::String(record.url));
        list.Push(std::move(item));
    }
    root.Set("sources", std::move(list));

    if (!Directory::WriteTextFile(path_, root.Dump())) {
        error = "sources.json 写入失败: " + path_;
        return false;
    }
    return true;
}

std::string Sources::Url(const std::string& kind) const {
    for (const auto& record : records_) {
        if (record.kind == kind && !record.url.empty()) return record.url;
    }
    return DefaultUrl(kind);
}

void Sources::Add(const std::string& kind, const std::string& name, const std::string& url) {
    records_.push_back(Record{kind, name, url});
}

void Sources::Update(size_t index, const std::string& name, const std::string& url) {
    if (index >= records_.size()) return;
    records_[index].name = name;
    records_[index].url = url;
}

void Sources::Remove(size_t index) {
    if (index >= records_.size()) return;
    records_.erase(records_.begin() + static_cast<std::vector<Record>::difference_type>(index));
}

}  // namespace versionsource
}  // namespace download
