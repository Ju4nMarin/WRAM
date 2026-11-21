#pragma once

#include <array>
#include <vector>

#include "../core/Metric.h"
#include "../platform/Win.h"

#include <pdh.h>

namespace wram {

// Métricas que salen de los contadores de rendimiento de Windows (PDH).
// Usa nombres en inglés (PdhAddEnglishCounter) para funcionar en cualquier idioma
// y solo abre los contadores de las métricas activas.
class PdhProvider final : public IMetricProvider {
public:
    PdhProvider() = default;
    ~PdhProvider() override;
    PdhProvider(const PdhProvider&) = delete;
    PdhProvider& operator=(const PdhProvider&) = delete;

    MetricMask Provides() const override;
    void SetActive(MetricMask active) override;
    void Collect(MetricFrame& frame) override;

private:
    enum Ctr { CpuLoad, CpuFreq, CpuPerf, Thermal, Gpu3D, Vram, DiskIdle, NetRecv, NetSent, Procs, kCtrCount };

    void Rebuild(MetricMask active);
    void CloseQuery();
    bool Add(Ctr ctr, const wchar_t* path);
    bool ReadSingle(Ctr ctr, double& out) const;
    const PDH_FMT_COUNTERVALUE_ITEM_W* ReadArray(Ctr ctr, DWORD& count);

    PDH_HQUERY query_ = nullptr;
    std::array<PDH_HCOUNTER, kCtrCount> counters_{};
    MetricMask active_ = 0;
    bool thermalHighPrecision_ = true;
    std::vector<BYTE> arrayBuf_;
};

}  // namespace wram
