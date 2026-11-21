#pragma once

#include <string>
#include <string_view>

#include "../platform/Win.h"

struct ICoreWebView2;
struct ICoreWebView2Controller;
struct ICoreWebView2Environment;

namespace wram {

class ISettingsHost {
public:
    virtual ~ISettingsHost() = default;
    virtual std::string BuildInitMessage() = 0;            // JSON con configuración y catálogo
    virtual void OnSettingsApply(std::string_view ini) = 0;  // cambios desde la interfaz
    virtual void OnSettingsReset() = 0;
    virtual void OnSettingsClosed() = 0;
    virtual void OnSettingsOpen(std::string_view target) = 0;  // "config", "repo" o "license"
};

// Ventana de configuración. La interfaz es un HTML embebido en el ejecutable y
// mostrado con WebView2. El navegador solo existe mientras la ventana está abierta;
// al cerrarla se liberan sus procesos y la memoria vuelve a unos pocos MB.
class SettingsWindow {
public:
    explicit SettingsWindow(ISettingsHost& host) : host_(host) {}
    ~SettingsWindow();
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    void Show(HINSTANCE instance);
    bool IsOpen() const { return hwnd_ != nullptr; }
    void SetDarkTheme(bool dark);
    static void PurgeUserData();
    void Post(const std::string& json);
    void Close();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void CreateBrowser();
    void OnControllerReady(ICoreWebView2Controller* controller);
    void OnMessage(std::wstring_view message);
    void ResizeBrowser();
    void ReleaseBrowser();
    void ShowError(const wchar_t* text);
    void ApplyChrome();

    ISettingsHost& host_;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    ICoreWebView2Environment* env_ = nullptr;
    ICoreWebView2Controller* controller_ = nullptr;
    ICoreWebView2* webview_ = nullptr;
    bool contentLoaded_ = false;
    bool dark_ = true;
    unsigned generation_ = 0;  // invalida callbacks asíncronos de una ventana ya cerrada
};

}  // namespace wram
