// 装配检查：不开窗口，读 %APPDATA%\RML\settings.json，验证能不能组装出命令行。
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "app/launch/Launch.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"
#include "include/AccountProvider.h"

namespace {

std::string SettingsPath() {
    const char* appdata = std::getenv("APPDATA");
    if (appdata == nullptr) return {};
    return std::string(appdata) + "\\RML\\settings.json";
}

}  // namespace

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const std::string settings_path = SettingsPath();
    const std::string text = tools::ReadTextFile(settings_path);

    std::string game_dir;
    std::string selected;

    if (!text.empty()) {
        Json::Value root;
        Json json;
        std::string error;
        if (json.Parse(text, root, error)) {
            game_dir = root["gameDir"].AsString();
            selected = root["selectedVersion"].AsString();
        } else {
            std::printf("settings.json 解析失败: %s\n", error.c_str());
        }
    }

    std::printf("配置: %s\n", settings_path.c_str());
    if (game_dir.empty()) {
        std::printf("settings.json 里没有 gameDir\n");
        return 2;
    }

    Launch launch(game_dir);
    const auto& entries = launch.Versions().Entries();
    std::printf("游戏目录: %s\n版本数: %zu\n", game_dir.c_str(), entries.size());
    for (const auto& entry : entries) {
        std::printf("  %-32s json=%s jar=%s\n", entry.id.c_str(), entry.has_json ? "y" : "n",
                    entry.has_jar ? "y" : "n");
    }

    std::printf("\nJava:\n");
    for (const auto& java : launch.Tools().Javas()) {
        std::printf("  %-60s major=%d source=%s\n", java.path.c_str(), java.major, java.source.c_str());
    }

    if (!selected.empty() && launch.Versions().Select(selected)) {
        std::printf("\n选中: %s\n", selected.c_str());
    } else if (!entries.empty()) {
        launch.Versions().Select(entries.front().id);
        std::printf("\n默认选中: %s\n", entries.front().id.c_str());
    }

    launch::Version::Detail detail;
    std::string error;
    if (!launch.Versions().LoadDetail(detail, error)) {
        std::printf("解析失败: %s\n", error.c_str());
        return 1;
    }
    std::printf("主类: %s\n需要 Java: %d\n库: %zu 个\n客户端 jar: %s\n",
                detail.main_class.c_str(), detail.java_major, detail.libraries.size(),
                detail.client_jar.c_str());

    AccountProvider::Credentials credentials;
    credentials.username = "Player";
    credentials.uuid = "00000000-0000-0000-0000-000000000000";
    credentials.access_token = "0";
    credentials.user_type = "legacy";

    launch::Starter::Command command;
    std::vector<launch::Starter::Missing> missing;
    const bool built = launch.GameStarter().Build(credentials, {}, command, missing, error);

    std::printf("\n装配: %s\n", built ? "成功" : "失败");
    if (!error.empty()) std::printf("错误: %s\n", error.c_str());
    for (const auto& item : missing) {
        std::printf("  缺: %s  (%s)\n", item.what.c_str(), item.hint.c_str());
    }
    if (built) std::printf("命令: %s\n", command.preview().c_str());

    // 补全检查：跑一遍安装器（文件齐时全部跳过，不发请求）
    std::vector<std::string> missing_files;
    std::string inspect_error;
    directory::installer::Transfer::Result report;
    auto& installer = launch.Downloader().Installers();
    const bool inspected = installer.Versions().Install(
        installer.Transfers(), [](const std::string&, int, int) {}, report, inspect_error);
    std::printf("\n补全: %s  下载 %d  跳过 %d  失败 %d\n", inspected ? "成功" : "失败",
                report.downloaded, report.skipped, report.failed);
    if (!inspect_error.empty()) std::printf("  错误: %s\n", inspect_error.c_str());

    return (built && inspected) ? 0 : 1;
}
