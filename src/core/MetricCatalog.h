#pragma once

#include <string_view>

#include "Metric.h"

namespace wram {

enum class Unit : uint8_t { Percent, Celsius, MHz, GigaBytes, BytesPerSec, Duration, Count };

struct MetricInfo {
    MetricId id;
    const char* key;            // clave persistida en config.ini
    const char* name;           // nombre visible (UTF-8)
    const char* source;         // de dónde sale el dato (UTF-8)
    const wchar_t* label;       // etiqueta corta por defecto en la barra
    Unit unit;
    double defaultThreshold;    // en unidad de visualización
    double thresholdMax;        // 0 = la métrica no admite aviso
    bool alertBelow;            // true: avisa cuando el valor baja del umbral
    bool approximate;           // el sensor puede no ser exacto
};

const MetricInfo& Info(MetricId id);
const MetricInfo* FindByKey(std::string_view key);

// Textos en el idioma activo (ver I18n.h).
const char* NameOf(const MetricInfo& info);
const char* SourceOf(const MetricInfo& info);
const wchar_t* LabelOf(const MetricInfo& info);

// Factor entre la unidad de visualización del umbral y el valor crudo.
double ThresholdScale(Unit unit);

// Evalúa si una muestra supera el umbral configurado.
bool IsAlert(const MetricInfo& info, double rawValue, double threshold);

}  // namespace wram
