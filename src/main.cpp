#include "app/App.h"

#include <objbase.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    // Tras actualizar, el ejecutable nuevo espera a que termine el anterior antes de tomar la instancia única.
    const bool justUpdated = wram::updater::WaitForPreviousInstance(GetCommandLineW());

    // Una sola instancia por sesión. Una segunda ejecución abre la configuración de la primera.
    wram::UniqueHandle mutex(CreateMutexW(nullptr, TRUE, L"Local\\WRAM.SingleInstance"));
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND host = FindWindowW(wram::kHostClassName, nullptr)) PostMessageW(host, wram::kMsgShowSettings, 0, 0);
        return 0;
    }

    // WebView2 exige un hilo STA.
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return 1;
    int code = 0;
    bool restart = false;
    {
        wram::App app(instance, justUpdated);
        code = app.Run();
        restart = app.RestartAfterExit();
    }
    CoUninitialize();
    // Tras instalar una actualización: se suelta la instancia única y recién entonces arranca la versión nueva.
    mutex.reset();
    if (restart) wram::updater::LaunchUpdated();
    return code;
}
