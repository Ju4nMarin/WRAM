#pragma once

#include <memory>
#include <vector>

#include "../core/Metric.h"

namespace wram {

// Orquesta los proveedores. Agregar una fuente nueva no modifica a las demás.
class Sampler {
public:
    void Add(std::unique_ptr<IMetricProvider> provider) { providers_.push_back(std::move(provider)); }

    void SetActive(MetricMask active) {
        for (auto& p : providers_) p->SetActive(active & p->Provides());
    }

    const MetricFrame& Sample() {
        frame_.Clear();
        for (auto& p : providers_) p->Collect(frame_);
        return frame_;
    }

    const MetricFrame& Last() const { return frame_; }

private:
    std::vector<std::unique_ptr<IMetricProvider>> providers_;
    MetricFrame frame_;
};

}  // namespace wram
