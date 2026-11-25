#include "GpuThermalProvider.h"

#include <algorithm>

#include "../platform/Win.h"

#include <winternl.h>
#include <d3dkmthk.h>

namespace wram {
namespace {
constexpr int kReopenAfterFailures = 30;
}

GpuThermalProvider::~GpuThermalProvider() { CloseAdapters(); }

MetricMask GpuThermalProvider::Provides() const { return Bit(MetricId::GpuTemp); }

void GpuThermalProvider::SetActive(MetricMask active) {
    active &= Provides();
    if (active == active_) return;
    active_ = active;
    if (active_) OpenAdapters();
    else CloseAdapters();
}

void GpuThermalProvider::OpenAdapters() {
    CloseAdapters();
    D3DKMT_ENUMADAPTERS2 e{};
    if (D3DKMTEnumAdapters2(&e) != 0 || e.NumAdapters == 0) return;
    std::vector<D3DKMT_ADAPTERINFO> infos(e.NumAdapters);
    e.pAdapters = infos.data();
    if (D3DKMTEnumAdapters2(&e) != 0) return;
    for (ULONG i = 0; i < e.NumAdapters; ++i) adapters_.push_back(infos[i].hAdapter);
}

void GpuThermalProvider::CloseAdapters() {
    for (const uint32_t h : adapters_) {
        D3DKMT_CLOSEADAPTER c{};
        c.hAdapter = h;
        (void)D3DKMTCloseAdapter(&c);  // al cerrar no hay nada que hacer si falla
    }
    adapters_.clear();
}

void GpuThermalProvider::Collect(MetricFrame& frame) {
    if (!active_) return;
    double hottest = 0;
    for (const uint32_t h : adapters_) {
        D3DKMT_ADAPTER_PERFDATA perf{};
        D3DKMT_QUERYADAPTERINFO q{};
        q.hAdapter = h;
        q.Type = KMTQAITYPE_ADAPTERPERFDATA;
        q.pPrivateDriverData = &perf;
        q.PrivateDriverDataSize = sizeof perf;
        if (D3DKMTQueryAdapterInfo(&q) == 0 && perf.Temperature > 0) {
            hottest = std::max(hottest, perf.Temperature / 10.0);  // décimas de grado
        }
    }
    if (hottest > 0 && hottest < 150) {
        frame.Set(MetricId::GpuTemp, hottest);
        failures_ = 0;
    } else if (++failures_ >= kReopenAfterFailures) {
        // Cambio de driver o GPU conectada en caliente: se vuelve a enumerar.
        failures_ = 0;
        OpenAdapters();
    }
}

}  // namespace wram
