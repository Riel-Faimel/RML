#include "app/download/VersionSource/Forge/Forge.h"

#include <algorithm>
#include <vector>

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace download {
namespace versionsource {

namespace {

constexpr const char* kPromotions =
    "https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json";

std::vector<std::string> XmlVersions(const std::string& xml) {
    std::vector<std::string> out;
    const std::string open = "<version>";
    const std::string close = "</version>";
    size_t pos = 0;
    while (true) {
        const size_t start = xml.find(open, pos);
        if (start == std::string::npos) break;
        const size_t end = xml.find(close, start + open.size());
        if (end == std::string::npos) break;
        out.push_back(xml.substr(start + open.size(), end - start - open.size()));
        pos = end + close.size();
    }
    return out;
}

bool Contains(const std::vector<std::string>& values, const std::string& wanted) {
    for (const auto& value : values) {
        if (value == wanted) return true;
    }
    return false;
}

std::string MetadataUrl(const std::string& root) {
    return root + "/net/minecraftforge/forge/maven-metadata.xml";
}

// 按数字段比大小；非数字段直接当 0，只用于在同一 MC 版本里挑最新
bool Newer(const std::string& left, const std::string& right) {
    size_t a = 0;
    size_t b = 0;
    while (a < left.size() || b < right.size()) {
        long long va = 0;
        long long vb = 0;
        while (a < left.size() && (left[a] < '0' || left[a] > '9')) ++a;
        while (b < right.size() && (right[b] < '0' || right[b] > '9')) ++b;
        while (a < left.size() && left[a] >= '0' && left[a] <= '9') va = va * 10 + (left[a++] - '0');
        while (b < right.size() && right[b] >= '0' && right[b] <= '9') vb = vb * 10 + (right[b++] - '0');
        if (va != vb) return va > vb;
        if (a >= left.size() && b >= right.size()) return false;
    }
    return false;
}

// 从 maven-metadata 里挑 <mc>- 开头、数字段最大的版本
std::string PickLatest(const std::vector<std::string>& versions, const std::string& mc_version) {
    const std::string prefix = mc_version + "-";
    std::string best;
    for (const auto& candidate : versions) {
        if (candidate.compare(0, prefix.size(), prefix) != 0) continue;
        if (best.empty() || Newer(candidate, best)) best = candidate;
    }
    return best;
}

}  // namespace

bool Forge::Promotions(const std::string& root, std::string& json, std::string& error) const {
    if (root != Sources::DefaultUrl("forge")) {
        error = "自定义 Forge 源没有推荐版本清单";
        return false;
    }
    if (cache_.TryGet(kPromotions, json, 60 * 60)) return true;
    if (!tools::HttpGet(kPromotions, json, error)) {
        error = "拿不到 Forge 推荐版本清单: " + error;
        return false;
    }
    cache_.Put(kPromotions, json);
    return true;
}

bool Forge::Versions(const std::string& root, std::string& xml, std::string& error) const {
    const std::string url = MetadataUrl(root);
    if (cache_.TryGet(url, xml, 60 * 60)) return true;
    if (!tools::HttpGet(url, xml, error)) {
        error = "拿不到 Forge 版本清单: " + url;
        return false;
    }
    cache_.Put(url, xml);
    return true;
}

bool Forge::Latest(const std::string& mc_version, std::string& version,
                   std::string& error) const {
    const std::string root = sources_.ForgeRoot();

    // 自定义源没有 promotions，直接从 maven-metadata 里挑
    auto from_metadata = [&]() -> bool {
        std::string xml;
        if (!Versions(root, xml, error)) return false;
        const std::string picked = PickLatest(XmlVersions(xml), mc_version);
        if (picked.empty()) {
            error = "Forge 没有 " + mc_version + " 的安装包";
            return false;
        }
        version = picked;
        return true;
    };

    if (root != Sources::DefaultUrl("forge")) return from_metadata();

    std::string build;
    std::string text;
    std::string promotions_error;
    if (Promotions(root, text, promotions_error)) {
        Json::Value promo;
        Json json;
        std::string parse_error;
        if (!json.Parse(text, promo, parse_error)) {
            promotions_error = "Forge 推荐版本清单解析失败: " + parse_error;
        } else {
            // 先看 recommended，没有再退到 latest
            build = promo["promos"][mc_version + "-recommended"].AsString();
            if (build.empty()) build = promo["promos"][mc_version + "-latest"].AsString();
        }
    }

    // promotions 拿不到（或没有这个 MC 版本）就退回 metadata
    if (build.empty()) return from_metadata();

    std::string xml;
    std::string xml_error;
    if (!Versions(root, xml, xml_error)) {
        // 清单拿不到就按新格式拼，1.12 之后都是 <mc>-<forge>
        version = mc_version + "-" + build;
        return true;
    }

    const std::vector<std::string> all = XmlVersions(xml);
    const std::string plain = mc_version + "-" + build;
    if (Contains(all, plain)) {
        version = plain;
        return true;
    }

    // 1.7.10 / 1.8.9 的 artifact 带一次 MC 后缀
    const std::string suffixed = mc_version + "-" + build + "-" + mc_version;
    if (Contains(all, suffixed)) {
        version = suffixed;
        return true;
    }

    error = "maven 上找不到 Forge " + plain;
    return false;
}

bool Forge::List(const std::string& mc_version, std::vector<std::string>& out,
                 std::string& error) const {
    out.clear();
    std::string xml;
    if (!Versions(sources_.ForgeRoot(), xml, error)) return false;

    const std::string prefix = mc_version + "-";
    for (const auto& candidate : XmlVersions(xml)) {
        if (candidate.compare(0, prefix.size(), prefix) == 0) out.push_back(candidate);
    }
    std::sort(out.begin(), out.end(),
              [](const std::string& a, const std::string& b) { return Newer(a, b); });
    return true;
}

std::string Forge::InstallerUrl(const std::string& root, const std::string& version) {    return root + "/net/minecraftforge/forge/" + version + "/forge-" + version + "-installer.jar";
}

}  // namespace versionsource
}  // namespace download
