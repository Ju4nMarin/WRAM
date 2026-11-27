#pragma once

namespace wram::autostart {

// Inicio con Windows mediante HKCU\...\Run (sin permisos de administrador), o con la tarea
// de inicio del paquete cuando WRAM corre como app de Microsoft Store.
bool IsEnabled();
bool SetEnabled(bool enabled);

}  // namespace wram::autostart
