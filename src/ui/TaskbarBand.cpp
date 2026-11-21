#include "TaskbarBand.h"

#include <algorithm>
#include <cmath>

#include <shellapi.h>
#include <UIAutomation.h>

#include "../core/Format.h"
#include "../core/MetricCatalog.h"

namespace wram {
namespace {

constexpr wchar_t kClassName[] = L"WRAM.Band";
TaskbarBand* g_band = nullptr;  // destino del hook de foco (una sola franja por proceso)

D2D1_COLOR_F Rgb(uint32_t rgb, float alpha = 1.0f) {
    return D2D1::ColorF(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha);
}

struct TaskbarGeometry {
    RECT bar{};
    RECT tray{};
    float scale = 1.0f;
    bool visible = false;
    bool vertical = false;  // barra a la izquierda o a la derecha
};

TaskbarGeometry ReadTaskbar() {
    TaskbarGeometry g;
    HWND bar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!bar || !GetWindowRect(bar, &g.bar)) return g;

    MONITORINFO mi{sizeof mi};
    GetMonitorInfoW(MonitorFromWindow(bar, MONITOR_DEFAULTTOPRIMARY), &mi);
    RECT visible{};
    IntersectRect(&visible, &g.bar, &mi.rcMonitor);
    // Con ocultar automáticamente, la barra queda casi fuera de pantalla.
    const LONG thickness = std::min(visible.right - visible.left, visible.bottom - visible.top);
    g.visible = IsWindowVisible(bar) && thickness >= 8;
    g.vertical = (g.bar.bottom - g.bar.top) > (g.bar.right - g.bar.left);
    g.bar = visible;

    HWND tray = FindWindowExW(bar, nullptr, L"TrayNotifyWnd", nullptr);
    if (!tray || !GetWindowRect(tray, &g.tray) || g.tray.right <= g.tray.left) {
        g.tray = g.vertical ? RECT{g.bar.left, g.bar.bottom, g.bar.right, g.bar.bottom}
                            : RECT{g.bar.right, g.bar.top, g.bar.right, g.bar.bottom};
    }
    const UINT dpi = GetDpiForWindow(bar);
    g.scale = (dpi ? dpi : 96) / 96.0f;
    return g;
}

bool IsShellOrOwnWindow(HWND hwnd) {
    wchar_t cls[64]{};
    GetClassNameW(hwnd, cls, 64);
    static constexpr const wchar_t* kIgnored[] = {L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
                                                  L"WRAM.Band", L"WRAM.Settings", L"WRAM.Host"};
    for (const wchar_t* name : kIgnored) {
        if (wcscmp(cls, name) == 0) return true;
    }
    return false;
}

// Una ventana en primer plano que cubre todo el monitor de la barra (video, juego, F11 del navegador).
// Se calcula sin esperar al temporizador para reaccionar en el mismo instante.
bool ForegroundIsFullscreen(RECT& monitorRect) {
    HWND fg = GetForegroundWindow();
    if (!fg || IsShellOrOwnWindow(fg)) return false;
    HWND bar = FindWindowW(L"Shell_TrayWnd", nullptr);
    HMONITOR monitor = MonitorFromWindow(bar ? bar : fg, MONITOR_DEFAULTTOPRIMARY);
    if (MonitorFromWindow(fg, MONITOR_DEFAULTTONULL) != monitor) return false;
    const LONG style = GetWindowLongW(fg, GWL_STYLE);
    if (IsZoomed(fg) && (style & WS_CAPTION) == WS_CAPTION) return false;  // maximizada normal, no pantalla completa
    RECT wr{};
    MONITORINFO mi{sizeof mi};
    if (!GetWindowRect(fg, &wr) || !GetMonitorInfoW(monitor, &mi)) return false;
    monitorRect = mi.rcMonitor;
    return wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top && wr.right >= mi.rcMonitor.right &&
           wr.bottom >= mi.rcMonitor.bottom;
}

// Juegos Direct3D exclusivos y presentaciones: ninguna ventana puede dibujarse encima.
bool ExclusiveFullscreen() {
    QUERY_USER_NOTIFICATION_STATE state{};
    if (FAILED(SHQueryUserNotificationState(&state))) return false;
    return state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE;
}

}  // namespace

TaskbarBand::~TaskbarBand() {
    if (hook_) UnhookWinEvent(hook_);
    if (locationHook_) UnhookWinEvent(locationHook_);
    if (barHook_) UnhookWinEvent(barHook_);
    if (memDc_) {
        if (oldBitmap_) SelectObject(memDc_, oldBitmap_);
        DeleteDC(memDc_);
    }
    if (bitmap_) DeleteObject(bitmap_);
    if (hwnd_) DestroyWindow(hwnd_);
    if (g_band == this) g_band = nullptr;
}

bool TaskbarBand::Create(HINSTANCE instance) {
    WNDCLASSEXW wc{sizeof wc};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = &TaskbarBand::WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, kClassName, L"WRAM",
                            WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, this);
    if (!hwnd_ || !EnsureFactories()) return false;

    g_band = this;
    constexpr DWORD kFlags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
    hook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, &TaskbarBand::OnWinEvent, 0, 0,
                            kFlags);
    WatchForeground(GetForegroundWindow());
    WatchTaskbar();
    ReadTheme();
    return true;
}

void TaskbarBand::WatchTaskbar() {
    if (barHook_) UnhookWinEvent(barHook_);
    barHook_ = nullptr;
    barHwnd_ = FindWindowW(L"Shell_TrayWnd", nullptr);
    // Solo con "Ocultar automáticamente la barra de tareas" la barra entra y sale de la pantalla; sin esa opción
    // escuchar sus movimientos internos solo gastaría CPU.
    APPBARDATA abd{sizeof abd};
    const bool autoHide = (SHAppBarMessage(ABM_GETSTATE, &abd) & ABS_AUTOHIDE) != 0;
    DWORD pid = 0;
    const DWORD tid = barHwnd_ ? GetWindowThreadProcessId(barHwnd_, &pid) : 0;
    if (!tid || !autoHide) return;
    barHook_ = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
                               &TaskbarBand::OnWinEvent, pid, tid, WINEVENT_OUTOFCONTEXT);
}

void TaskbarBand::WatchForeground(HWND fg) {
    // Entrar o salir de pantalla completa (F11, doble clic en un video) cambia el tamaño sin cambiar el foco.
    // El gancho se limita al hilo de la ventana activa: escuchar a todo el sistema despertaría el proceso
    // con cada parpadeo de cursor de cualquier aplicación.
    if (locationHook_) UnhookWinEvent(locationHook_);
    locationHook_ = nullptr;
    DWORD pid = 0;
    const DWORD tid = fg ? GetWindowThreadProcessId(fg, &pid) : 0;
    if (!tid || pid == GetCurrentProcessId()) return;
    locationHook_ = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
                                    &TaskbarBand::OnWinEvent, pid, tid, WINEVENT_OUTOFCONTEXT);
}

bool TaskbarBand::EnsureFactories() {
    if (!d2d_ && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d_.put()))) return false;
    if (!dwrite_ && FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                               reinterpret_cast<IUnknown**>(dwrite_.put())))) {
        return false;
    }
    if (!target_) {
        const auto props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE,  // superficie diminuta: la CPU es más barata que un dispositivo GPU
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
        if (FAILED(d2d_->CreateDCRenderTarget(&props, target_.put()))) return false;
        target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    }
    return true;
}

void TaskbarBand::ReadTheme() {
    DWORD light = 0;
    DWORD size = sizeof light;
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    lightTheme_ = light != 0;
}

void TaskbarBand::RebuildFormats() {
    labelFormat_.reset();
    valueFormat_.reset();
    const float valueSize = config_.fontSize * scale_ * fontScale_;
    const float labelSize = std::max(6.0f * scale_, valueSize * 0.8f);
    dwrite_->CreateTextFormat(config_.font.c_str(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                              DWRITE_FONT_STRETCH_NORMAL, labelSize, L"es-es", labelFormat_.put());
    dwrite_->CreateTextFormat(config_.font.c_str(), nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                              DWRITE_FONT_STRETCH_NORMAL, valueSize, L"es-es", valueFormat_.put());
    if (!labelFormat_ || !valueFormat_) return;
    // En una barra vertical cada métrica va centrada: etiqueta arriba y valor abajo.
    const bool stacked = vertical_ && !overlay_;
    valueFormat_->SetTextAlignment(stacked ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_TRAILING);
    labelFormat_->SetTextAlignment(stacked ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING);
    labelFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    valueFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    auto lineOf = [this](IDWriteTextFormat* format, float& height, float& baseline) {
        ComPtr<IDWriteTextLayout> layout;
        if (FAILED(dwrite_->CreateTextLayout(L"0", 1, format, 1000, 1000, layout.put()))) return;
        DWRITE_LINE_METRICS lm{};
        UINT32 count = 0;
        layout->GetLineMetrics(&lm, 1, &count);
        height = lm.height;
        baseline = lm.baseline;
    };
    lineOf(valueFormat_.get(), lineHeight_, valueBaseline_);
    lineOf(labelFormat_.get(), labelLineHeight_, labelBaseline_);
}

void TaskbarBand::FitToBar(int thickness) {
    // Barra pequeña o vertical: si el texto configurado no cabe, se reduce lo justo (hasta un 60 %).
    fontScale_ = 1.0f;
    RebuildFormats();
    if (overlay_ || thickness <= 0 || !valueFormat_) return;
    float factor = 1.0f;
    if (vertical_) {
        float widest = 0;
        for (const Cell& c : cells_) {
            widest = std::max(widest, Measure(valueFormat_.get(), WidestValue(Info(c.id).unit)));
            if (config_.showLabels) widest = std::max(widest, Measure(labelFormat_.get(), c.label));
        }
        const float available = static_cast<float>(thickness) - 8.0f * scale_;
        if (widest > available && widest > 0) factor = available / widest;
    } else {
        const float rows = static_cast<float>(std::min<size_t>(static_cast<size_t>(std::max(1, config_.rows)), cells_.size()));
        const float needed = lineHeight_ * rows;
        const float available = static_cast<float>(thickness) - 2.0f * scale_;
        if (needed > available && needed > 0) factor = available / needed;
    }
    if (factor < 0.999f) {
        fontScale_ = std::max(0.6f, factor);
        RebuildFormats();
    }
}

float TaskbarBand::Measure(IDWriteTextFormat* format, const std::wstring& text) const {
    if (!format || text.empty()) return 0;
    ComPtr<IDWriteTextLayout> layout;
    if (FAILED(dwrite_->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format, 4000, 1000,
                                         layout.put()))) {
        return 0;
    }
    DWRITE_TEXT_METRICS m{};
    layout->GetMetrics(&m);
    return m.widthIncludingTrailingWhitespace;
}

void TaskbarBand::ApplyConfig(const Config& config) {
    config_ = config;
    cells_.clear();
    for (const SlotConfig& slot : config_.slots) {
        Cell cell;
        cell.id = slot.id;
        cell.label = slot.label.empty() ? LabelOf(Info(slot.id)) : slot.label;
        cell.value = kNoValue;
        cells_.push_back(std::move(cell));
    }
    ReadTheme();
    FitToBar(barThickness_);
    bandH_ = 0;  // Reposition vuelve a hacer el diseño con la forma actual de la barra
    if (vertical_ && !overlay_) bandW_ = barThickness_;
    lastRect_ = {};
    Reposition();
}

void TaskbarBand::Layout() {
    if (!labelFormat_ || !valueFormat_) return;
    const float gap = 5.0f * scale_;
    const float columnGap = 14.0f * scale_;
    const float pad = (overlay_ ? 12.0f : 6.0f) * scale_;
    const size_t rows = static_cast<size_t>(std::max(1, config_.rows));

    for (Cell& c : cells_) {
        c.labelWidth = config_.showLabels ? Measure(labelFormat_.get(), c.label) : 0.0f;
        c.valueWidth = std::max(Measure(valueFormat_.get(), WidestValue(Info(c.id).unit)),
                                Measure(valueFormat_.get(), c.value));
    }
    if (vertical_ && !overlay_) {
        // Barra vertical: una métrica debajo de otra, con el ancho de la barra.
        const float rowGap = 6.0f * scale_;
        float y = pad;
        for (Cell& c : cells_) {
            c.x = 0;
            c.columnWidth = static_cast<float>(bandW_);
            c.labelY = y;
            if (config_.showLabels) y += labelLineHeight_;
            c.y = y;
            y += lineHeight_ + rowGap;
        }
        bandH_ = cells_.empty() ? 1 : static_cast<int>(std::ceil(y - rowGap + pad));
        return;
    }

    const size_t usedRows = std::min(rows, std::max<size_t>(1, cells_.size()));
    const float block = lineHeight_ * static_cast<float>(usedRows);
    const float top = std::floor((static_cast<float>(bandH_) - block) / 2.0f);

    float x = pad;
    for (size_t first = 0; first < cells_.size(); first += rows) {
        const size_t last = std::min(first + rows, cells_.size());
        float maxLabel = 0;
        float maxValue = 0;
        for (size_t i = first; i < last; ++i) {
            maxLabel = std::max(maxLabel, cells_[i].labelWidth);
            maxValue = std::max(maxValue, cells_[i].valueWidth);
        }
        const float width = maxLabel + (maxLabel > 0 ? gap : 0) + maxValue;
        for (size_t i = first; i < last; ++i) {
            cells_[i].x = x;
            cells_[i].y = top + lineHeight_ * static_cast<float>(i - first);
            cells_[i].labelY = cells_[i].y + valueBaseline_ - labelBaseline_;  // misma línea base que el valor
            cells_[i].columnWidth = width;
        }
        x += width + columnGap;
    }
    bandW_ = cells_.empty() ? 1 : static_cast<int>(std::ceil(x - columnGap + pad));
}

void TaskbarBand::Update(const MetricFrame& frame) {
    bool changed = false;
    bool grew = false;
    for (size_t i = 0; i < cells_.size(); ++i) {
        Cell& c = cells_[i];
        const MetricInfo& info = Info(c.id);
        const Sample& s = frame.Get(c.id);
        std::wstring value = s.valid ? FormatValue(info.unit, s.value) : kNoValue;
        const SlotConfig& slot = config_.slots[i];
        const bool alert = s.valid && slot.alert && IsAlert(info, s.value, slot.threshold);
        if (value != c.value || alert != c.alert) {
            if (valueFormat_ && Measure(valueFormat_.get(), value) > c.valueWidth + 0.5f) grew = true;
            c.value = std::move(value);
            c.alert = alert;
            changed = true;
        }
    }
    if (grew) {
        Layout();
        lastRect_ = {};
        Reposition();
    } else if (changed && visible_) {
        Render();
    }
}

TaskbarBand::Fullscreen TaskbarBand::DetectFullscreen(RECT& monitor) const {
    if (ForegroundIsFullscreen(monitor)) return Fullscreen::Window;
    if (ExclusiveFullscreen()) return Fullscreen::Exclusive;
    return Fullscreen::None;
}

void TaskbarBand::OnLocationChanged() {
    // Solo importa si cambia el estado de pantalla completa: así un arrastre de ventana no hace trabajo extra.
    RECT monitor{};
    if (DetectFullscreen(monitor) == fullscreen_) return;
    Reposition();
}

void TaskbarBand::Reposition() {
    if (!hwnd_) return;
    const TaskbarGeometry g = ReadTaskbar();
    RECT monitor{};
    fullscreen_ = DetectFullscreen(monitor);
    const auto mode = static_cast<FullscreenMode>(config_.fullscreenMode);

    // Con una ventana en pantalla completa la franja se oculta o se muestra flotando en la posición elegida.
    // Sobre un juego Direct3D exclusivo nada puede dibujarse encima, así que siempre se oculta.
    const bool overlay = fullscreen_ == Fullscreen::Window && mode == FullscreenMode::Visible;
    const bool hide = cells_.empty() || fullscreen_ == Fullscreen::Exclusive ||
                      (fullscreen_ == Fullscreen::Window && mode == FullscreenMode::Hidden) || (!overlay && !g.visible);
    if (hide) {
        if (visible_) ShowWindow(hwnd_, SW_HIDE);
        visible_ = false;
        return;
    }

    const int barH = g.bar.bottom - g.bar.top;
    const int barW = g.bar.right - g.bar.left;
    const bool horizontal = !g.vertical;
    const int thickness = horizontal ? barH : barW;

    // Cambios de escala, de orientación, de grosor de la barra (tamaño pequeño) o de modo flotante:
    // se ajusta el tamaño del texto para que quepa y se vuelve a medir.
    const bool shapeChanged = std::fabs(g.scale - scale_) > 0.01f || g.vertical != vertical_ ||
                              thickness != barThickness_ || overlay != overlay_;
    if (shapeChanged) {
        scale_ = g.scale;
        vertical_ = g.vertical;
        barThickness_ = thickness;
        overlay_ = overlay;
        FitToBar(thickness);
        bandH_ = 0;  // fuerza a recalcular el diseño
    }
    const float rows = static_cast<float>(std::min<size_t>(static_cast<size_t>(std::max(1, config_.rows)), cells_.size()));
    if (overlay_ || horizontal) {
        const int height = overlay_ ? static_cast<int>(std::ceil(lineHeight_ * rows + 16.0f * scale_)) : barH;
        if (height != bandH_) {
            bandH_ = height;
            Layout();
            lastRect_ = {};
        }
    } else if (bandW_ != barW || bandH_ == 0) {
        bandW_ = barW;  // en vertical el ancho lo da la barra y el alto sale del diseño
        Layout();
        lastRect_ = {};
    }

    RECT rc;
    if (overlay_) {
        // Posiciones: 0 arriba izq., 1 arriba centro, 2 arriba der., 3 centro izq., 4 centro der.,
        // 5 abajo izq., 6 abajo centro, 7 abajo der.
        const int margin = static_cast<int>(24 * scale_);
        const int pos = std::clamp(config_.overlayPosition, 0, 7);
        const int col = (pos == 0 || pos == 3 || pos == 5) ? 0 : (pos == 1 || pos == 6) ? 1 : 2;
        const int row = pos <= 2 ? 0 : (pos <= 4 ? 1 : 2);
        const LONG w = monitor.right - monitor.left;
        const LONG h = monitor.bottom - monitor.top;
        rc.left = col == 0 ? monitor.left + margin : col == 1 ? monitor.left + (w - bandW_) / 2 : monitor.right - margin - bandW_;
        rc.top = row == 0 ? monitor.top + margin : row == 1 ? monitor.top + (h - bandH_) / 2 : monitor.bottom - margin - bandH_;
    } else {
        const int offset = static_cast<int>(config_.offset * scale_);
        if (horizontal) {
            rc.left = std::max(g.bar.left, g.tray.left - offset - bandW_);
            // Nunca encima de los íconos de las aplicaciones, aunque la distancia a la bandeja sea grande.
            if (appsRight_ != kNoAppsEdge) rc.left = std::max<LONG>(rc.left, appsRight_ + static_cast<LONG>(8 * scale_));
            rc.top = g.bar.top;
        } else {
            // Barra a la izquierda o a la derecha: la franja va encima de la bandeja, que queda abajo.
            rc.left = g.bar.left;
            rc.top = std::max(g.bar.top, g.tray.top - offset - bandH_);
            if (appsRight_ != kNoAppsEdge) rc.top = std::max<LONG>(rc.top, appsRight_ + static_cast<LONG>(8 * scale_));
        }
    }
    rc.right = rc.left + bandW_;
    rc.bottom = rc.top + bandH_;

    if (!EqualRect(&rc, &lastRect_) || !visible_) {
        lastRect_ = rc;
        Render();
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        visible_ = true;
    }
    RaiseAboveTaskbar();
}

void TaskbarBand::RaiseAboveTaskbar() {
    if (visible_) {
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
}

void TaskbarBand::RefreshTaskbarApps() {
    // UI Automation entrega los botones de la barra (inicio, búsqueda y apps). Se toma el borde derecho del
    // último botón que queda a la izquierda de la bandeja. Es una consulta entre procesos: se hace pocas veces.
    lastAppsScan_ = GetTickCount64();
    const TaskbarGeometry g = ReadTaskbar();
    HWND bar = FindWindowW(L"Shell_TrayWnd", nullptr);
    LONG right = kNoAppsEdge;
    if (bar && g.visible) {
        if (!uia_) CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(uia_.put()));
        ComPtr<IUIAutomationElement> root;
        ComPtr<IUIAutomationCondition> isButton;
        ComPtr<IUIAutomationElementArray> buttons;
        VARIANT type{};
        type.vt = VT_I4;
        type.lVal = UIA_ButtonControlTypeId;
        if (uia_ && SUCCEEDED(uia_->ElementFromHandle(bar, root.put())) && root &&
            SUCCEEDED(uia_->CreatePropertyCondition(UIA_ControlTypePropertyId, type, isButton.put())) &&
            SUCCEEDED(root->FindAll(TreeScope_Descendants, isButton.get(), buttons.put())) && buttons) {
            int count = 0;
            buttons->get_Length(&count);
            for (int i = 0; i < count; ++i) {
                ComPtr<IUIAutomationElement> button;
                RECT r{};
                if (FAILED(buttons->GetElement(i, button.put())) || !button ||
                    FAILED(button->get_CurrentBoundingRectangle(&r))) {
                    continue;
                }
                if (r.right <= r.left || r.bottom <= r.top) continue;
                if (g.vertical) {
                    // Barra vertical: los íconos van de arriba hacia abajo hasta la bandeja.
                    if (r.bottom <= g.tray.top && r.left < g.bar.right && r.right > g.bar.left) right = std::max(right, r.bottom);
                } else if (r.right <= g.tray.left && r.top < g.bar.bottom && r.bottom > g.bar.top) {
                    right = std::max(right, r.right);
                }
            }
        }
    }
    const int extraBand = static_cast<int>(8 * scale_);
    const LONG trayEdge = g.vertical ? g.tray.top : g.tray.left;
    const int bandLength = g.vertical ? bandH_ : bandW_;
    maxOffsetDip_ = (right == kNoAppsEdge || scale_ <= 0)
                        ? Config::kMaxOffset
                        : std::max(0, static_cast<int>((trayEdge - right - extraBand - bandLength) / scale_));
    appsGapDip_ = (right == kNoAppsEdge || scale_ <= 0 || g.vertical) ? -1 : static_cast<int>((trayEdge - right) / scale_);
    if (right != appsRight_) {
        appsRight_ = right;
        Reposition();
    }
}

void TaskbarBand::OnSystemChange() {
    WatchTaskbar();  // explorer.exe pudo reiniciarse
    ReadTheme();
    scale_ = 0;  // fuerza a recalcular DPI y medidas
    lastRect_ = {};
    Reposition();
}

void TaskbarBand::ResizeSurface(int width, int height) {
    if (width == surfaceW_ && height == surfaceH_ && bitmap_) return;
    if (!memDc_) memDc_ = CreateCompatibleDC(nullptr);
    if (bitmap_) {
        SelectObject(memDc_, oldBitmap_);
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;  // de arriba hacia abajo
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    bitmap_ = CreateDIBSection(memDc_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    oldBitmap_ = SelectObject(memDc_, bitmap_);
    surfaceW_ = width;
    surfaceH_ = height;
}

void TaskbarBand::Render() {
    if (bandW_ <= 0 || bandH_ <= 0 || !EnsureFactories() || !valueFormat_) return;
    ResizeSurface(bandW_, bandH_);
    if (!bitmap_) return;

    const RECT bounds{0, 0, bandW_, bandH_};
    if (FAILED(target_->BindDC(memDc_, &bounds))) return;

    // Flotando sobre pantalla completa no hay barra detrás: fondo oscuro translúcido y texto claro.
    const bool light = lightTheme_ && !overlay_;
    const uint32_t valueRgb =
        config_.valueColor != Config::kAutoColor ? config_.valueColor : (light ? 0x1A1A1Au : 0xFFFFFFu);
    ComPtr<ID2D1SolidColorBrush> valueBrush, labelBrush, alertBrush, backBrush;
    target_->CreateSolidColorBrush(Rgb(valueRgb), valueBrush.put());
    target_->CreateSolidColorBrush(light ? Rgb(0x000000, 0.62f) : Rgb(0xFFFFFF, 0.68f), labelBrush.put());
    target_->CreateSolidColorBrush(Rgb(config_.alertColor), alertBrush.put());

    target_->BeginDraw();
    // Alfa mínimo en todo el rectángulo: invisible, pero la franja recibe los clics.
    target_->Clear(D2D1::ColorF(0, 0, 0, overlay_ ? 0.0f : 1.0f / 255.0f));
    if (overlay_) {
        target_->CreateSolidColorBrush(Rgb(0x141414, 0.72f), backBrush.put());
        const float r = 8.0f * scale_;
        target_->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(0, 0, static_cast<float>(bandW_), static_cast<float>(bandH_)), r, r),
            backBrush.get());
    }
    for (const Cell& c : cells_) {
        if (config_.showLabels && c.labelWidth > 0) {
            const float y = c.labelY;
            target_->DrawText(c.label.c_str(), static_cast<UINT32>(c.label.size()), labelFormat_.get(),
                              D2D1::RectF(c.x, y, c.x + c.columnWidth, y + lineHeight_), labelBrush.get(),
                              D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        target_->DrawText(c.value.c_str(), static_cast<UINT32>(c.value.size()), valueFormat_.get(),
                          D2D1::RectF(c.x, c.y, c.x + c.columnWidth, c.y + lineHeight_),
                          c.alert ? alertBrush.get() : valueBrush.get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    if (target_->EndDraw() == D2DERR_RECREATE_TARGET) {
        target_.reset();
        return;
    }

    POINT dst{lastRect_.left, lastRect_.top};
    SIZE size{bandW_, bandH_};
    POINT src{0, 0};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd_, nullptr, &dst, &size, memDc_, &src, 0, &blend, ULW_ALPHA);
}

void CALLBACK TaskbarBand::OnWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD,
                                      DWORD) {
    if (!g_band) return;
    if (event == EVENT_SYSTEM_FOREGROUND) {
        // Al hacer clic en la barra, Windows la sube en el orden Z; la franja vuelve encima.
        g_band->WatchForeground(hwnd);
        // Abrir o cerrar programas cambia los íconos de la barra; se vuelve a medir como mucho cada 2 s.
        if (GetTickCount64() - g_band->lastAppsScan_ > 2000) g_band->RefreshTaskbarApps();
        g_band->Reposition();
    } else if (idObject == OBJID_WINDOW && idChild == CHILDID_SELF && hwnd == g_band->barHwnd_) {
        // La barra se movió o apareció (ocultar automáticamente): la franja la sigue en el acto.
        g_band->Reposition();
    } else if (idObject == OBJID_WINDOW && idChild == CHILDID_SELF && hwnd == GetForegroundWindow()) {
        g_band->OnLocationChanged();
    }
}

LRESULT CALLBACK TaskbarBand::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }
    auto* self = reinterpret_cast<TaskbarBand*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->Handle(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT TaskbarBand::Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_LBUTTONDBLCLK: events_.OnBandActivate(); return 0;
        case WM_RBUTTONUP: {
            POINT pt;
            GetCursorPos(&pt);
            events_.OnBandContextMenu(pt);
            return 0;
        }
        case WM_NCDESTROY: hwnd_ = nullptr; break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace wram
