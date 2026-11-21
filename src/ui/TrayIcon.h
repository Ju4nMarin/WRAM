#pragma once

#include "../platform/Win.h"

#include <shellapi.h>

namespace wram {

// Ícono en el área de notificación. Los eventos llegan a la ventana dueña como
// `callbackMessage` (NOTIFYICON_VERSION_4: LOWORD(lParam) = evento).
class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon() { Remove(); }
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    bool Add(HWND owner, UINT callbackMessage, HICON icon, const wchar_t* tooltip);
    void Remove();
    bool Readd();  // tras reiniciarse explorer.exe

private:
    NOTIFYICONDATAW data_{};
    bool added_ = false;
};

}  // namespace wram
