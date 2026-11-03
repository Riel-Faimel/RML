// 安装器：版本安装。原版和加载器共用同一套「建目录、写文件、下资源」的逻辑。
#pragma once

#include <string>
#include <vector>

#include "app/download/Installer/Transfer/Transfer.h"
#include "app/launch/Version/Version.h"

namespace download {
namespace installer {

class Version {
public:
    explicit Version(const launch::Version& version) : version_(version) {}

    // 只算缺什么，不发请求
    bool Inspect(std::vector<std::string>& missing, std::string& error) const;

    // 缺什么下什么；已有且校验通过的直接跳过
    // apply_defaults：新装一个版本时为 true（会写 options.txt 默认中文），补齐时为 false
    // assets_root：资源对象根地址（软件源可配），默认 Mojang 官方
    bool Install(const Transfer& transfer, const Transfer::Progress& progress,
                 Transfer::Result& result, std::string& error, bool apply_defaults = false,
                 std::string assets_root = "https://resources.download.minecraft.net") const;

    // 把一个版本的 json 落到 versions/<id>/<id>.json（装新版本的第一步）
    static bool WriteVersionJson(const std::string& game_dir, const std::string& id,
                                 const std::string& json, std::string& error);

    // 装完之后写默认设置：目前只有语言 -> 简体中文
    static void ApplyDefaults(const std::string& game_dir);

    // 写托管标记 rml.json：complete=false 表示装到一半。
    // write_user=true 时写入 user 标记，区分“用户自己装的原版”和 Forge 依赖的底座。
    static bool WriteMarker(const std::string& game_dir, const std::string& id,
                            const std::string& loader, bool complete,
                            bool user_installed = false, bool write_user = false);

private:
    bool BuildCoreTasks(const launch::Version::Detail& detail,
                        std::vector<Transfer::Task>& out, std::string& error) const;
    bool BuildAssetTasks(const launch::Version::Detail& detail, const std::string& assets_root,
                         std::vector<Transfer::Task>& out, std::string& error) const;
    bool ExtractNatives(const launch::Version::Detail& detail, std::string& error) const;

    const launch::Version& version_;
};

}  // namespace installer
}  // namespace download
