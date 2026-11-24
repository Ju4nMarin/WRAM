#include "Version.h"

namespace wram {

bool ParseVersion(std::string_view text, Version& out) {
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) text.remove_prefix(1);
    int parts[3]{};
    int index = 0;
    int digits = 0;
    for (char ch : text) {
        if (ch >= '0' && ch <= '9') {
            if (++digits > 4) return false;
            parts[index] = parts[index] * 10 + (ch - '0');
        } else if (ch == '.') {
            if (digits == 0 || ++index > 2) return false;
            digits = 0;
        } else {
            return false;
        }
    }
    if (index != 2 || digits == 0) return false;
    out = {parts[0], parts[1], parts[2]};
    return true;
}

int CompareVersion(const Version& a, const Version& b) {
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    return 0;
}

std::string VersionToString(const Version& v) {
    return std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);
}

std::string JsonStringField(std::string_view json, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string_view::npos) return {};
    pos += needle.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) ++pos;
    if (pos >= json.size() || json[pos] != ':') return {};
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) ++pos;
    if (pos >= json.size() || json[pos] != '"') return {};
    const size_t start = ++pos;
    const size_t end = json.find('"', start);
    if (end == std::string_view::npos || end - start > 64) return {};
    const std::string_view value = json.substr(start, end - start);
    if (value.find('\\') != std::string_view::npos) return {};
    return std::string(value);
}

}  // namespace wram
