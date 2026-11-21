#include "PdhProvider.h"

#include <algorithm>
#include <pdhmsg.h>
#include <string_view>

namespace wram {
namespace {

constexpr MetricMask kProvided = Bit(MetricId::CpuLoad) | Bit(MetricId::CpuTemp) | Bit(MetricId::CpuClock) |
                                 Bit(MetricId::GpuLoad) | Bit(MetricId::VramUsed) | Bit(MetricId::DiskActivity) |
                                 Bit(MetricId::NetDown) | Bit(MetricId::NetUp) | Bit(MetricId::Processes);

constexpr DWORD kFormat = PDH_FMT_DOUBLE | PDH_FMT_NOCAP100;
constexpr double kBytesPerGiB = 1024.0 * 1024.0 * 1024.0;

bool IsValid(DWORD status) { return status == PDH_CSTATUS_VALID_DATA || status == PDH_CSTATUS_NEW_DATA; }

// "pid_1234_luid_0x0_0xD1F0_phys_0_eng_0_engtype_3D" -> "luid_0x0_0xD1F0_phys_0_eng_0_engtype_3D"
std::wstring_view EngineKey(const wchar_t* instance) {
    std::wstring_view name(instance);
    const size_t pos = name.find(L"luid_");
    return pos == std::wstring_view::npos ? name : name.substr(pos);
}

}  // namespace

PdhProvider::~PdhProvider() { CloseQuery(); }

MetricMask PdhProvider::Provides() const { return kProvided; }

void PdhProvider::CloseQuery() {
    if (query_) PdhCloseQuery(query_);
    query_ = nullptr;
    counters_.fill(nullptr);
}

bool PdhProvider::Add(Ctr ctr, const wchar_t* path) {
    PDH_HCOUNTER h = nullptr;
    if (PdhAddEnglishCounterW(query_, path, 0, &h) != ERROR_SUCCESS) return false;
    counters_[ctr] = h;
    return true;
}

void PdhProvider::SetActive(MetricMask active) {
    active &= kProvided;
    if (active == active_ && (query_ || active == 0)) return;
    Rebuild(active);
}

void PdhProvider::Rebuild(MetricMask active) {
    CloseQuery();
    active_ = active;
    if (active == 0 || PdhOpenQueryW(nullptr, 0, &query_) != ERROR_SUCCESS) {
        query_ = nullptr;
        return;
    }
    if (active & Bit(MetricId::CpuLoad)) {
        if (!Add(CpuLoad, L"\\Processor Information(_Total)\\% Processor Utility")) {
            Add(CpuLoad, L"\\Processor(_Total)\\% Processor Time");
        }
    }
    if (active & Bit(MetricId::CpuClock)) {
        Add(CpuFreq, L"\\Processor Information(_Total)\\Processor Frequency");
        Add(CpuPerf, L"\\Processor Information(_Total)\\% Processor Performance");
    }
    if (active & Bit(MetricId::CpuTemp)) {
        thermalHighPrecision_ = Add(Thermal, L"\\Thermal Zone Information(*)\\High Precision Temperature");
        if (!thermalHighPrecision_) Add(Thermal, L"\\Thermal Zone Information(*)\\Temperature");
    }
    if (active & Bit(MetricId::GpuLoad)) Add(Gpu3D, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage");
    if (active & Bit(MetricId::VramUsed)) Add(Vram, L"\\GPU Adapter Memory(*)\\Dedicated Usage");
    if (active & Bit(MetricId::DiskActivity)) Add(DiskIdle, L"\\PhysicalDisk(*)\\% Idle Time");
    if (active & Bit(MetricId::NetDown)) Add(NetRecv, L"\\Network Interface(*)\\Bytes Received/sec");
    if (active & Bit(MetricId::NetUp)) Add(NetSent, L"\\Network Interface(*)\\Bytes Sent/sec");
    if (active & Bit(MetricId::Processes)) Add(Procs, L"\\System\\Processes");

    PdhCollectQueryData(query_);  // primera muestra: los contadores de tasa necesitan dos
}

bool PdhProvider::ReadSingle(Ctr ctr, double& out) const {
    if (!counters_[ctr]) return false;
    PDH_FMT_COUNTERVALUE v{};
    if (PdhGetFormattedCounterValue(counters_[ctr], kFormat, nullptr, &v) != ERROR_SUCCESS || !IsValid(v.CStatus)) {
        return false;
    }
    out = v.doubleValue;
    return true;
}

const PDH_FMT_COUNTERVALUE_ITEM_W* PdhProvider::ReadArray(Ctr ctr, DWORD& count) {
    count = 0;
    if (!counters_[ctr]) return nullptr;
    DWORD size = static_cast<DWORD>(arrayBuf_.size());
    PDH_STATUS st = PdhGetFormattedCounterArrayW(counters_[ctr], kFormat, &size, &count,
                                                 arrayBuf_.empty() ? nullptr : reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(arrayBuf_.data()));
    if (st == PDH_MORE_DATA) {
        arrayBuf_.resize(size);
        st = PdhGetFormattedCounterArrayW(counters_[ctr], kFormat, &size, &count,
                                          reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(arrayBuf_.data()));
    }
    if (st != ERROR_SUCCESS) {
        count = 0;
        return nullptr;
    }
    return reinterpret_cast<const PDH_FMT_COUNTERVALUE_ITEM_W*>(arrayBuf_.data());
}

void PdhProvider::Collect(MetricFrame& frame) {
    if (!query_) return;
    if (PdhCollectQueryData(query_) != ERROR_SUCCESS) return;

    double v = 0;
    if ((active_ & Bit(MetricId::CpuLoad)) && ReadSingle(CpuLoad, v)) {
        frame.Set(MetricId::CpuLoad, std::clamp(v, 0.0, 100.0));
    }
    double base = 0, perf = 0;
    if ((active_ & Bit(MetricId::CpuClock)) && ReadSingle(CpuFreq, base) && ReadSingle(CpuPerf, perf) && base > 0) {
        frame.Set(MetricId::CpuClock, base * perf / 100.0);
    }
    if ((active_ & Bit(MetricId::Processes)) && ReadSingle(Procs, v)) frame.Set(MetricId::Processes, v);

    DWORD n = 0;
    if (active_ & Bit(MetricId::CpuTemp)) {
        // Se toma la zona más caliente; se descartan lecturas fuera de rango físico.
        double hottest = -1;
        if (const auto* items = ReadArray(Thermal, n)) {
            for (DWORD i = 0; i < n; ++i) {
                if (!IsValid(items[i].FmtValue.CStatus)) continue;
                const double kelvin = thermalHighPrecision_ ? items[i].FmtValue.doubleValue / 10.0 : items[i].FmtValue.doubleValue;
                const double c = kelvin - 273.15;
                if (c > 1.0 && c < 125.0) hottest = std::max(hottest, c);
            }
        }
        if (hottest > 0) frame.Set(MetricId::CpuTemp, hottest);
    }
    if (active_ & Bit(MetricId::GpuLoad)) {
        // Como el Administrador de tareas: suma por motor físico y se queda con el más ocupado.
        struct Engine { std::wstring_view key; double sum; };
        std::vector<Engine> engines;
        bool any = false;
        if (const auto* items = ReadArray(Gpu3D, n)) {
            for (DWORD i = 0; i < n; ++i) {
                if (!IsValid(items[i].FmtValue.CStatus)) continue;
                any = true;
                const std::wstring_view key = EngineKey(items[i].szName);
                auto it = std::find_if(engines.begin(), engines.end(), [&](const Engine& e) { return e.key == key; });
                if (it == engines.end()) engines.push_back({key, items[i].FmtValue.doubleValue});
                else it->sum += items[i].FmtValue.doubleValue;
            }
        }
        double busiest = 0;
        for (const auto& e : engines) busiest = std::max(busiest, e.sum);
        if (any) frame.Set(MetricId::GpuLoad, std::clamp(busiest, 0.0, 100.0));
    }
    if (active_ & Bit(MetricId::VramUsed)) {
        double maxBytes = -1;
        if (const auto* items = ReadArray(Vram, n)) {
            for (DWORD i = 0; i < n; ++i) {
                if (IsValid(items[i].FmtValue.CStatus)) maxBytes = std::max(maxBytes, items[i].FmtValue.doubleValue);
            }
        }
        if (maxBytes >= 0) frame.Set(MetricId::VramUsed, maxBytes / kBytesPerGiB);
    }
    if (active_ & Bit(MetricId::DiskActivity)) {
        double busiest = -1;
        if (const auto* items = ReadArray(DiskIdle, n)) {
            for (DWORD i = 0; i < n; ++i) {
                if (!IsValid(items[i].FmtValue.CStatus) || std::wstring_view(items[i].szName) == L"_Total") continue;
                busiest = std::max(busiest, 100.0 - std::clamp(items[i].FmtValue.doubleValue, 0.0, 100.0));
            }
        }
        if (busiest >= 0) frame.Set(MetricId::DiskActivity, busiest);
    }
    auto sumOf = [&](Ctr ctr, MetricId id) {
        if (!(active_ & Bit(id))) return;
        double total = 0;
        bool any = false;
        if (const auto* items = ReadArray(ctr, n)) {
            for (DWORD i = 0; i < n; ++i) {
                if (!IsValid(items[i].FmtValue.CStatus)) continue;
                total += items[i].FmtValue.doubleValue;
                any = true;
            }
        }
        if (any) frame.Set(id, total);
    };
    sumOf(NetRecv, MetricId::NetDown);
    sumOf(NetSent, MetricId::NetUp);
}

}  // namespace wram
