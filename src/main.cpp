#include "app/App.h"

#include <objbase.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    // Una sola instancia por sesión. Una segunda ejecución abre la configuración de la primera.
    wram::UniqueHandle mutex(CreateMutexW(nullptr, TRUE, L"Local\\WRAM.SingleInstance"));
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND host = FindWindowW(wram::kHostClassName, nullptr)) PostMessageW(host, wram::kMsgShowSettings, 0, 0);
        return 0;
    }

    // WebView2 exige un hilo STA.
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return 1;
    int code = 0;
    {
        wram::App app(instance);
        code = app.Run();
    }
    CoUninitialize();
    return code;
}
