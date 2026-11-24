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
    void SetIcon(HICON icon);
    void Notify(const wchar_t* title, const wchar_t* text, HICON largeIcon);  // el clic llega como NIN_BALLOONUSERCLICK

private:
    NOTIFYICONDATAW data_{};
    bool added_ = false;
};

}  // namespace wram
