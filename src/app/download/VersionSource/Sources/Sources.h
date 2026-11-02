// 软件源记录：用户可以增删改，每条记录是一个下载源。
// 同一 kind 有多条时按列表顺序取第一条；没有配置就回退默认官方地址。
#pragma once

#include <string>
#include <vector>

namespace download {
namespace versionsource {

class Sources {
public:
    struct Record {
        std::string kind;  // mojang / assets / forge / neoforge / fabric
        std::string name;
        std::string url;
    };

    explicit Sources(std::string path);

    void Reload();
    bool Save(std::string& error) const;

    const std::vector<Record>& Records() const { return records_; }

    // 没配就回退默认
    std::string Url(const std::string& kind) const;
    std::string ManifestUrl() const { return Url("mojang"); }
    std::string AssetsRoot() const { return Url("assets"); }
    std::string ForgeRoot() const { return Url("forge"); }
    std::string NeoForgeRoot() const { return Url("neoforge"); }
    std::string FabricRoot() const { return Url("fabric"); }

    static std::string DefaultUrl(const std::string& kind);
    static std::string DefaultName(const std::string& kind);
    static bool KnownKind(const std::string& kind);
    static std::vector<std::string> Kinds();

    void Add(const std::string& kind, const std::string& name, const std::string& url);
    void Update(size_t index, const std::string& name, const std::string& url);
    void Remove(size_t index);
    void ResetDefaults();

private:
    std::string path_;
    std::vector<Record> records_;
};

}  // namespace versionsource
}  // namespace download
