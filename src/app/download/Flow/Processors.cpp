#include "app/download/Flow/Processors.h"

#include <filesystem>
#include <map>
#include <set>
#include <vector>

#include "app/download/Flow/Support.h"
#include "app/tools/tools.h"

namespace download {
namespace flow {

namespace {

std::string StripBrackets(const std::string& text) {
    if (text.size() >= 2 && text.front() == '[' && text.back() == ']') {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

bool IsBracketed(const std::string& text) {
    return text.size() >= 2 && text.front() == '[' && text.back() == ']';
}

bool ShaMatches(const std::string& path, const std::string& sha1) {
    if (sha1.empty()) return true;
    std::string actual;
    if (!tools::Sha1File(path, actual)) return false;
    return tools::ToLower(actual) == tools::ToLower(sha1);
}

std::string LogTail(const std::string& path) {
    std::string text = tools::ReadTextFile(path);
    if (text.size() > 1200) text = text.substr(text.size() - 1200);
    return text;
}

bool ClientSide(const Json::Value& processor) {
    const Json::Value& sides = processor["sides"];
    if (!sides.IsArray() || sides.Size() == 0) return true;
    for (size_t i = 0; i < sides.Size(); ++i) {
        if (sides.At(i).AsString() == "client") return true;
    }
    return false;
}

bool OutputsValid(const std::vector<std::pair<std::string, std::string>>& outputs) {
    for (const auto& output : outputs) {
        if (!tools::PathExists(output.first)) return false;
        if (!ShaMatches(output.first, output.second)) {
            std::error_code ignored;
            std::filesystem::remove(tools::ToPath(output.first), ignored);
            return false;
        }
    }
    return true;
}

}  // namespace

bool Processors::DownloadLibraries(const std::string& libraries_root, const Json::Value& profile,
                                   const Progress& progress, std::string& error) const {
    const Json::Value& libraries = profile["libraries"];
    std::vector<installer::Transfer::Task> tasks;
    std::set<std::string> seen;

    for (size_t i = 0; i < libraries.Size(); ++i) {
        const Json::Value& library = libraries.At(i);
        const Json::Value& artifact = library["downloads"]["artifact"];
        std::string relative = artifact["path"].AsString();
        const std::string url = artifact["url"].AsString();
        if (relative.empty()) relative = Support::MavenPath(library["name"].AsString());
        if (relative.empty() || url.empty()) continue;

        const std::string dest = tools::JoinPath(libraries_root, relative);
        if (!seen.insert(dest).second) continue;

        installer::Transfer::Task task;
        task.url = url;
        task.sha1 = artifact["sha1"].AsString();
        task.dest = dest;
        task.label = relative;
        task.size = artifact["size"].AsInt(0);
        tasks.push_back(std::move(task));
    }

    installer::Transfer::Result result;
    return transfer_.Run(tasks, progress, result, error);
}

bool Processors::Run(const std::string& java, const std::string& game_dir,
                     const std::string& installer_jar, const std::string& mc_version,
                     const std::string& work_dir, const Json::Value& profile,
                     const Progress& progress, std::string& error) const {
    const std::string libraries_root = tools::JoinPath(game_dir, "libraries");
    std::map<std::string, std::string> tokens;

    // —— data：工件路径 / 字面量 / 安装器里的文件 ——
    const Json::Value& data = profile["data"];
    for (size_t i = 0; i < data.Size(); ++i) {
        const std::string key = data.KeyAt(i);
        const std::string value = data.At(i)["client"].AsString();
        if (value.empty()) continue;

        if (IsBracketed(value)) {
            const std::string path = Support::ResolveArtifact(libraries_root, StripBrackets(value));
            tokens[key] = path;
            if (!path.empty()) {
                const size_t slash = path.find_last_of('/');
                if (slash != std::string::npos) {
                    std::string dir_error;
                    if (!tools::EnsureDir(path.substr(0, slash), dir_error)) {
                        error = dir_error;
                        return false;
                    }
                }
            }
            continue;
        }
        if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
            tokens[key] = value.substr(1, value.size() - 2);
            continue;
        }

        std::string entry = value;
        if (!entry.empty() && entry.front() == '/') entry.erase(entry.begin());
        std::string extracted;
        if (!tools::ExtractEntry(installer_jar, entry, tools::JoinPath(work_dir, "data"), extracted,
                                error)) {
            error = "安装器里的数据文件取不出来: " + value;
            return false;
        }
        tokens[key] = extracted;
    }

    tokens["SIDE"] = "client";
    tokens["MINECRAFT_JAR"] = tools::JoinPath(
        tools::JoinPath(tools::JoinPath(game_dir, "versions"), mc_version), mc_version + ".jar");
    tokens["MINECRAFT_VERSION"] = mc_version;
    tokens["ROOT"] = game_dir;
    tokens["INSTALLER"] = installer_jar;
    tokens["LIBRARY_DIR"] = libraries_root;

    // —— processors：先算有多少要跑（进度分母） ——
    const Json::Value& processors = profile["processors"];
    int total = 0;
    for (size_t i = 0; i < processors.Size(); ++i) {
        if (ClientSide(processors.At(i))) ++total;
    }

    int index = 0;
    for (size_t i = 0; i < processors.Size(); ++i) {
        const Json::Value& processor = processors.At(i);
        if (!ClientSide(processor)) continue;
        ++index;

        // outputs：已经跑过且校验通过就跳过（重装时省很多时间）
        std::vector<std::pair<std::string, std::string>> outputs;
        const Json::Value& output_map = processor["outputs"];
        for (size_t k = 0; k < output_map.Size(); ++k) {
            const std::string key = output_map.KeyAt(k);
            std::string file;
            if (IsBracketed(key)) {
                file = Support::ResolveArtifact(libraries_root, StripBrackets(key));
            } else if (!Support::ReplaceTokens(tokens, key, file, error)) {
                return false;
            }

            std::string sha;
            const std::string value = output_map.At(k).AsString();
            if (!value.empty() && !Support::ReplaceTokens(tokens, value, sha, error)) return false;
            outputs.emplace_back(file, sha);
        }
        if (!outputs.empty() && OutputsValid(outputs)) {
            if (progress) progress("处理器已就绪 " + std::to_string(index) + "/" + std::to_string(total), index, total);
            continue;
        }

        // 处理器只负责写文件，不保证父目录存在
        for (const auto& output : outputs) {
            const size_t slash = output.first.find_last_of('/');
            if (slash == std::string::npos) continue;
            std::string dir_error;
            if (!tools::EnsureDir(output.first.substr(0, slash), dir_error)) {
                error = dir_error;
                return false;
            }
        }

        const std::string jar_relative = Support::MavenPath(StripBrackets(processor["jar"].AsString()));
        if (jar_relative.empty()) {
            error = "处理器缺少 jar 坐标";
            return false;
        }
        const std::string jar_path = tools::JoinPath(libraries_root, jar_relative);
        if (!tools::PathExists(jar_path)) {
            error = "处理器 jar 不存在: " + jar_path;
            return false;
        }

        std::string main_class;
        if (!Support::ReadMainClass(jar_path,
                                    tools::JoinPath(work_dir, "manifest-" + std::to_string(index)),
                                    main_class, error)) {
            return false;
        }

        std::vector<std::string> classpath{jar_path};
        const Json::Value& deps = processor["classpath"];
        for (size_t k = 0; k < deps.Size(); ++k) {
            const std::string dep = Support::ResolveArtifact(libraries_root, deps.At(k).AsString());
            if (dep.empty() || !tools::PathExists(dep)) {
                error = "处理器依赖缺失: " + deps.At(k).AsString();
                return false;
            }
            classpath.push_back(dep);
        }

        std::string classpath_text;
        for (size_t k = 0; k < classpath.size(); ++k) {
            if (k) classpath_text.push_back(';');
            classpath_text += classpath[k];
        }

        std::vector<std::string> command{java, "-cp", classpath_text, main_class};
        const Json::Value& args = processor["args"];
        for (size_t k = 0; k < args.Size(); ++k) {
            const std::string arg = args.At(k).AsString();
            if (IsBracketed(arg)) {
                command.push_back(Support::ResolveArtifact(libraries_root, StripBrackets(arg)));
                continue;
            }
            std::string expanded;
            if (!Support::ReplaceTokens(tokens, arg, expanded, error)) return false;
            command.push_back(expanded);
        }

        if (progress) {
            progress("处理器 " + std::to_string(index) + "/" + std::to_string(total) + "  " + main_class,
                     index, total);
        }

        const std::string log_path =
            tools::JoinPath(work_dir, "processor-" + std::to_string(index) + ".log");
        std::string run_error;
        if (!tools::RunAndWait(command, log_path, run_error)) {
            error = "处理器 " + main_class + " 失败：" + run_error + "\n" + LogTail(log_path);
            return false;
        }

        for (const auto& output : outputs) {
            if (!tools::PathExists(output.first)) {
                error = "处理器没有产出 " + output.first + "\n" + LogTail(log_path);
                return false;
            }
            if (!ShaMatches(output.first, output.second)) {
                error = "处理器产出的文件校验不通过: " + output.first + "\n" + LogTail(log_path);
                return false;
            }
        }
    }

    // 没有 outputs 的安装器（NeoForge）只能确认最终产物在
    {
        auto patched = tokens.find("PATCHED");
        if (patched != tokens.end() && !patched->second.empty() &&
            !tools::PathExists(patched->second)) {
            error = "处理器跑完但没有生成补丁 jar: " + patched->second;
            return false;
        }
    }

    return true;
}

}  // namespace flow
}  // namespace download
