// 接口：内容源。Modrinth / CurseForge 都从这里长出来。
// 只回答“有什么”，不碰磁盘、不发下载。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class ContentSource {
public:
    struct Project {
        std::string id;
        std::string slug;
        std::string title;
        std::string description;
        int64_t downloads = 0;
    };

    struct File {
        std::string url;
        std::string filename;
        std::string sha1;
        int64_t size = 0;
    };

    struct Version {
        std::string id;
        std::string name;
        std::string version_number;
        std::vector<std::string> game_versions;
        std::vector<std::string> loaders;
        std::vector<std::string> required_projects;
        std::vector<File> files;
    };

    virtual ~ContentSource() = default;

    virtual std::string Id() const = 0;           // "modrinth" / "curseforge"
    virtual std::string DisplayName() const = 0;  // 界面上显示的名字

    // 没配好（比如缺 API key）时给出原因，界面据此禁用
    virtual bool Ready(std::string& reason) const = 0;

    virtual bool Search(const std::string& query, const std::string& loader,
                        const std::string& mc_version, std::vector<Project>& out,
                        std::string& error) = 0;

    virtual bool Versions(const std::string& project_id, const std::string& loader,
                          const std::string& mc_version, std::vector<Version>& out,
                          std::string& error) = 0;
};
