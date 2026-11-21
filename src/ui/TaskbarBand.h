#pragma once

#include <string>
#include <vector>

#include "../core/Config.h"
#include "../platform/Win.h"

#include <d2d1.h>
#include <dwrite.h>

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

    bool Create(HINSTANCE instance);
    void ApplyConfig(const Config& config);
    void Update(const MetricFrame& frame);
    void Reposition();
    void RaiseAboveTaskbar();
    void OnSystemChange();  // tema, DPI o explorer reiniciado

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
    };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static void CALLBACK OnWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD);
    void OnLocationChanged();
    LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    bool EnsureFactories();
    void RebuildFormats();
    float Measure(IDWriteTextFormat* format, const std::wstring& text) const;
    void Layout();
    void Render();
    void ResizeSurface(int width, int height);
    void ReadTheme();
    bool ShouldHide() const;

    IBandEvents& events_;
    HWND hwnd_ = nullptr;
    HWINEVENTHOOK hook_ = nullptr;
    HWINEVENTHOOK locationHook_ = nullptr;
    bool hiddenForFullscreen_ = false;
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
    bool lightTheme_ = false;
    bool visible_ = false;
    RECT lastRect_{};
};

}  // namespace wram
