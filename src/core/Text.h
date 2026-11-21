#pragma once

#include <string>
#include <string_view>

namespace wram::text {

std::wstring Utf8ToWide(std::string_view utf8);
std::string WideToUtf8(std::wstring_view wide);

// Escapa un texto UTF-8 para incrustarlo entre comillas en JSON.
void AppendJsonString(std::string& out, std::string_view utf8);

std::string_view Trim(std::string_view s);

// Convierte un entero en base 10 o devuelve false. Sin excepciones.
bool ParseInt(std::string_view s, long long& out);
bool ParseDouble(std::string_view s, double& out);

}  // namespace wram::text
