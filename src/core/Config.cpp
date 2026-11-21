#include "Config.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "I18n.h"
#include "MetricCatalog.h"
#include "Text.h"

namespace wram {
namespace {

constexpr size_t kMaxIniBytes = 64 * 1024;

std::wstring SanitizeLabel(std::wstring label) {
    label.erase(std::remove_if(label.begin(), label.end(),
                               [](wchar_t c) { return c < 0x20 || c == L'|' || c == L'=' || c == L'[' || c == L']'; }),
                label.end());
    if (label.size() > Config::kMaxLabelChars) label.resize(Config::kMaxLabelChars);
    return label;
}

bool ParseColor(std::string_view s, uint32_t& out) {
    s = text::Trim(s);
    if (s == "auto") {
        out = Config::kAutoColor;
        return true;
    }
    if (s.size() != 7 || s[0] != '#') return false;
    uint32_t v = 0;
    for (size_t i = 1; i < 7; ++i) {
        const char c = s[i];
        uint32_t d;
        if (c >= '0' && c <= '9') d = static_cast<uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') d = static_cast<uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = static_cast<uint32_t>(c - 'A' + 10);
        else return false;
        v = (v << 4) | d;
    }
    out = v;
    return true;
}

std::string ColorToString(uint32_t c) {
    if (c == Config::kAutoColor) return "auto";
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%06x", c & 0xFFFFFFu);
    return buf;
}

template <typename T>
T Clamp(long long v, T lo, T hi) {
    return static_cast<T>(std::clamp<long long>(v, lo, hi));
}

bool ParseBool(std::string_view s, bool& out) {
    s = text::Trim(s);
    if (s == "1" || s == "true") { out = true; return true; }
    if (s == "0" || s == "false") { out = false; return true; }
    return false;
}

// Formato de una posición: clave|etiqueta|umbral|aviso
bool ParseSlot(std::string_view value, SlotConfig& slot) {
    std::string_view parts[4];
    size_t n = 0;
    while (n < 4) {
        const size_t bar = value.find('|');
        parts[n++] = value.substr(0, bar);
        if (bar == std::string_view::npos) break;
        value.remove_prefix(bar + 1);
    }
    const MetricInfo* info = FindByKey(text::Trim(parts[0]));
    if (!info) return false;
    slot.id = info->id;
    slot.label = n > 1 ? SanitizeLabel(text::Utf8ToWide(text::Trim(parts[1]))) : std::wstring{};
    slot.threshold = info->defaultThreshold;
    slot.alert = info->thresholdMax > 0 && info->defaultThreshold > 0;
    double thr = 0;
    if (n > 2 && text::ParseDouble(parts[2], thr) && std::isfinite(thr)) {
        slot.threshold = std::clamp(thr, 0.0, info->thresholdMax);
    }
    bool alert = false;
    if (n > 3 && ParseBool(parts[3], alert)) slot.alert = alert && info->thresholdMax > 0;
    return true;
}

}  // namespace

MetricMask Config::ActiveMask() const {
    MetricMask mask = 0;
    for (const auto& s : slots) mask |= Bit(s.id);
    return mask;
}

Config Config::Defaults() {
    Config c;
    for (MetricId id : {MetricId::CpuLoad, MetricId::RamLoad, MetricId::GpuLoad, MetricId::GpuTemp, MetricId::NetDown,
                        MetricId::NetUp}) {
        const MetricInfo& info = Info(id);
        c.slots.push_back({id, {}, info.defaultThreshold, info.thresholdMax > 0 && info.defaultThreshold > 0});
    }
    return c;
}

const std::vector<std::wstring>& AllowedFonts() {
    static const std::vector<std::wstring> kFonts{L"Segoe UI Variable Text", L"Bahnschrift", L"Segoe UI",
                                                  L"Cascadia Mono", L"Consolas"};
    return kFonts;
}

namespace ConfigCodec {

std::string Serialize(const Config& c) {
    std::string out;
    out.reserve(1024);
    auto line = [&out](const char* key, const std::string& value) {
        out += key;
        out += '=';
        out += value;
        out += '\n';
    };
    out += "[general]\n";
    line("interval", std::to_string(c.intervalMs));
    line("rows", std::to_string(c.rows));
    line("font", text::WideToUtf8(c.font));
    line("fontSize", std::to_string(c.fontSize));
    line("valueColor", ColorToString(c.valueColor));
    line("alertColor", ColorToString(c.alertColor));
    line("labels", c.showLabels ? "1" : "0");
    line("autostart", c.autostart ? "1" : "0");
    line("fullscreen", std::to_string(c.fullscreenMode));
    line("overlayPos", std::to_string(c.overlayPosition));
    line("offset", std::to_string(c.offset));
    line("theme", std::to_string(c.theme));
    line("lang", std::to_string(c.language));
    out += "\n[slots]\n";
    for (size_t i = 0; i < c.slots.size(); ++i) {
        const SlotConfig& s = c.slots[i];
        char thr[32];
        std::snprintf(thr, sizeof thr, "%g", s.threshold);
        out += std::to_string(i + 1);
        out += '=';
        out += Info(s.id).key;
        out += '|';
        out += text::WideToUtf8(SanitizeLabel(s.label));
        out += '|';
        out += thr;
        out += '|';
        out += s.alert ? "1" : "0";
        out += '\n';
    }
    return out;
}

Config Parse(std::string_view ini) {
    Config c = Config::Defaults();
    if (ini.size() > kMaxIniBytes) return c;
    if (ini.size() >= 3 && static_cast<unsigned char>(ini[0]) == 0xEF) ini.remove_prefix(3);  // BOM

    bool sawSlots = false;
    std::vector<SlotConfig> slots;
    MetricMask used = 0;
    std::string_view section;

    while (!ini.empty()) {
        const size_t eol = ini.find('\n');
        std::string_view raw = ini.substr(0, eol);
        ini.remove_prefix(eol == std::string_view::npos ? ini.size() : eol + 1);
        const std::string_view line = text::Trim(raw);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            if (section == "slots") sawSlots = true;
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string_view::npos) continue;
        const std::string_view key = text::Trim(line.substr(0, eq));
        const std::string_view value = text::Trim(line.substr(eq + 1));
        long long n = 0;

        if (section == "general") {
            if (key == "interval" && text::ParseInt(value, n)) c.intervalMs = Clamp<uint32_t>(n, Config::kMinInterval, Config::kMaxInterval);
            else if (key == "rows" && text::ParseInt(value, n)) c.rows = Clamp<int>(n, 1, 2);
            else if (key == "fontSize" && text::ParseInt(value, n)) c.fontSize = Clamp<int>(n, Config::kMinFontSize, Config::kMaxFontSize);
            else if (key == "offset" && text::ParseInt(value, n)) c.offset = Clamp<int>(n, 0, Config::kMaxOffset);
            else if (key == "theme" && text::ParseInt(value, n)) c.theme = Clamp<int>(n, 0, 2);
            else if (key == "lang" && text::ParseInt(value, n)) c.language = Clamp<int>(n, 0, kMaxLanguageSetting);
            else if (key == "valueColor") ParseColor(value, c.valueColor);
            else if (key == "alertColor") {
                uint32_t col = 0;
                if (ParseColor(value, col) && col != Config::kAutoColor) c.alertColor = col;
            } else if (key == "labels") ParseBool(value, c.showLabels);
            else if (key == "autostart") ParseBool(value, c.autostart);
            else if (key == "fullscreen" && text::ParseInt(value, n)) c.fullscreenMode = Clamp<int>(n, 0, 2);
            else if (key == "overlayPos" && text::ParseInt(value, n)) c.overlayPosition = Clamp<int>(n, 0, 7);
            else if (key == "hideFullscreen") {  // formato anterior: 1 ocultar, 0 dejar igual
                bool hideOld = true;
                if (ParseBool(value, hideOld)) c.fullscreenMode = hideOld ? 0 : 2;
            }
            else if (key == "font") {
                const std::wstring font = text::Utf8ToWide(value);
                const auto& allowed = AllowedFonts();
                if (std::find(allowed.begin(), allowed.end(), font) != allowed.end()) c.font = font;
            }
        } else if (section == "slots") {
            SlotConfig slot;
            if (slots.size() < kMaxSlots && ParseSlot(value, slot) && !(used & Bit(slot.id))) {
                used |= Bit(slot.id);
                slots.push_back(std::move(slot));
            }
        }
    }
    if (sawSlots) c.slots = std::move(slots);
    return c;
}

}  // namespace ConfigCodec
}  // namespace wram
