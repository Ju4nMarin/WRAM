#include "SystemProviders.h"

#include "../platform/Win.h"

namespace wram {

MetricMask MemoryProvider::Provides() const { return Bit(MetricId::RamLoad) | Bit(MetricId::RamUsed); }

void MemoryProvider::Collect(MetricFrame& frame) {
    if (!active_) return;
    MEMORYSTATUSEX ms{sizeof ms};
    if (!GlobalMemoryStatusEx(&ms) || ms.ullTotalPhys == 0) return;
    const double used = static_cast<double>(ms.ullTotalPhys - ms.ullAvailPhys);
    frame.Set(MetricId::RamLoad, used * 100.0 / static_cast<double>(ms.ullTotalPhys));
    frame.Set(MetricId::RamUsed, used / (1024.0 * 1024.0 * 1024.0));
}

MetricMask PowerProvider::Provides() const { return Bit(MetricId::Battery); }

void PowerProvider::Collect(MetricFrame& frame) {
    if (!active_) return;
    SYSTEM_POWER_STATUS ps{};
    constexpr BYTE kNoBattery = 128;
    constexpr BYTE kUnknown = 255;
    if (!GetSystemPowerStatus(&ps) || ps.BatteryFlag == kUnknown || (ps.BatteryFlag & kNoBattery) ||
        ps.BatteryLifePercent > 100) {
        return;
    }
    frame.Set(MetricId::Battery, ps.BatteryLifePercent);
}

MetricMask UptimeProvider::Provides() const { return Bit(MetricId::Uptime); }

void UptimeProvider::Collect(MetricFrame& frame) {
    if (active_) frame.Set(MetricId::Uptime, static_cast<double>(GetTickCount64()) / 1000.0);
}

}  // namespace wram
