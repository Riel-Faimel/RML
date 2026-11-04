// 核心/入口分离、引用与 GC、服务端实例 manifest 的临时目录测试。
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <windows.h>

#include "app/directory/Directory.h"
#include "app/json/JsonArena.h"
#include "app/launch/Version/Version.h"
#include "app/server/Catalog/Catalog.h"
#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace {

namespace fs = std::filesystem;

int failures = 0;

void Check(bool ok, const std::string& what) {
    if (ok) {
        std::printf("  ok   %s\n", what.c_str());
        return;
    }
    std::printf("  FAIL %s\n", what.c_str());
    ++failures;
}

std::string Read(const std::string& path) { return tools::ReadTextFile(path); }

bool Contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

void Ensure(const std::string& path) {
    std::string error;
    tools::EnsureDir(path, error);
}

void Write(const std::string& path, const std::string& text) {
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) Ensure(path.substr(0, slash));
    tools::WriteTextFile(path, text);
}

void Marker(const std::string& path, const std::string& id, const std::string& role,
            const std::string& loader, const std::string& minecraft, const std::string& base,
            bool server) {
    auto j = JsonArena::Instance().Open(path);
    j["schema"] = Json::Value::Int(2);
    j["id"] = Json::Value::String(id);
    j["name"] = Json::Value::String(id);
    j["role"] = Json::Value::String(role);
    j["loader"] = Json::Value::String(loader);
    j["loaderVersion"] = Json::Value::String("");
    j["minecraft"] = Json::Value::String(minecraft);
    j["base"] = Json::Value::String(base);
    j["complete"] = Json::Value::Bool(true);
    j["userInstalled"] = Json::Value::Bool(true);
    j["state"] = Json::Value::String("committed");
    j["fresh"] = Json::Value::Bool(false);
    if (server) {
        j["kind"] = Json::Value::String("server");
        j["serverJar"] = Json::Value::String("server.jar");
        j["javaMajor"] = Json::Value::Int(17);
    }
    std::string error;
    if (!j.commit(error)) std::printf("  写 marker 失败: %s\n", error.c_str());
}

std::string CoreOf(const std::string& marker) {
    auto j = JsonArena::Instance().Open(marker);
    return j.Get("core").AsString();
}

bool HasManifestKind(const std::string& path) {
    auto j = JsonArena::Instance().Open(path);
    return j && j.Get("kind").AsString() == "serverInstance";
}

}  // namespace

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const std::string root =
        "build/cores-test-" + std::to_string(static_cast<long long>(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(tools::ToPath(root), ec);

    const std::string appdata = root + "/appdata";
    const std::string game = root + "/game";
    _putenv_s("APPDATA", appdata.c_str());
    Directory paths(game);
    Ensure(paths.VersionsRoot());

    // —— 原版入口：核心文件 + 用户私有数据 ——
    const std::string vanilla = paths.VersionDir("1.20.1");
    const std::string vanilla_json =
        "{\"id\":\"1.20.1\",\"mainClass\":\"net.minecraft.client.main.Main\",\"assets\":\"1.20\","
        "\"javaVersion\":{\"majorVersion\":17},\"downloads\":{\"client\":{\"url\":\"x\",\"sha1\":\"y\"}}}";
    Write(vanilla + "/1.20.1.json", vanilla_json);
    Write(vanilla + "/1.20.1.jar", "vanilla-jar");
    Write(vanilla + "/1.20.1-natives/lwjgl.dll", "dll");
    Write(vanilla + "/saves/world/level.dat", "level");
    Write(vanilla + "/mods/user.jar", "user-mod");
    Write(vanilla + "/options.txt", "lang:zh_cn\n");
    Marker(vanilla + "/rml.json", "1.20.1", "game", "原版", "1.20.1", "", false);

    // —— 加载器入口：保留自己的 profile，继承文件应搬走/去重 ——
    const std::string loader = paths.VersionDir("1.20.1-forge-47.2.0");
    Write(loader + "/1.20.1-forge-47.2.0.json",
          "{\"id\":\"1.20.1-forge-47.2.0\",\"inheritsFrom\":\"1.20.1\","
          "\"mainClass\":\"cpw.mods.bootstraplauncher.BootstrapLauncher\"}");
    Write(loader + "/1.20.1-forge-47.2.0.jar", "vanilla-jar");
    Write(loader + "/1.20.1-forge-47.2.0-natives/lwjgl.dll", "dll");
    Write(loader + "/mods/forge.jar", "forge-mod");
    Marker(loader + "/rml.json", "1.20.1-forge-47.2.0", "loader", "Forge", "1.20.1", "1.20.1",
           false);

    // —— 服务端安装：server.jar 留在入口，只引用核心 ——
    const std::string server = paths.VersionDir("1.20.1-server");
    Write(server + "/server.jar", "server-jar");
    Marker(server + "/rml.json", "1.20.1-server", "server", "原版", "1.20.1", "", true);

    // —— 服务端实例：manifest + 旧 servers.json 导入 ——
    Write(paths.ServerDir("world1") + "/rml.json",
          "{\"schema\":2,\"kind\":\"serverInstance\",\"id\":\"world1\",\"name\":\"世界一\","
          "\"install\":\"1.20.1-server\",\"runtime\":{\"port\":25566,\"memoryMb\":2048}}");
    Ensure(paths.ServerDir("legacy"));
    Ensure(paths.ServerDir("broken"));
    Write(paths.ServerManifest("broken"), "{not json");
    const std::string legacy_json =
        "{\"instances\":["
        "{\"name\":\"旧实例\",\"dir\":\"legacy\",\"installation\":\"1.20.1-server\",\"port\":25570,"
        "\"pid\":0},"
        "{\"name\":\"断链\",\"dir\":\"missing-dir\",\"installation\":\"1.20.1-server\",\"port\":25571},"
        "{\"name\":\"非法记录\"}]}";
    Ensure(appdata + "/RML");
    Write(appdata + "/RML/servers.json", legacy_json);

    server::Catalog catalog(game);
    catalog.Reload();
    Check(catalog.Instances().size() == 3, "旧实例目录、已有 manifest、坏 manifest 都进入 Catalog");
    Check(HasManifestKind(paths.ServerManifest("legacy")), "无 manifest 的旧实例补写 manifest");
    Check(Read(paths.ServerManifest("broken")) == "{not json", "解析不了的实例 manifest 不被覆盖");
    Check(tools::PathExists(appdata + "/RML/servers.json.legacy.bak"), "旧 servers.json 先备份");
    Check(Read(appdata + "/RML/servers.json") == legacy_json,
          "旧 servers.json 原样保留（含断链/非法）");
    Check(catalog.Installations().size() == 1 && catalog.Installations()[0].refs == 2,
          "install.refs 计算为 2");

    {
        std::string error;
        catalog.Instances()[0].pid = 424242;
        Check(catalog.SaveRunState(error), "写独立 pid 缓存");
        Check(!Contains(Read(paths.ServerManifest("legacy")), "\"pid\""), "manifest 不含 pid");
        Check(Contains(Read(server::Catalog::RunStatePath()), "424242"), "pid 缓存保存 pid");
    }

    // —— dry-run：只出计划，不动文件 ——
    Directory::MigrationReport report;
    paths.MigrateCores(true, report);
    Check(report.ok && report.moved >= 3, "dry-run 生成迁移计划");
    Check(tools::PathExists(vanilla + "/1.20.1.json"), "dry-run 不移动入口 json");
    Check(!tools::IsDirectory(paths.CoreDir("1.20.1")), "dry-run 不创建核心");

    // —— 正式迁移 ——
    paths.MigrateCores(false, report);
    Check(report.ok, "正式迁移成功");
    Check(!tools::PathExists(vanilla + "/1.20.1.json") &&
              !tools::PathExists(vanilla + "/1.20.1.jar"),
          "原版入口不再保留核心 json/jar");
    Check(tools::PathExists(paths.CoreJson("1.20.1")) && tools::PathExists(paths.CoreJar("1.20.1")),
          "核心 json/jar 物理落在 cores");
    Check(tools::IsDirectory(paths.CoreNatives("1.20.1")), "核心 natives 物理落在 cores");
    Check(Read(vanilla + "/saves/world/level.dat") == "level" &&
              Read(vanilla + "/mods/user.jar") == "user-mod" &&
              Read(vanilla + "/options.txt") == "lang:zh_cn\n",
          "存档/模组/options 未受影响");
    Check(CoreOf(vanilla + "/rml.json") == "1.20.1", "原版入口 rml 写 core");
    Check(tools::PathExists(loader + "/1.20.1-forge-47.2.0.json"), "加载器入口保留自己的 profile");
    Check(!tools::PathExists(loader + "/1.20.1-forge-47.2.0.jar") &&
              !tools::IsDirectory(loader + "/1.20.1-forge-47.2.0-natives"),
          "加载器入口的继承 jar/natives 被去重");
    Check(CoreOf(loader + "/rml.json") == "1.20.1", "加载器入口引用父核心");
    Check(Read(loader + "/mods/forge.jar") == "forge-mod", "加载器 mods 未受影响");
    Check(tools::PathExists(server + "/server.jar"), "server.jar 留在服务端安装入口");
    Check(CoreOf(server + "/rml.json") == "1.20.1", "服务端安装引用核心");

    Directory fresh(game);
    Check(fresh.CoreForEntry("1.20.1-forge-47.2.0") == "1.20.1",
          "未扫描的 Directory 也能从 rml 读到 core");

    paths.RefreshVersions();
    bool loader_ok = false;
    for (const auto& entry : paths.Versions()) {
        if (entry.id == "1.20.1-forge-47.2.0") loader_ok = entry.valid && entry.core == "1.20.1";
    }
    Check(loader_ok, "扫描把加载器入口关联到核心");
    const auto* core = paths.FindCore("1.20.1");
    Check(core != nullptr && core->refs == 3, "core.refs = 原版 + 加载器 + 服务端安装");

    launch::Version versions(game);
    Check(versions.Select("1.20.1-forge-47.2.0"), "选中加载器入口");
    launch::Version::Detail detail;
    std::string launch_error;
    Check(versions.LoadDetail(detail, launch_error), "从核心回退解析加载器版本");
    Check(detail.main_class == "cpw.mods.bootstraplauncher.BootstrapLauncher", "加载器主类保留");
    Check(Contains(detail.client_jar, "cores/1.20.1/1.20.1.jar"), "客户端 jar 来自核心");

    std::string remove_error;
    Check(!paths.RemoveCore("1.20.1", remove_error), "有引用的核心拒绝删除");
    Check(Contains(remove_error, "引用"), "拒绝删除时给中文引用错误");

    // —— 回滚 ——
    std::string rollback_report;
    std::string rollback_error;
    Check(paths.RollbackLastMigration(rollback_error, rollback_report), "回滚上次迁移");
    Check(tools::PathExists(vanilla + "/1.20.1.json") &&
              tools::PathExists(vanilla + "/1.20.1.jar"),
          "回滚恢复原版入口 json/jar");
    Check(tools::IsDirectory(vanilla + "/1.20.1-natives"), "回滚恢复入口 natives");
    Check(tools::PathExists(loader + "/1.20.1-forge-47.2.0.jar"), "回滚恢复加载器继承 jar");
    Check(CoreOf(vanilla + "/rml.json").empty() && CoreOf(loader + "/rml.json").empty(),
          "回滚移除入口 core 字段");
    Check(!tools::IsDirectory(paths.CoreDir("1.20.1")), "回滚删除本次新建核心");
    Check(Read(vanilla + "/saves/world/level.dat") == "level", "回滚不动存档");

    // —— 再迁移一次，测 copy-on-write ——
    paths.MigrateCores(false, report);
    Write(vanilla + "/1.20.1.json",
          "{\"id\":\"1.20.1\",\"mainClass\":\"cow.Main\",\"assets\":\"1.20\"}");
    Write(vanilla + "/1.20.1.jar", "new-vanilla-jar");
    paths.MigrateCores(false, report);
    Check(report.ok, "内容变化后再次迁移成功");
    const std::string new_core = CoreOf(loader + "/rml.json");
    Check(new_core.find("1.20.1-cov-") == 0, "核心变化生成新 core id");
    Check(tools::PathExists(paths.CoreJson("1.20.1")), "旧核心仍在（不就地改写）");
    Check(!Contains(Read(paths.CoreJson("1.20.1")), "cow.Main"), "旧核心内容未被覆盖");
    paths.RefreshVersions();
    Check(paths.FindCore(new_core) != nullptr && paths.FindCore(new_core)->refs == 3,
          "新核心被三个入口引用");

    // COW 迁移也必须能回滚：入口文件恢复，旧核心不动。
    std::string cow_report;
    std::string cow_error;
    Check(paths.RollbackLastMigration(cow_error, cow_report), "COW 迁移可回滚");
    Check(tools::PathExists(vanilla + "/1.20.1.json") &&
              tools::PathExists(vanilla + "/1.20.1.jar"),
          "COW 回滚恢复入口文件");
    Check(CoreOf(vanilla + "/rml.json") == "1.20.1" &&
              CoreOf(loader + "/rml.json") == "1.20.1",
          "COW 回滚恢复原 core 引用");
    Check(!tools::IsDirectory(paths.CoreDir(new_core)), "COW 回滚删除新核心");
    Check(tools::PathExists(paths.CoreJson("1.20.1")), "COW 回滚不动旧核心");
    paths.MigrateCores(false, report);

    std::string orphan_error;
    Check(paths.RemoveCore("1.20.1", orphan_error), "旧核心已无引用，可显式清理");
    Check(!paths.RemoveCore(new_core, orphan_error), "仍被引用的新核心不能删");

    // —— 删除入口后 GC ——
    fs::remove_all(tools::ToPath(loader), ec);
    fs::remove_all(tools::ToPath(server), ec);
    fs::remove_all(tools::ToPath(vanilla), ec);
    paths.RefreshVersions();
    const int removed = paths.RemoveOrphanCores(orphan_error);
    Check(removed == 1 && orphan_error.empty(), "清理孤立核心");
    Check(!tools::IsDirectory(paths.CoreDir(new_core)), "孤立核心目录已删除");

    // —— 旧 cores 硬链接副本：入口去重、核心不动、可回滚 ——
    const std::string game2 = root + "/game2";
    Directory paths2(game2);
    Ensure(paths2.VersionsRoot());
    const std::string old_entry = paths2.VersionDir("1.19.4");
    Write(old_entry + "/1.19.4.json", "{\"id\":\"1.19.4\",\"mainClass\":\"old.Main\"}");
    Write(old_entry + "/1.19.4.jar", "old-jar");
    Write(old_entry + "/1.19.4-natives/old.dll", "old-dll");
    Write(old_entry + "/saves/keep.dat", "keep");
    Marker(old_entry + "/rml.json", "1.19.4", "game", "原版", "1.19.4", "", false);
    const std::string old_core_dir = paths2.CoreDir("1.19.4");
    Ensure(old_core_dir);
    {
        auto core_marker = JsonArena::Instance().Open(old_core_dir + "/rml.json");
        core_marker["schema"] = Json::Value::Int(2);
        core_marker["kind"] = Json::Value::String("core");
        core_marker["id"] = Json::Value::String("1.19.4");
        core_marker["complete"] = Json::Value::Bool(true);
        core_marker["state"] = Json::Value::String("committed");
        std::string error;
        core_marker.commit(error);
    }
    Write(old_core_dir + "/1.19.4.json", "{\"id\":\"1.19.4\",\"mainClass\":\"old.Main\"}");
    Write(old_core_dir + "/1.19.4.jar", "old-jar");
    Write(old_core_dir + "/1.19.4-natives/old.dll", "old-dll");

    Directory::MigrationReport report2;
    paths2.MigrateCores(false, report2);
    Check(report2.ok, "旧硬链接副本迁移成功");
    Check(!tools::PathExists(old_entry + "/1.19.4.json") &&
              !tools::PathExists(old_entry + "/1.19.4.jar") &&
              !tools::IsDirectory(old_entry + "/1.19.4-natives"),
          "入口里的重复核心文件被去掉");
    Check(CoreOf(old_entry + "/rml.json") == "1.19.4", "入口 core 指向已有核心");
    Check(Read(old_entry + "/saves/keep.dat") == "keep", "旧副本迁移不动存档");
    std::string old_report;
    std::string old_error;
    Check(paths2.RollbackLastMigration(old_error, old_report), "旧副本迁移可回滚");
    Check(tools::PathExists(old_entry + "/1.19.4.json") &&
              tools::PathExists(old_entry + "/1.19.4.jar") &&
              tools::IsDirectory(old_entry + "/1.19.4-natives"),
          "回滚从核心恢复入口文件");
    Check(Read(paths2.CoreJson("1.19.4")).find("old.Main") != std::string::npos,
          "回滚不破坏已有核心");

    std::printf("\n结果: %s（失败 %d）\n", failures == 0 ? "通过" : "失败", failures);
    fs::remove_all(tools::ToPath(root), ec);
    return failures == 0 ? 0 : 1;
}
