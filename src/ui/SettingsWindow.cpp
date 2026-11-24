#include "SettingsWindow.h"

#include <algorithm>

#include <compressapi.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wrl.h>

#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include "../../res/resource.h"
#include "../core/I18n.h"
#include "../core/Text.h"

using Microsoft::WRL::Callback;

namespace wram {
namespace {

constexpr wchar_t kClassName[] = L"WRAM.Settings";
constexpr size_t kMaxMessageChars = 32 * 1024;
constexpr int kWidth = 1080;
constexpr int kHeight = 720;
constexpr int kMinWidth = 820;
constexpr int kMinHeight = 560;
struct ChromeColors {
    COLORREF caption;
    COLORREF text;
    COREWEBVIEW2_COLOR page;
};

// Mismos fondos que usa la interfaz en cada tema, para que barra de título y página sean una sola superficie.
ChromeColors ColorsFor(bool dark) {
    return dark ? ChromeColors{RGB(0x20, 0x20, 0x20), RGB(0xFF, 0xFF, 0xFF), {255, 0x20, 0x20, 0x20}}
                : ChromeColors{RGB(0xF3, 0xF3, 0xF3), RGB(0x1A, 0x1A, 0x1A), {255, 0xF3, 0xF3, 0xF3}};
}

// DWMWINDOWATTRIBUTE disponibles desde Windows 11; se declaran por compatibilidad con SDK antiguos.
constexpr DWORD kDwmUseDarkMode = 20;
constexpr DWORD kDwmBorderColor = 34;
constexpr DWORD kDwmCaptionColor = 35;
constexpr DWORD kDwmTextColor = 36;

std::wstring LoadEmbeddedHtml(HINSTANCE instance) {
    HRSRC res = FindResourceW(instance, MAKEINTRESOURCEW(IDR_SETTINGS_HTML), RT_RCDATA);
    if (!res) return {};
    HGLOBAL handle = LoadResource(instance, res);
    if (!handle) return {};
    const DWORD size = SizeofResource(instance, res);
    const auto* data = static_cast<const unsigned char*>(LockResource(handle));
    if (!data || size < 2) return {};
    // El recurso viene comprimido (ver tools/pack.cpp): 1 byte con el algoritmo y el bloque comprimido.
    const DWORD algorithm = data[0];
    if (algorithm != COMPRESS_ALGORITHM_XPRESS_HUFF && algorithm != COMPRESS_ALGORITHM_LZMS) return {};
    DECOMPRESSOR_HANDLE d = nullptr;
    if (!CreateDecompressor(algorithm, nullptr, &d)) return {};
    SIZE_T needed = 0;
    Decompress(d, data + 1, size - 1, nullptr, 0, &needed);
    std::string html;
    if (needed > 0 && needed <= 4 * 1024 * 1024) {
        html.resize(needed);
        SIZE_T written = 0;
        if (!Decompress(d, data + 1, size - 1, html.data(), needed, &written)) written = 0;
        html.resize(written);
    }
    CloseDecompressor(d);
    return html.empty() ? std::wstring() : text::Utf8ToWide(html);
}

std::wstring UserDataFolder() {
    std::wstring folder;
    PWSTR local = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local))) {
        folder = local;
        CoTaskMemFree(local);
    }
    return folder + L"\\WRAM\\WebView2";
}

template <typename T>
void SafeRelease(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

}  // namespace

SettingsWindow::~SettingsWindow() { Close(); }

void SettingsWindow::SetDarkTheme(bool dark) {
    if (dark == dark_) return;
    dark_ = dark;
    ApplyChrome();
}

void SettingsWindow::ApplyChrome() {
    if (!hwnd_) return;
    const ChromeColors c = ColorsFor(dark_);
    const BOOL dark = dark_ ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, kDwmUseDarkMode, &dark, sizeof dark);
    DwmSetWindowAttribute(hwnd_, kDwmCaptionColor, &c.caption, sizeof c.caption);
    DwmSetWindowAttribute(hwnd_, kDwmBorderColor, &c.caption, sizeof c.caption);
    DwmSetWindowAttribute(hwnd_, kDwmTextColor, &c.text, sizeof c.text);
    if (controller_) {
        ICoreWebView2Controller2* controller2 = nullptr;
        if (SUCCEEDED(controller_->QueryInterface(IID_PPV_ARGS(&controller2)))) {
            controller2->put_DefaultBackgroundColor(c.page);
            controller2->Release();
        }
    }
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void SettingsWindow::Show(HINSTANCE instance) {
    if (hwnd_) {
        if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
        return;
    }
    instance_ = instance;
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof wc};
        wc.lpfnWndProc = &SettingsWindow::WndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;  // se pinta en WM_ERASEBKGND con el color del tema
        wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
        wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
        wc.lpszClassName = kClassName;
        registered = RegisterClassExW(&wc) != 0;
    }

    POINT cursor;
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{sizeof mi};
    GetMonitorInfoW(monitor, &mi);
    UINT dpiX = 96, dpiY = 96;
    using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    if (HMODULE shcore = GetModuleHandleW(L"shcore.dll")) {
        if (auto fn = reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore, "GetDpiForMonitor"))) {
            fn(monitor, 0, &dpiX, &dpiY);
        }
    }
    const RECT& work = mi.rcWork;
    const int w = std::min<int>(MulDiv(kWidth, dpiX, 96), work.right - work.left);
    const int h = std::min<int>(MulDiv(kHeight, dpiX, 96), work.bottom - work.top);
    const int x = work.left + (work.right - work.left - w) / 2;
    const int y = work.top + (work.bottom - work.top - h) / 2;

    hwnd_ = CreateWindowExW(0, kClassName, L"WRAM", WS_OVERLAPPEDWINDOW, x, y, w, h, nullptr, nullptr, instance, this);
    if (!hwnd_) return;

    ApplyChrome();

    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
    CreateBrowser();
}

void SettingsWindow::CreateBrowser() {
    const unsigned generation = ++generation_;
    const std::wstring folder = UserDataFolder();
    // La interfaz es local y estática: sin descargas de componentes (DRM, filtros, voz) ni cachés en disco.
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(
        L"--disable-component-update --disable-background-networking --disable-gpu-shader-disk-cache "
        L"--disk-cache-size=1 --no-pings --disable-features=msSmartScreenProtection,SpareRendererForSitePerProcess");
    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, folder.c_str(), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, generation](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (generation != generation_ || !hwnd_) return S_OK;
                if (FAILED(result) || !env) {
                    ShowError(Text(Str::ErrWebViewStart));
                    return S_OK;
                }
                env_ = env;
                env_->AddRef();
                return env_->CreateCoreWebView2Controller(
                    hwnd_, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                               [this, generation](HRESULT r, ICoreWebView2Controller* controller) -> HRESULT {
                                   if (generation != generation_ || !hwnd_) {
                                       if (controller) controller->Close();
                                       return S_OK;
                                   }
                                   if (FAILED(r) || !controller) {
                                       ShowError(Text(Str::ErrView));
                                       return S_OK;
                                   }
                                   OnControllerReady(controller);
                                   return S_OK;
                               })
                               .Get());
            })
            .Get());
    if (FAILED(hr)) {
        ShowError(Text(Str::ErrWebViewMissing));
    }
}

void SettingsWindow::OnControllerReady(ICoreWebView2Controller* controller) {
    controller_ = controller;
    controller_->AddRef();
    if (FAILED(controller_->get_CoreWebView2(&webview_)) || !webview_) {
        ShowError(Text(Str::ErrView));
        return;
    }

    ICoreWebView2Controller2* controller2 = nullptr;
    if (SUCCEEDED(controller_->QueryInterface(IID_PPV_ARGS(&controller2)))) {
        controller2->put_DefaultBackgroundColor(ColorsFor(dark_).page);  // sin destello al abrir
        controller2->Release();
    }

    // Superficie mínima: sin herramientas de desarrollo, menús, zoom, autocompletado ni objetos del host.
    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(webview_->get_Settings(&settings))) {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(FALSE);
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);
        settings->put_AreHostObjectsAllowed(FALSE);
        settings->put_IsBuiltInErrorPageEnabled(FALSE);
#ifdef NDEBUG
        settings->put_AreDevToolsEnabled(FALSE);
#endif
        ICoreWebView2Settings3* s3 = nullptr;
        if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&s3)))) {
            s3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
            s3->Release();
        }
        ICoreWebView2Settings4* s4 = nullptr;
        if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&s4)))) {
            s4->put_IsGeneralAutofillEnabled(FALSE);
            s4->put_IsPasswordAutosaveEnabled(FALSE);
            s4->Release();
        }
        ICoreWebView2Settings5* s5 = nullptr;
        if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&s5)))) {
            s5->put_IsPinchZoomEnabled(FALSE);
            s5->Release();
        }
        settings->Release();
    }

    EventRegistrationToken token{};
    // Solo se permite la navegación inicial al HTML embebido; cualquier otra se cancela.
    webview_->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                if (contentLoaded_) args->put_Cancel(TRUE);
                contentLoaded_ = true;
                return S_OK;
            })
            .Get(),
        &token);
    webview_->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                args->put_Handled(TRUE);
                return S_OK;
            })
            .Get(),
        &token);
    webview_->add_PermissionRequested(
        Callback<ICoreWebView2PermissionRequestedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
                args->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);
                return S_OK;
            })
            .Get(),
        &token);
    webview_->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR message = nullptr;
                if (SUCCEEDED(args->TryGetWebMessageAsString(&message)) && message) {
                    OnMessage(message);
                    CoTaskMemFree(message);
                }
                return S_OK;
            })
            .Get(),
        &token);

    ResizeBrowser();
    const std::wstring html = LoadEmbeddedHtml(instance_);
    if (html.empty()) {
        ShowError(Text(Str::ErrCorrupt));
        return;
    }
    contentLoaded_ = false;
    webview_->NavigateToString(html.c_str());
}

void SettingsWindow::OnMessage(std::wstring_view message) {
    if (message.size() > kMaxMessageChars) return;
    if (message == L"ready") {
        Post(host_.BuildInitMessage());
    } else if (message.rfind(L"apply\n", 0) == 0) {
        const std::string ini = text::WideToUtf8(message.substr(6));
        host_.OnSettingsApply(ini);
    } else if (message == L"reset") {
        host_.OnSettingsReset();
    } else if (message == L"close") {
        Close();
    } else if (message.rfind(L"open\n", 0) == 0) {
        host_.OnSettingsOpen(text::WideToUtf8(message.substr(5)));
    } else if (message == L"update\ncheck" || message == L"update\ninstall") {
        host_.OnSettingsUpdate(message.substr(7) == L"check" ? "check" : "install");
    }
}

void SettingsWindow::Post(const std::string& json) {
    if (!webview_) return;
    webview_->PostWebMessageAsJson(text::Utf8ToWide(json).c_str());
}

void SettingsWindow::ResizeBrowser() {
    if (!controller_ || !hwnd_) return;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    controller_->put_Bounds(rc);
}

void SettingsWindow::PurgeUserData() {
    // Perfil temporal de WebView2: se borra con la ventana cerrada para no dejar datos en disco.
    std::wstring from = UserDataFolder();
    if (GetFileAttributesW(from.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    from.push_back(L'\0');  // SHFileOperation espera una lista terminada en doble nulo
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_NO_UI;
    SHFileOperationW(&op);
}

void SettingsWindow::ReleaseBrowser() {
    ++generation_;
    if (controller_) controller_->Close();
    SafeRelease(webview_);
    SafeRelease(controller_);
    SafeRelease(env_);
    contentLoaded_ = false;
}

void SettingsWindow::ShowError(const wchar_t* text) {
    HWND owner = hwnd_;
    MessageBoxW(owner, text, L"WRAM", MB_OK | MB_ICONWARNING);
    Close();
}

void SettingsWindow::Close() {
    if (hwnd_) DestroyWindow(hwnd_);
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }
    auto* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->Handle(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT SettingsWindow::Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE: ResizeBrowser(); return 0;
        case WM_ERASEBKGND: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH brush = CreateSolidBrush(ColorsFor(dark_).caption);
            FillRect(reinterpret_cast<HDC>(wp), &rc, brush);
            DeleteObject(brush);
            return 1;
        }
        case WM_MOVE:
            if (controller_) controller_->NotifyParentWindowPositionChanged();
            return 0;
        case WM_SETFOCUS:
            if (controller_) controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            return 0;
        case WM_GETMINMAXINFO: {
            const UINT dpi = GetDpiForWindow(hwnd);
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize = {MulDiv(kMinWidth, dpi, 96), MulDiv(kMinHeight, dpi, 96)};
            return 0;
        }
        case WM_DPICHANGED: {
            const auto* r = reinterpret_cast<const RECT*>(lp);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_DESTROY:
            ReleaseBrowser();
            hwnd_ = nullptr;
            host_.OnSettingsClosed();
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace wram
