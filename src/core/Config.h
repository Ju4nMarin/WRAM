#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Metric.h"

namespace wram {

struct SlotConfig {
    MetricId id = MetricId::CpuLoad;
    std::wstring label;      // vacío = etiqueta por defecto del catálogo
    double threshold = 0.0;  // unidad de visualización
    bool alert = false;
};

struct Config {
    static constexpr uint32_t kMinInterval = 500;
    static constexpr uint32_t kMaxInterval = 5000;
    static constexpr int kMinFontSize = 9;
    static constexpr int kMaxFontSize = 18;
    static constexpr int kMaxOffset = 1200;
    static constexpr size_t kMaxLabelChars = 8;
    static constexpr uint32_t kAutoColor = 0xFFFFFFFFu;

    std::vector<SlotConfig> slots;
    uint32_t intervalMs = 1000;
    int rows = 2;
    std::wstring font = L"Segoe UI Variable Text";
    int fontSize = 12;                 // DIP
    uint32_t valueColor = kAutoColor;  // 0xRRGGBB o automático según el tema
    uint32_t alertColor = 0xFF5A4E;
    bool showLabels = true;
    bool autostart = false;
    bool hideOnFullscreen = true;
    int offset = 8;                    // DIP entre la franja y la bandeja
    int theme = 0;                     // ventana de configuración: 0 sistema, 1 claro, 2 oscuro
    int language = 0;                  // ver I18n.h: 0 sistema, 1 es, 2 en, 3 pt, 4 fr, 5 zh

    MetricMask ActiveMask() const;
    static Config Defaults();
};

// Lista blanca de fuentes que puede elegir la interfaz.
const std::vector<std::wstring>& AllowedFonts();

// Serialización a INI. Es pura (sin E/S) para poder probarla de forma aislada.
// Parse nunca falla: lo inválido se descarta y se ajusta a los rangos permitidos.
namespace ConfigCodec {
std::string Serialize(const Config& config);
Config Parse(std::string_view ini);
}  // namespace ConfigCodec

}  // namespace wram
