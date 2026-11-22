#pragma once

#include <string>
#include <vector>

#include "../core/Config.h"
#include "../platform/Win.h"

#include <d2d1.h>
#include <dwrite.h>
#include <UIAutomation.h>

namespace wram {

class IBandEvents {
public:
    virtual ~IBandEvents() = default;
    virtual void OnBandActivate() = 0;
    virtual void OnBandContextMenu(POINT screenPt) = 0;
};

// Franja transparente que se dibuja sobre la barra de tareas, a la izquierda de
// la bandeja del sistema. Ventana por capas con alfa por píxel (UpdateLayeredWindow)
// y texto con Direct2D/DirectWrite: sin bordes, sin parpadeo y sin robar el foco.
class TaskbarBand {
public:
    explicit TaskbarBand(IBandEvents& events) : events_(events) {}
    ~TaskbarBand();
    TaskbarBand(const TaskbarBand&) = delete;
    TaskbarBand& operator=(const TaskbarBand&) = delete;

    bool Create(HINSTANCE instance, HWND taskbar, bool primary);
    HWND Taskbar() const { return taskbar_; }
    bool IsPrimary() const { return primary_; }
    void ApplyConfig(const Config& config);
    void Update(const MetricFrame& frame);
    void Reposition();
    void RaiseAboveTaskbar();
    bool CreateBandWindow();
    void OnSystemChange();  // tema, DPI o explorer reiniciado
    void RefreshTaskbarApps();  // dónde terminan los íconos de las apps en la barra
    int MaxOffsetDip() const { return maxOffsetDip_; }
    int AppsGapDip() const { return appsGapDip_; }  // espacio libre entre los íconos de las apps y la bandeja, -1 si no se sabe

private:
    struct Cell {
        MetricId id;
        std::wstring label;
        std::wstring value;
        bool alert = false;
        float labelWidth = 0;
        float valueWidth = 0;
        float x = 0;
        float y = 0;
        float columnWidth = 0;
        float labelY = 0;
    };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static void CALLBACK OnWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD);
    void OnLocationChanged();
    static void WatchForeground(HWND fg);
    static void WatchTaskbar();
    enum class Fullscreen { None, Window, Exclusive };
    Fullscreen DetectFullscreen(RECT& monitor) const;
    LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    bool EnsureFactories();
    void RebuildFormats();
    void FitToBar(int thickness);
    float Measure(IDWriteTextFormat* format, const std::wstring& text) const;
    void Layout();
    void Render();
    void ResizeSurface(int width, int height);
    void ReadTheme();


    IBandEvents& events_;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    Fullscreen fullscreen_ = Fullscreen::None;
    bool overlay_ = false;
    Config config_;
    std::vector<Cell> cells_;

    ComPtr<ID2D1Factory> d2d_;
    ComPtr<IDWriteFactory> dwrite_;
    ComPtr<ID2D1DCRenderTarget> target_;
    ComPtr<IDWriteTextFormat> labelFormat_;
    ComPtr<IDWriteTextFormat> valueFormat_;

    HDC memDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    int surfaceW_ = 0;
    int surfaceH_ = 0;

    float scale_ = 1.0f;
    int bandW_ = 0;
    int bandH_ = 0;
    float lineHeight_ = 0;
    float valueBaseline_ = 0;
    float labelBaseline_ = 0;
    float labelLineHeight_ = 0;
    float fontScale_ = 1.0f;
    bool vertical_ = false;
    int barThickness_ = 0;
    HWND taskbar_ = nullptr;   // barra que acompaña esta franja
    bool primary_ = true;
    RECT secondaryTray_{};     // zona del reloj en barras de otras pantallas, medida con UI Automation
    bool lightTheme_ = false;
    bool visible_ = false;
    RECT lastRect_{};
    static constexpr LONG kNoAppsEdge = -2147483647 - 1;
    LONG appsRight_ = kNoAppsEdge;  // borde de los íconos de apps: x en barra horizontal, y en vertical
    int maxOffsetDip_ = Config::kMaxOffset;
    int appsGapDip_ = -1;
    ULONGLONG lastAppsScan_ = 0;
    ComPtr<IUIAutomation> uia_;
};

}  // namespace wram
