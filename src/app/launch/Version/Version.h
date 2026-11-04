// 组件：版本管理。初始化时探测，只对选中的版本做完整解析。
#pragma once

#include <map>
#include <string>
#include <vector>

#include "app/directory/Directory.h"
#include "app/tools/Json.h"

namespace launch {

class Version {
public:
    using Entry = Directory::VersionEntry;

    struct Classifier {
        std::string path;  // 相对 libraries/
        std::string url;
        std::string sha1;
        int64_t size = 0;
    };

    struct Library {
        std::string name;
        std::string path;  // 相对 libraries/，可能为空（需要按 name 推导）
        std::string url;
        std::string sha1;
        int64_t size = 0;
        Json::Value rules;
        std::map<std::string, std::string> natives;  // os -> classifier 模板
        std::map<std::string, Classifier> classifiers;
    };

    struct Detail {
        std::string id;
        std::string main_class;
        std::string assets;
        std::string asset_index;
        int java_major = 8;
        std::string client_jar;  // 绝对路径，继承时来自父版本
        std::string client_url;
        std::string client_sha1;
        int64_t client_size = 0;
        std::string asset_index_url;
        std::string asset_index_sha1;
        std::vector<Library> libraries;
        Json::Value arguments;
        bool has_arguments = false;
        std::string legacy_arguments;
    };

    explicit Version(std::string game_dir);

    const std::string& GameDir() const { return paths_.GameRoot(); }
    const Directory& Paths() const { return paths_; }
    const std::vector<Entry>& Entries() const { return paths_.VisibleVersions(); }

    bool Select(const std::string& id);

    // 改名：写进版本的 rml.json，与目录名解耦
    bool Rename(const std::string& id, const std::string& name);
    const std::string& Selected() const { return selected_; }
    // 选中入口引用的共享核心；没有 core 时退回入口自身。
    std::string SelectedCore() const;
    bool HasSelection() const { return !selected_.empty(); }

    // 重新扫描 versions/（装完新版本后调用），保留仍然存在的选择
    void Refresh();

    // 只解析选中版本。继承链在这里合并。
    bool LoadDetail(Detail& out, std::string& error) const;

    std::string VersionDir(const std::string& id) const;

private:
    bool LoadOne(const std::string& id, Detail& out, std::string& error, int depth) const;

    Directory paths_;
    std::string selected_;
};

}  // namespace launch
