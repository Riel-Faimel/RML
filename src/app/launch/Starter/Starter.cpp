#include "app/launch/Starter/Starter.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <set>

#include "app/tools/tools.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace launch {

namespace {

#ifdef _WIN32
constexpr char kClasspathSeparator = ';';
constexpr const char* kOsName = "windows";
#else
constexpr char kClasspathSeparator = ':';
constexpr const char* kOsName = "linux";
#endif

bool MatchesRules(const Json::Value& rules, const std::set<std::string>& features) {
    if (!rules.IsArray() || rules.Size() == 0) return true;
    bool allowed = false;
    for (size_t i = 0; i < rules.Size(); ++i) {
        const Json::Value& rule = rules.At(i);

        const Json::Value& os = rule["os"];
        if (os.IsObject()) {
            std::string name = os["name"].AsString();
            if (!name.empty() && name != kOsName) continue;
            std::string arch = os["arch"].AsString();
            if (!arch.empty() && arch != "x86_64") continue;
        }

        // features 里声明的每个开关都必须与当前启动方式一致，否则这条规则不匹配
        const Json::Value& rule_features = rule["features"];
        if (rule_features.IsObject()) {
            bool all_match = true;
            for (size_t k = 0; k < rule_features.Size(); ++k) {
                const std::string& key = rule_features.KeyAt(k);
                bool wanted = rule_features.At(k).AsBool();
                if (features.count(key) != static_cast<size_t>(wanted)) {
                    all_match = false;
                    break;
                }
            }
            if (!all_match) continue;
        }

        allowed = rule["action"].AsString() == "allow";
    }
    return allowed;
}

std::string MavenToPath(const std::string& name) {
    std::string text = name;
    std::string extension = "jar";
    size_t at = text.find('@');
    if (at != std::string::npos) {
        extension = text.substr(at + 1);
        text.resize(at);
    }
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t pos = text.find(':', start);
        if (pos == std::string::npos) {
            parts.push_back(text.substr(start));
            break;
        }
        parts.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    if (parts.size() < 3) return {};
    std::string group = parts[0];
    for (char& c : group) {
        if (c == '.') c = '/';
    }
    std::string file = parts[1] + "-" + parts[2];
    if (parts.size() >= 4) file += "-" + parts[3];
    file += "." + extension;
    return group + "/" + parts[1] + "/" + parts[2] + "/" + file;
}

std::string Substitute(std::string_view text, const std::map<std::string, std::string>& variables) {
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '$' && i + 1 < text.size() && text[i + 1] == '{') {
            size_t end = text.find('}', i + 2);
            if (end != std::string_view::npos) {
                auto found = variables.find(std::string(text.substr(i + 2, end - i - 2)));
                if (found != variables.end()) {
                    out += found->second;
                    i = end + 1;
                    continue;
                }
            }
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

void Expand(const Json::Value& entries, const std::map<std::string, std::string>& variables,
            const std::set<std::string>& features, std::vector<std::string>& out) {
    if (!entries.IsArray()) return;
    for (size_t i = 0; i < entries.Size(); ++i) {
        const Json::Value& entry = entries.At(i);
        if (entry.IsString()) {
            out.push_back(Substitute(entry.AsString(), variables));
            continue;
        }
        if (!entry.IsObject()) continue;
        if (!MatchesRules(entry["rules"], features)) continue;
        const Json::Value& value = entry["value"];
        if (value.IsString()) {
            out.push_back(Substitute(value.AsString(), variables));
        } else if (value.IsArray()) {
            for (size_t k = 0; k < value.Size(); ++k) {
                out.push_back(Substitute(value.At(k).AsString(), variables));
            }
        }
    }
}

std::vector<std::string> SplitCommandLine(std::string_view text) {
    std::vector<std::string> parts;
    std::string current;
    bool in_quotes = false;
    for (char c : text) {
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ' ' && !in_quotes) {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

std::string Quote(const std::string& text) {
    if (text.find(' ') == std::string::npos && text.find('\t') == std::string::npos) return text;
    return "\"" + text + "\"";
}

}  // namespace

std::string Starter::Command::preview() const {
    std::string out = Quote(java);
    for (const auto& arg : args) {
        out += " ";
        out += Quote(arg);
    }
    return out;
}

bool Starter::Build(const AccountProvider::Credentials& credentials, const Options& options,
                    Command& out, std::vector<Missing>& missing, std::string& error) const {
    Version::Detail detail;
    if (!version_.LoadDetail(detail, error)) return false;

    const std::string game_dir = version_.GameDir();
    const std::string version_dir = version_.VersionDir(version_.Selected());

    Command command;
    command.working_dir = game_dir;

    // —— Java ——
    const Environment::Java* java = nullptr;
    if (!options.java_path.empty()) java = environment_.Find(options.java_path);
    if (java == nullptr) java = environment_.Pick(detail.java_major);

    if (java == nullptr) {
        missing.push_back({"没有可用的 Java", "需要 Java " + std::to_string(detail.java_major)});
        error = "没有可用的 Java";
        return false;
    }
    if (java->major < detail.java_major) {
        missing.push_back({"Java 版本过低",
                           "需要 Java " + std::to_string(detail.java_major) + "，可用最高为 " +
                               std::to_string(java->major)});
    }
    command.java = java->path;

    // —— natives ——
    // 1.19 起 LWJGL natives 直接进 classpath，不再解压；没有 natives 声明的版本不该要求这个目录
    bool needs_natives = false;
    for (const auto& library : detail.libraries) {
        if (library.natives.find("windows") != library.natives.end()) {
            needs_natives = true;
            break;
        }
    }

    const std::string natives_dir = tools::JoinPath(version_dir, version_.Selected() + "-natives");
    if (needs_natives) {
        const bool ready = tools::IsDirectory(natives_dir) && !tools::ListFiles(natives_dir).empty();
        if (!ready) missing.push_back({"natives 未解压", natives_dir});
    } else {
        std::string dir_error;
        tools::EnsureDir(natives_dir, dir_error);  // 参数里还要用，先建个空目录
    }
    // —— 本次启动启用的 features：rules 靠它决定参数要不要带上 ——
    std::set<std::string> features;
    features.insert("has_custom_resolution");
    if (!options.quick_play.empty()) features.insert("is_quick_play_multiplayer");

    // —— classpath ——
    std::vector<std::string> classpath;
    const std::string libraries_root = tools::JoinPath(game_dir, "libraries");
    int missing_libraries = 0;
    for (const auto& library : detail.libraries) {
        if (!MatchesRules(library.rules, features)) continue;
        std::string relative = library.path.empty() ? MavenToPath(library.name) : library.path;
        if (relative.empty()) continue;
        std::string full = tools::JoinPath(libraries_root, relative);
        if (!tools::PathExists(full)) {
            ++missing_libraries;
            continue;
        }
        classpath.push_back(full);
    }
    if (missing_libraries > 0) {
        missing.push_back({"缺少 " + std::to_string(missing_libraries) + " 个库文件",
                           tools::JoinPath(game_dir, "libraries")});
    }

    if (!detail.client_jar.empty() && tools::PathExists(detail.client_jar)) {
        classpath.push_back(detail.client_jar);
    } else {
        missing.push_back({"缺少客户端 jar", tools::JoinPath(version_dir, version_.Selected() + ".jar")});
    }

    if (!missing.empty()) {
        error = "启动所需文件不完整";
        return false;
    }

    // —— 占位符 ——
    std::string classpath_text;
    for (size_t i = 0; i < classpath.size(); ++i) {
        if (i) classpath_text.push_back(kClasspathSeparator);
        classpath_text += classpath[i];
    }

    std::map<std::string, std::string> variables;
    variables["auth_player_name"] = credentials.username;
    variables["auth_uuid"] = credentials.uuid;
    variables["auth_access_token"] = credentials.access_token;
    variables["auth_session"] = credentials.access_token;
    variables["user_type"] = credentials.user_type.empty() ? "legacy" : credentials.user_type;
    variables["version_name"] = detail.id;
    variables["version_type"] = "release";
    variables["game_directory"] = game_dir;
    variables["assets_root"] = tools::JoinPath(game_dir, "assets");
    variables["assets_index_name"] = detail.asset_index;
    variables["natives_directory"] = natives_dir;
    variables["launcher_name"] = options.launcher_name;
    variables["launcher_version"] = options.launcher_version;
    variables["classpath"] = classpath_text;
    variables["classpath_separator"] = std::string(1, kClasspathSeparator);
    variables["resolution_width"] = std::to_string(options.width);
    variables["resolution_height"] = std::to_string(options.height);
    variables["quickPlayMultiplayer"] = options.quick_play;
    variables["clientid"] = "";
    variables["auth_xuid"] = "";
    variables["xuid"] = "";

    if (detail.has_arguments) {
        Expand(detail.arguments["jvm"], variables, features, command.args);
    } else {
        command.args.push_back("-Djava.library.path=" + natives_dir);
        command.args.push_back("-cp");
        command.args.push_back(classpath_text);
    }

    command.args.push_back("-Xmx" + std::to_string(options.memory_mb) + "M");
    command.args.push_back(detail.main_class);

    if (detail.has_arguments) {
        Expand(detail.arguments["game"], variables, features, command.args);
    } else {
        for (const auto& arg : SplitCommandLine(detail.legacy_arguments)) {
            command.args.push_back(Substitute(arg, variables));
        }
    }

    out = std::move(command);
    return true;
}

bool Starter::Start(const Command& command, std::string& error) {
#ifdef _WIN32
    std::string command_line = command.preview();

    const std::string log_dir = tools::JoinPath(command.working_dir, "logs");
    std::string log_path = tools::JoinPath(log_dir, "rml-launch.log");

    std::string log_dir_error;
    if (!tools::EnsureDir(log_dir, log_dir_error)) {
        error = "无法创建日志目录: " + log_dir_error;
        return false;
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE log_file = CreateFileA(log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &attributes,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log_file == INVALID_HANDLE_VALUE) {
        error = "无法创建日志文件: " + log_path;
        return false;
    }

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = log_file;
    startup.hStdError = log_file;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION info{};
    std::vector<char> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back('\0');

    BOOL created = CreateProcessA(nullptr, mutable_command.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr,
                                  command.working_dir.empty() ? nullptr : command.working_dir.c_str(),
                                  &startup, &info);
    CloseHandle(log_file);

    if (!created) {
        error = "CreateProcess 失败，错误码 " + std::to_string(GetLastError());
        return false;
    }

    CloseHandle(info.hThread);
    process_ = info.hProcess;
    return true;
#else
    (void)command;
    error = "当前平台不支持启动";
    return false;
#endif
}

bool Starter::IsRunning() const {
#ifdef _WIN32
    if (process_ == nullptr) return false;
    return WaitForSingleObject(static_cast<HANDLE>(process_), 0) == WAIT_TIMEOUT;
#else
    return false;
#endif
}

int Starter::ExitCode() const {
#ifdef _WIN32
    if (process_ == nullptr) return -1;
    DWORD code = 0;
    if (!GetExitCodeProcess(static_cast<HANDLE>(process_), &code)) return -1;
    return static_cast<int>(code);
#else
    return -1;
#endif
}

}  // namespace launch
