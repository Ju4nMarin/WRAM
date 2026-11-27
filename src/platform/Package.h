#pragma once

#include <string>

namespace wram::package {

// true cuando WRAM corre desde un paquete MSIX (versión de Microsoft Store).
bool IsPackaged();

// Carpeta LocalState del paquete (%LOCALAPPDATA%\Packages\<familia>\LocalState). Vacía sin paquete.
std::wstring LocalStateDir();

}  // namespace wram::package
