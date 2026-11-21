#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace wram {

// Identificadores estables de cada métrica. El orden define el índice interno;
// la clave de texto (ver MetricCatalog) es la que se persiste en disco.
enum class MetricId : uint8_t {
    CpuLoad,
    CpuTemp,
    CpuClock,
    RamLoad,
    RamUsed,
    GpuLoad,
    GpuTemp,
    VramUsed,
    DiskActivity,
    NetDown,
    NetUp,
    Battery,
    Uptime,
    Processes,
    Count
};

constexpr size_t kMetricCount = static_cast<size_t>(MetricId::Count);
constexpr size_t kMaxSlots = 10;

constexpr size_t ToIndex(MetricId id) { return static_cast<size_t>(id); }

using MetricMask = uint32_t;
constexpr MetricMask Bit(MetricId id) { return MetricMask{1} << ToIndex(id); }
constexpr MetricMask kAllMetrics = (MetricMask{1} << kMetricCount) - 1;

struct Sample {
    double value = 0.0;
    bool valid = false;
};

// Una lectura completa del sistema. Las métricas sin sensor quedan inválidas.
class MetricFrame {
public:
    void Set(MetricId id, double v) { samples_[ToIndex(id)] = {v, true}; }
    void Invalidate(MetricId id) { samples_[ToIndex(id)] = {}; }
    const Sample& Get(MetricId id) const { return samples_[ToIndex(id)]; }
    void Clear() { samples_.fill({}); }

private:
    std::array<Sample, kMetricCount> samples_{};
};

// Fuente de datos. Cada proveedor declara qué métricas sabe producir y solo
// abre sus contadores cuando alguna de ellas está activa.
class IMetricProvider {
public:
    virtual ~IMetricProvider() = default;
    virtual MetricMask Provides() const = 0;
    virtual void SetActive(MetricMask active) = 0;
    virtual void Collect(MetricFrame& frame) = 0;
};

}  // namespace wram
