#include "Autostart.h"

#include <string>

#include "Win.h"

namespace wram::autostart {
namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"WRAM";

std::wstring QuotedExePath() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (n == 0) return {};
        if (n < path.size()) {
            path.resize(n);
            break;
        }
        path.resize(path.size() * 2);
    }
    return L"\"" + path + L"\"";
}

}  // namespace

bool IsEnabled() {
    wchar_t buf[1024];
    DWORD size = sizeof buf;
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) {
        return false;
    }
    return QuotedExePath() == buf;
}

bool SetEnabled(bool enabled) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return false;
    LSTATUS status;
    if (enabled) {
        const std::wstring cmd = QuotedExePath();
        status = RegSetValueExW(key, kValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()),
                                static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, kValueName);
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

}  // namespace wram::autostart
