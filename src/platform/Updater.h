#pragma once

#include <string>

#include "../core/Version.h"
#include "Win.h"

namespace wram::updater {

enum class Error { None, Network, Verify, Write };

struct CheckResult {
    bool ok = false;
    std::string tag;  // "v1.2.3", ya validado
    Version version;
};

// Consulta la última versión publicada en GitHub. Bloquea: se llama desde un hilo de trabajo.
CheckResult CheckLatest();

// Descarga WRAM.exe de esa versión, verifica SHA-256, cabecera PE y número de versión, y reemplaza el
// ejecutable actual (el anterior queda como WRAM.exe.old hasta el próximo arranque). Bloquea.
Error Install(const std::string& tag, const Version& expected);

// Arranca el ejecutable ya reemplazado; el nuevo proceso espera a que este termine.
bool LaunchUpdated();

// Borra el WRAM.exe.old que deja una actualización.
void RemoveLeftovers();

// Línea de comandos de un arranque tras actualizar: espera a que el proceso anterior termine.
bool WaitForPreviousInstance(const wchar_t* commandLine);

// Estado del aviso automático, en %LOCALAPPDATA%\WRAM\update.ini: cuándo se consultó GitHub por última
// vez (segundos Unix) y de qué versión ya se avisó, para notificar cada versión una sola vez.
struct NotifyState {
    long long lastCheck = 0;
    std::string notifiedTag;
};
NotifyState LoadNotifyState();
void SaveNotifyState(const NotifyState& state);
long long UnixNow();

}  // namespace wram::updater
