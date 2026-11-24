#include "TrayIcon.h"

#include <cwchar>

namespace wram {

bool TrayIcon::Add(HWND owner, UINT callbackMessage, HICON icon, const wchar_t* tooltip) {
    data_ = {};
    data_.cbSize = sizeof data_;
    data_.hWnd = owner;
    data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = callbackMessage;
    data_.hIcon = icon;
    wcsncpy_s(data_.szTip, tooltip, _TRUNCATE);
    return Readd();
}

bool TrayIcon::Readd() {
    if (!data_.hWnd) return false;
    Shell_NotifyIconW(NIM_DELETE, &data_);
    added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
    if (added_) {
        data_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data_);
    }
    return added_;
}

void TrayIcon::SetIcon(HICON icon) {
    data_.hIcon = icon;
    if (!added_) return;
    NOTIFYICONDATAW change = data_;
    change.uFlags = NIF_ICON;
    Shell_NotifyIconW(NIM_MODIFY, &change);
}

void TrayIcon::Notify(const wchar_t* title, const wchar_t* text, HICON largeIcon) {
    if (!added_) return;
    NOTIFYICONDATAW note = data_;
    note.uFlags = NIF_INFO;
    note.dwInfoFlags = largeIcon ? (NIIF_USER | NIIF_LARGE_ICON) : NIIF_INFO;
    note.hBalloonIcon = largeIcon;
    wcsncpy_s(note.szInfoTitle, title, _TRUNCATE);
    wcsncpy_s(note.szInfo, text, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &note);
}

void TrayIcon::Remove() {
    if (added_) Shell_NotifyIconW(NIM_DELETE, &data_);
    added_ = false;
}

}  // namespace wram
