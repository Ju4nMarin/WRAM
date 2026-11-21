#pragma once

namespace wram::autostart {

// Inicio con Windows mediante HKCU\...\Run (sin permisos de administrador).
bool IsEnabled();
bool SetEnabled(bool enabled);

}  // namespace wram::autostart
