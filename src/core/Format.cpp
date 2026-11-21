#include "Format.h"

#include <cmath>
#include <cwchar>

namespace wram {
namespace {

wchar_t g_decimal = L'.';

std::wstring Localize(const wchar_t* text) {
    std::wstring out(text);
    if (g_decimal != L'.') {
        for (wchar_t& c : out) {
            if (c == L'.') c = g_decimal;
        }
    }
    return out;
}

std::wstring Printf(const wchar_t* fmt, double a) {
    wchar_t buf[32];
    std::swprintf(buf, 32, fmt, a);
    return Localize(buf);
}

std::wstring FormatRate(double bytesPerSec) {
    static constexpr const wchar_t* kUnits[] = {L"B/s", L"KB/s", L"MB/s", L"GB/s"};
    double v = bytesPerSec < 0 ? 0 : bytesPerSec;
    int u = 0;
    while (v >= 1000.0 && u < 3) {
        v /= 1024.0;
        ++u;
    }
    wchar_t buf[32];
    if (u == 0 || v >= 10.0) {
        std::swprintf(buf, 32, L"%.0f %ls", v, kUnits[u]);
    } else {
        std::swprintf(buf, 32, L"%.1f %ls", v, kUnits[u]);
    }
    return Localize(buf);
}

std::wstring FormatDuration(double seconds) {
    const auto total = static_cast<long long>(seconds < 0 ? 0 : seconds);
    const long long days = total / 86400;
    const long long hours = (total % 86400) / 3600;
    const long long minutes = (total % 3600) / 60;
    wchar_t buf[32];
    if (days > 0) {
        std::swprintf(buf, 32, L"%lldd %02lldh", days, hours);
    } else {
        std::swprintf(buf, 32, L"%lld:%02lld", hours, minutes);
    }
    return buf;
}

}  // namespace

std::wstring FormatValue(Unit unit, double value) {
    if (!std::isfinite(value)) return kNoValue;
    switch (unit) {
        case Unit::Percent: return Printf(L"%.0f%%", value < 0 ? 0 : (value > 100 ? 100 : value));
        case Unit::Celsius: return Printf(L"%.0f\x00B0", value);
        case Unit::MHz:
            return value >= 1000.0 ? Printf(L"%.2f GHz", value / 1000.0) : Printf(L"%.0f MHz", value);
        case Unit::GigaBytes:
            return value >= 100.0 ? Printf(L"%.0f GB", value) : Printf(L"%.1f GB", value);
        case Unit::BytesPerSec: return FormatRate(value);
        case Unit::Duration: return FormatDuration(value);
        case Unit::Count: return Printf(L"%.0f", value);
    }
    return kNoValue;
}

void SetDecimalSeparator(wchar_t separator) { g_decimal = separator ? separator : L'.'; }

const wchar_t* WidestValue(Unit unit) {
    switch (unit) {
        case Unit::Percent: return L"100%";
        case Unit::Celsius: return L"100\x00B0";
        case Unit::MHz: return L"8.88 GHz";
        case Unit::GigaBytes: return L"88.8 GB";
        case Unit::BytesPerSec: return L"888 KB/s";
        case Unit::Duration: return L"88d 88h";
        case Unit::Count: return L"8888";
    }
    return L"8888";
}

}  // namespace wram
