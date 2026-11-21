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

void TrayIcon::Remove() {
    if (added_) Shell_NotifyIconW(NIM_DELETE, &data_);
    added_ = false;
}

}  // namespace wram
