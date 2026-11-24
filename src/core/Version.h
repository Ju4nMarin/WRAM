#pragma once

#include <string>
#include <string_view>

namespace wram {

// Versión semántica sin sufijos: "1.2.3" o "v1.2.3".
struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

// Solo dígitos y dos puntos, cada parte de 1 a 4 dígitos. Cualquier otra cosa se rechaza.
bool ParseVersion(std::string_view text, Version& out);
int CompareVersion(const Version& a, const Version& b);  // <0, 0 o >0
std::string VersionToString(const Version& v);

// Valor de texto de una clave de primer nivel en un JSON ("clave":"valor"), sin secuencias de escape.
// Alcanza para leer "tag_name" de la respuesta de GitHub; devuelve vacío si no lo encuentra.
std::string JsonStringField(std::string_view json, std::string_view key);

}  // namespace wram
