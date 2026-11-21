#pragma once

#include "../core/Metric.h"

namespace wram {

// Memoria física (GlobalMemoryStatusEx).
class MemoryProvider final : public IMetricProvider {
public:
    MetricMask Provides() const override;
    void SetActive(MetricMask active) override { active_ = active & Provides(); }
    void Collect(MetricFrame& frame) override;

private:
    MetricMask active_ = 0;
};

// Batería (GetSystemPowerStatus). Sin batería la métrica queda inválida.
class PowerProvider final : public IMetricProvider {
public:
    MetricMask Provides() const override;
    void SetActive(MetricMask active) override { active_ = active & Provides(); }
    void Collect(MetricFrame& frame) override;

private:
    MetricMask active_ = 0;
};

// Tiempo desde el arranque.
class UptimeProvider final : public IMetricProvider {
public:
    MetricMask Provides() const override;
    void SetActive(MetricMask active) override { active_ = active & Provides(); }
    void Collect(MetricFrame& frame) override;

private:
    MetricMask active_ = 0;
};

}  // namespace wram
