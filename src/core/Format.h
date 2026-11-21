#pragma once

#include <string>

#include "MetricCatalog.h"

namespace wram {

// Texto del valor tal como se ve en la barra ("23%", "3.2 MB/s", "4:12").
std::wstring FormatValue(Unit unit, double value);

// Texto más ancho posible para una unidad. Se usa para reservar el ancho de
// cada celda y evitar que la barra "baile" cuando cambian los dígitos.
const wchar_t* WidestValue(Unit unit);

// Separador decimal. Por defecto "."; la app aplica el del usuario de Windows.
void SetDecimalSeparator(wchar_t separator);

// Marcador para una métrica sin sensor.
constexpr const wchar_t* kNoValue = L"\x2014";

}  // namespace wram
