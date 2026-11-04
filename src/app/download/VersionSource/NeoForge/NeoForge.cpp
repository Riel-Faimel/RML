#include "app/network/Network.h"
#include "app/download/VersionSource/NeoForge/NeoForge.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "app/tools/Json.h"
#include "app/tools/tools.h"

namespace download {
namespace versionsource {

namespace {

std::string NeoForgeMetadataUrl(const std::string& root) {
    return root + "/net/neoforged/neoforge/maven-metadata.xml";
}

std::string ForgeMetadataUrl(const std::string& root) {
    return root + "/net/neoforged/forge/maven-metadata.xml";
}

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

bool IsDigits(const std::string& text) {
    if (text.empty()) return false;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
    }
    return true;
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

// MC 1.21.1 → 21.1：NeoForge 版本号就是 MC 版本号去掉最前面的 1.，后面接 build
std::string VersionPrefix(const std::string& mc_version) {
    if (mc_version.rfind("1.", 0) == 0) return mc_version.substr(2);
    return mc_version;
}

int SegmentCount(const std::string& text) {
    int count = 1;
    for (char c : text) {
        if (c == '.') ++count;
    }
    return count;
}

// 候选规则：<prefix>.<build>，或两段 MC 版本时允许 <prefix>.0.<build>
bool Matches(const std::string& candidate, const std::string& prefix, int prefix_segments) {
    if (candidate.compare(0, prefix.size(), prefix) != 0) return false;
    if (candidate.size() <= prefix.size() || candidate[prefix.size()] != '.') return false;

    const std::string rest = candidate.substr(prefix.size() + 1);
    if (IsDigits(rest)) return true;
    if (prefix_segments == 2 && rest.rfind("0.", 0) == 0 && IsDigits(rest.substr(2))) return true;
    return false;
}

std::string PickLatest(const std::vector<std::string>& versions, const std::string& prefix,
                       int prefix_segments) {
    std::string best;
    for (const auto& candidate : versions) {
        if (!Matches(candidate, prefix, prefix_segments)) continue;
        if (best.empty() || Newer(candidate, best)) best = candidate;
    }
    return best;
}

}  // namespace

bool NeoForge::Metadata(const std::string& url, std::string& xml, std::string& error) const {
    if (cache_.TryGet(url, xml, 60 * 60)) return true;
    if (!Network::http(HttpCMD::GET, url, xml, error)) {
        error = "拿不到 NeoForge 版本清单: " + error;
        return false;
    }
    cache_.Put(url, xml);
    return true;
}

bool NeoForge::List(const std::string& mc_version, std::vector<std::string>& out,
                    std::string& error) const {
    out.clear();
    const std::string root = sources_.NeoForgeRoot();
    std::string xml;
    if (mc_version == "1.20.1") {
        if (!Metadata(ForgeMetadataUrl(root), xml, error)) return false;
        for (const auto& candidate : XmlVersions(xml)) {
            if (Matches(candidate, "1.20.1-47.1.", 3)) out.push_back(candidate);
        }
    } else {
        if (!Metadata(NeoForgeMetadataUrl(root), xml, error)) return false;
        const std::string prefix = VersionPrefix(mc_version);
        const int segments = SegmentCount(prefix);
        for (const auto& candidate : XmlVersions(xml)) {
            if (Matches(candidate, prefix, segments)) out.push_back(candidate);
        }
    }
    std::sort(out.begin(), out.end(),
              [](const std::string& a, const std::string& b) { return Newer(a, b); });
    return true;
}

bool NeoForge::Latest(const std::string& mc_version, std::string& version,
                      std::string& error) const {
    const std::string root = sources_.NeoForgeRoot();

    // 1.20.1 分叉初期沿用 Forge 的版本号（47.1.x），artifact 也在 net/neoforged/forge 下
    if (mc_version == "1.20.1") {
        std::string xml;
        if (!Metadata(ForgeMetadataUrl(root), xml, error)) return false;
        version = PickLatest(XmlVersions(xml), "1.20.1-47.1.", 3);
        if (version.empty()) {
            error = "NeoForge 没有 1.20.1 的安装包";
            return false;
        }
        return true;
    }

    std::string xml;
    if (!Metadata(NeoForgeMetadataUrl(root), xml, error)) return false;

    const std::string prefix = VersionPrefix(mc_version);
    version = PickLatest(XmlVersions(xml), prefix, SegmentCount(prefix));
    if (version.empty()) {
        error = "NeoForge 没有 " + mc_version + " 的安装包";
        return false;
    }
    return true;
}

std::string NeoForge::InstallerUrl(const std::string& root, const std::string& version) {
    const std::string base = root + "/net/neoforged/";
    if (version.rfind("1.20.1-", 0) == 0) {
        return base + "forge/" + version + "/forge-" + version + "-installer.jar";
    }
    return base + "neoforge/" + version + "/neoforge-" + version + "-installer.jar";
}

}  // namespace versionsource
}  // namespace download
