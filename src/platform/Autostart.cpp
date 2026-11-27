#include "Autostart.h"

#include <string>

#include "Package.h"
#include "Win.h"

#include <roapi.h>
#include <windows.applicationmodel.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

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

// Paquete de Microsoft Store: la clave Run queda virtualizada, así que se usa la tarea de
// inicio declarada en AppxManifest.xml (windows.startupTask, TaskId "WRAMStartup").
namespace abi = ABI::Windows::ApplicationModel;
namespace af = ABI::Windows::Foundation;
using Microsoft::WRL::ComPtr;

constexpr wchar_t kTaskId[] = L"WRAMStartup";

// Espera una operación asíncrona de WinRT (las de StartupTask terminan en milisegundos).
template <typename Op>
bool Wait(Op* op) {
    ComPtr<af::IAsyncInfo> info;
    if (FAILED(op->QueryInterface(IID_PPV_ARGS(&info)))) return false;
    for (int i = 0; i < 300; ++i) {
        AsyncStatus status = AsyncStatus::Started;
        if (FAILED(info->get_Status(&status))) return false;
        if (status == AsyncStatus::Completed) return true;
        if (status != AsyncStatus::Started) return false;
        Sleep(10);
    }
    return false;
}

ComPtr<abi::IStartupTask> StartupTask() {
    ComPtr<abi::IStartupTaskStatics> statics;
    if (FAILED(RoGetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_ApplicationModel_StartupTask).Get(),
                                      IID_PPV_ARGS(&statics)))) {
        return nullptr;
    }
    ComPtr<af::IAsyncOperation<abi::StartupTask*>> op;
    if (FAILED(statics->GetAsync(Microsoft::WRL::Wrappers::HStringReference(kTaskId).Get(), &op)) || !Wait(op.Get())) return nullptr;
    ComPtr<abi::IStartupTask> task;
    if (FAILED(op->GetResults(&task))) return nullptr;
    return task;
}

bool TaskEnabled(abi::StartupTaskState state) {
    return state == abi::StartupTaskState_Enabled || state == abi::StartupTaskState_EnabledByPolicy;
}

bool PackagedIsEnabled() {
    const ComPtr<abi::IStartupTask> task = StartupTask();
    abi::StartupTaskState state = abi::StartupTaskState_Disabled;
    return task && SUCCEEDED(task->get_State(&state)) && TaskEnabled(state);
}

bool PackagedSetEnabled(bool enabled) {
    const ComPtr<abi::IStartupTask> task = StartupTask();
    if (!task) return false;
    if (!enabled) return SUCCEEDED(task->Disable());
    // Si la persona lo desactivó en el Administrador de tareas, Windows no deja reactivarlo desde la app.
    ComPtr<af::IAsyncOperation<abi::StartupTaskState>> op;
    abi::StartupTaskState state = abi::StartupTaskState_Disabled;
    return SUCCEEDED(task->RequestEnableAsync(&op)) && Wait(op.Get()) && SUCCEEDED(op->GetResults(&state)) && TaskEnabled(state);
}

}  // namespace

bool IsEnabled() {
    if (package::IsPackaged()) return PackagedIsEnabled();
    wchar_t buf[1024];
    DWORD size = sizeof buf;
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) {
        return false;
    }
    return QuotedExePath() == buf;
}

bool SetEnabled(bool enabled) {
    if (package::IsPackaged()) return PackagedSetEnabled(enabled);
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
