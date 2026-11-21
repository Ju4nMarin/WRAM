#pragma once

#include <cstdint>
#include <vector>

#include "../core/Metric.h"

namespace wram {

// Temperatura de GPU a través del kernel gráfico (D3DKMT, WDDM 2.4+).
// Es la misma lectura que usa el Administrador de tareas y funciona con
// cualquier fabricante cuyo driver la exponga, sin permisos de administrador.
class GpuThermalProvider final : public IMetricProvider {
public:
    GpuThermalProvider() = default;
    ~GpuThermalProvider() override;
    GpuThermalProvider(const GpuThermalProvider&) = delete;
    GpuThermalProvider& operator=(const GpuThermalProvider&) = delete;

    MetricMask Provides() const override;
    void SetActive(MetricMask active) override;
    void Collect(MetricFrame& frame) override;

private:
    void OpenAdapters();
    void CloseAdapters();

    std::vector<uint32_t> adapters_;  // D3DKMT_HANDLE
    MetricMask active_ = 0;
    int failures_ = 0;
};

}  // namespace wram
