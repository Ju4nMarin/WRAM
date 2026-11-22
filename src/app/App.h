#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../core/Config.h"
#include "../platform/ConfigStore.h"
#include "../platform/Win.h"
#include "../sensors/Sampler.h"
#include "../ui/SettingsWindow.h"
#include "../ui/TaskbarBand.h"
#include "../ui/TrayIcon.h"

namespace wram {

constexpr wchar_t kHostClassName[] = L"WRAM.Host";
constexpr UINT kMsgShowSettings = WM_APP + 2;

// Raíz de composición: crea las piezas, las conecta y reparte los eventos.
// Ningún componente conoce a los demás; solo dependen de interfaces.
class App final : public IBandEvents, public ISettingsHost {
public:
    explicit App(HINSTANCE instance);
    ~App() override;
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int Run();

private:
    static LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void Tick();
    void ApplyConfig(Config config, bool persist);
    void RefreshSamplerMask();
    void RestartTimer();
    void ShowMenu(POINT pt);
    std::string ValuesMessage(const MetricFrame& frame) const;

    void OnBandActivate() override;
    void OnBandContextMenu(POINT screenPt) override;

    std::string BuildInitMessage() override;
    void OnSettingsApply(std::string_view ini) override;
    void OnSettingsReset() override;
    void OnSettingsClosed() override;
    void OnSettingsOpen(std::string_view target) override;
    bool SettingsUseDarkTheme() const;
    void SyncBands();
    void ScanTaskbars();
    void StartSettle();
    void OnSystemChange();
    TaskbarBand& Primary() const { return *bands_.front(); }

    HINSTANCE instance_;
    HWND host_ = nullptr;
    HICON icon_ = nullptr;
    UINT taskbarCreatedMsg_ = 0;
    ULONGLONG lastAppsScan_ = 0;
    ULONGLONG settleUntil_ = 0;
    RECT lastBarRect_{};
    static constexpr DWORD kUnknownLayout = 0xFFFFFFFF;
    DWORD lastTaskbarLayout_ = kUnknownLayout;  // alineación y tamaño de la barra, para detectar cambios
    ConfigStore store_;
    Config config_;
    Sampler sampler_;
    std::vector<std::unique_ptr<TaskbarBand>> bands_;  // la primera es la barra principal
    TrayIcon tray_;
    SettingsWindow settings_;
};

}  // namespace wram
