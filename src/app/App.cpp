#include "App.h"

#include <cmath>
#include <cstdio>
#include <memory>

#include <shellapi.h>
#include <windowsx.h>

#include "../../res/resource.h"
#include "../core/Format.h"
#include "../core/MetricCatalog.h"
#include "../core/I18n.h"
#include "../core/Text.h"
#include "../platform/Autostart.h"
#include "../sensors/GpuThermalProvider.h"
#include "../sensors/PdhProvider.h"
#include "../sensors/SystemProviders.h"

namespace wram {
namespace {

constexpr UINT kMsgTray = WM_APP + 1;
constexpr UINT_PTR kTimerSample = 1;
constexpr UINT_PTR kTimerTrim = 2;
constexpr UINT kCmdSettings = 100;
constexpr UINT kCmdExit = 101;
constexpr char kVersion[] = "1.0.0";
constexpr char kRepoUrl[] = "https://github.com/Ju4nMarin/WRAM";
constexpr wchar_t kRepoUrlW[] = L"https://github.com/Ju4nMarin/WRAM";
constexpr wchar_t kLicenseUrlW[] = L"https://github.com/Ju4nMarin/WRAM/blob/main/LICENSE";

const char* UnitKey(Unit unit) {
    switch (unit) {
        case Unit::Percent: return "percent";
        case Unit::Celsius: return "celsius";
        case Unit::MHz: return "mhz";
        case Unit::GigaBytes: return "gb";
        case Unit::BytesPerSec: return "rate";
        case Unit::Duration: return "duration";
        case Unit::Count: return "count";
    }
    return "count";
}

void AppendNumber(std::string& out, double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4g", std::isfinite(v) ? v : 0.0);
    out += buf;
}

void AppendColor(std::string& out, uint32_t c) {
    if (c == Config::kAutoColor) {
        out += "\"auto\"";
        return;
    }
    char buf[16];
    std::snprintf(buf, sizeof buf, "\"#%06x\"", c & 0xFFFFFFu);
    out += buf;
}

bool ReadLightThemeFlag(const wchar_t* name) {
    DWORD value = 0;
    DWORD size = sizeof value;
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", name,
                 RRF_RT_REG_DWORD, nullptr, &value, &size);
    return value != 0;
}

wchar_t ApplyUserDecimalSeparator() {
    // El mismo separador que usa el reloj y el resto de Windows para este usuario.
    wchar_t sep[4] = L".";
    GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, sep, 4);
    SetDecimalSeparator(sep[0]);
    return sep[0] ? sep[0] : L'.';
}

void TrimWorkingSet() {
    // Devuelve al sistema las páginas que el proceso ya no usa (por ejemplo, tras cerrar la configuración).
    SetProcessWorkingSetSizeEx(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1), 0);
}

}  // namespace

App::App(HINSTANCE instance) : instance_(instance), band_(*this), settings_(*this) {
    sampler_.Add(std::make_unique<PdhProvider>());
    sampler_.Add(std::make_unique<MemoryProvider>());
    sampler_.Add(std::make_unique<GpuThermalProvider>());
    sampler_.Add(std::make_unique<PowerProvider>());
    sampler_.Add(std::make_unique<UptimeProvider>());
}

App::~App() {
    tray_.Remove();
    if (host_) DestroyWindow(host_);
}

int App::Run() {
    WNDCLASSEXW wc{sizeof wc};
    wc.lpfnWndProc = &App::HostProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kHostClassName;
    if (!RegisterClassExW(&wc)) return 1;
    // Ventana oculta de nivel superior (no message-only) para recibir avisos del sistema.
    host_ = CreateWindowExW(WS_EX_TOOLWINDOW, kHostClassName, L"WRAM", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                            instance_, this);
    if (!host_) return 1;

    taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");
    ChangeWindowMessageFilterEx(host_, taskbarCreatedMsg_, MSGFLT_ALLOW, nullptr);

    ApplyUserDecimalSeparator();
    const bool firstRun = GetFileAttributesW(store_.Path().c_str()) == INVALID_FILE_ATTRIBUTES;
    Config config = store_.Load();
    config.autostart = autostart::IsEnabled();

    if (!band_.Create(instance_)) return 1;
    icon_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                          GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    tray_.Add(host_, kMsgTray, icon_, L"WRAM");

    ApplyConfig(std::move(config), firstRun);
    RestartTimer();
    Tick();
    if (firstRun) settings_.Show(instance_);
    SetTimer(host_, kTimerTrim, 5000, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void App::Tick() {
    const MetricFrame& frame = sampler_.Sample();
    band_.Update(frame);
    band_.Reposition();
    if (settings_.IsOpen()) settings_.Post(ValuesMessage(frame));
}

void App::ApplyConfig(Config config, bool persist) {
    if (config.autostart != autostart::IsEnabled()) {
        if (!autostart::SetEnabled(config.autostart)) config.autostart = autostart::IsEnabled();
    }
    const bool intervalChanged = config.intervalMs != config_.intervalMs;
    const Lang lang = ResolveLang(config.language);
    const bool langChanged = lang != CurrentLang();
    SetLang(lang);
    config_ = std::move(config);
    if (persist) store_.Save(config_);
    RefreshSamplerMask();
    band_.ApplyConfig(config_);
    band_.Update(sampler_.Last());
    settings_.SetDarkTheme(SettingsUseDarkTheme());
    // Con otro idioma cambian los nombres del catálogo: la ventana los recibe de nuevo.
    if (langChanged && settings_.IsOpen()) settings_.Post(BuildInitMessage());
    if (intervalChanged || persist) RestartTimer();
}

bool App::SettingsUseDarkTheme() const {
    if (config_.theme == 1) return false;
    if (config_.theme == 2) return true;
    return !ReadLightThemeFlag(L"AppsUseLightTheme");
}

void App::OnSettingsOpen(std::string_view target) {
    // Solo destinos fijos: la interfaz no puede pedir abrir rutas o URLs arbitrarias.
    if (target == "config") {
        const std::wstring& path = store_.Path();
        const std::wstring folder = path.substr(0, path.find_last_of(L'\\'));
        CreateDirectoryW(folder.c_str(), nullptr);
        ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (target == "repo") {
        ShellExecuteW(nullptr, L"open", kRepoUrlW, nullptr, nullptr, SW_SHOWNORMAL);
    } else if (target == "license") {
        ShellExecuteW(nullptr, L"open", kLicenseUrlW, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void App::RefreshSamplerMask() {
    // Con la configuración abierta se leen todas las métricas para mostrar el catálogo en vivo.
    sampler_.SetActive(settings_.IsOpen() ? kAllMetrics : config_.ActiveMask());
}

void App::RestartTimer() {
    // Temporizador coalescente: Windows puede agrupar despertares y ahorrar energía.
    SetCoalescableTimer(host_, kTimerSample, config_.intervalMs, nullptr, config_.intervalMs / 10);
}

std::string App::ValuesMessage(const MetricFrame& frame) const {
    std::string out = "{\"t\":\"v\",\"v\":{";
    for (size_t i = 0; i < kMetricCount; ++i) {
        const auto id = static_cast<MetricId>(i);
        const MetricInfo& info = Info(id);
        const Sample& s = frame.Get(id);
        if (i) out += ',';
        out += '"';
        out += info.key;
        out += "\":";
        if (!s.valid) {
            out += "null";
            continue;
        }
        out += '[';
        AppendNumber(out, s.value);
        out += ',';
        text::AppendJsonString(out, text::WideToUtf8(FormatValue(info.unit, s.value)));
        out += ']';
    }
    out += "},\"a\":[";
    bool first = true;
    for (const SlotConfig& slot : config_.slots) {
        const Sample& s = frame.Get(slot.id);
        if (s.valid && slot.alert && IsAlert(Info(slot.id), s.value, slot.threshold)) {
            if (!first) out += ',';
            text::AppendJsonString(out, Info(slot.id).key);
            first = false;
        }
    }
    out += "]}";
    return out;
}

std::string App::BuildInitMessage() {
    std::string out = "{\"t\":\"init\",\"version\":\"";
    out += kVersion;
    out += "\",\"lang\":\"";
    out += LangCode(CurrentLang());
    out += "\",\"repo\":\"";
    out += kRepoUrl;
    out += "\",\"taskbarLight\":";
    out += ReadLightThemeFlag(L"SystemUsesLightTheme") ? "true" : "false";
    out += ",\"appsLight\":";
    out += ReadLightThemeFlag(L"AppsUseLightTheme") ? "true" : "false";
    out += ",\"decimal\":";
    const wchar_t dec[2] = {ApplyUserDecimalSeparator(), L'\0'};
    text::AppendJsonString(out, text::WideToUtf8(dec));
    out += ",\"config\":{\"interval\":" + std::to_string(config_.intervalMs);
    out += ",\"rows\":" + std::to_string(config_.rows);
    out += ",\"font\":";
    text::AppendJsonString(out, text::WideToUtf8(config_.font));
    out += ",\"fontSize\":" + std::to_string(config_.fontSize);
    out += ",\"valueColor\":";
    AppendColor(out, config_.valueColor);
    out += ",\"alertColor\":";
    AppendColor(out, config_.alertColor);
    out += ",\"labels\":";
    out += config_.showLabels ? "true" : "false";
    out += ",\"autostart\":";
    out += config_.autostart ? "true" : "false";
    out += ",\"hideFullscreen\":";
    out += config_.hideOnFullscreen ? "true" : "false";
    out += ",\"offset\":" + std::to_string(config_.offset);
    out += ",\"theme\":" + std::to_string(config_.theme);
    out += ",\"language\":" + std::to_string(config_.language);
    out += ",\"slots\":[";
    for (size_t i = 0; i < config_.slots.size(); ++i) {
        const SlotConfig& s = config_.slots[i];
        if (i) out += ',';
        out += "{\"key\":";
        text::AppendJsonString(out, Info(s.id).key);
        out += ",\"label\":";
        text::AppendJsonString(out, text::WideToUtf8(s.label));
        out += ",\"thr\":";
        AppendNumber(out, s.threshold);
        out += ",\"alert\":";
        out += s.alert ? "true" : "false";
        out += '}';
    }
    out += "]},\"catalog\":[";
    for (size_t i = 0; i < kMetricCount; ++i) {
        const MetricInfo& info = Info(static_cast<MetricId>(i));
        if (i) out += ',';
        out += "{\"key\":";
        text::AppendJsonString(out, info.key);
        out += ",\"name\":";
        text::AppendJsonString(out, NameOf(info));
        out += ",\"source\":";
        text::AppendJsonString(out, SourceOf(info));
        out += ",\"label\":";
        text::AppendJsonString(out, text::WideToUtf8(LabelOf(info)));
        out += ",\"unit\":\"";
        out += UnitKey(info.unit);
        out += "\",\"thr\":";
        AppendNumber(out, info.defaultThreshold);
        out += ",\"thrMax\":";
        AppendNumber(out, info.thresholdMax);
        out += ",\"below\":";
        out += info.alertBelow ? "true" : "false";
        out += ",\"approx\":";
        out += info.approximate ? "true" : "false";
        out += '}';
    }
    out += "],\"fonts\":[";
    const auto& fonts = AllowedFonts();
    for (size_t i = 0; i < fonts.size(); ++i) {
        if (i) out += ',';
        text::AppendJsonString(out, text::WideToUtf8(fonts[i]));
    }
    out += "],\"limits\":{\"slots\":" + std::to_string(kMaxSlots);
    out += ",\"label\":" + std::to_string(Config::kMaxLabelChars);
    out += ",\"fontMin\":" + std::to_string(Config::kMinFontSize);
    out += ",\"fontMax\":" + std::to_string(Config::kMaxFontSize);
    out += ",\"offsetMax\":" + std::to_string(Config::kMaxOffset);
    out += ",\"intervalMin\":" + std::to_string(Config::kMinInterval);
    out += ",\"intervalMax\":" + std::to_string(Config::kMaxInterval);
    out += "},\"configPath\":";
    text::AppendJsonString(out, text::WideToUtf8(store_.Path()));
    out += '}';
    return out;
}

void App::OnSettingsApply(std::string_view ini) { ApplyConfig(ConfigCodec::Parse(ini), true); }

void App::OnSettingsReset() {
    Config defaults = Config::Defaults();
    defaults.autostart = config_.autostart;  // restablecer no toca el inicio con Windows
    ApplyConfig(std::move(defaults), true);
    settings_.Post(BuildInitMessage());
    settings_.Post(ValuesMessage(sampler_.Last()));
}

void App::OnSettingsClosed() {
    RefreshSamplerMask();
    SetTimer(host_, kTimerTrim, 3000, nullptr);
}

void App::OnBandActivate() {
    settings_.Show(instance_);
    RefreshSamplerMask();
}

void App::OnBandContextMenu(POINT screenPt) { ShowMenu(screenPt); }

void App::ShowMenu(POINT pt) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kCmdSettings, Text(Str::MenuSettings));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdExit, Text(Str::MenuExit));
    SetMenuDefaultItem(menu, kCmdSettings, FALSE);
    SetForegroundWindow(host_);  // requisito para que el menú se cierre al hacer clic afuera
    const UINT cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x,
                                      pt.y, host_, nullptr);
    DestroyMenu(menu);
    PostMessageW(host_, WM_NULL, 0, 0);
    if (cmd == kCmdSettings) OnBandActivate();
    else if (cmd == kCmdExit) PostQuitMessage(0);
}

LRESULT CALLBACK App::HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }
    auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->Handle(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == taskbarCreatedMsg_ && taskbarCreatedMsg_ != 0) {
        tray_.Readd();
        band_.OnSystemChange();
        return 0;
    }
    switch (msg) {
        case WM_TIMER:
            if (wp == kTimerSample) Tick();
            else if (wp == kTimerTrim) {
                KillTimer(hwnd, kTimerTrim);
                TrimWorkingSet();
            }
            return 0;
        case kMsgTray:
            switch (LOWORD(lp)) {
                case NIN_SELECT:
                case NIN_KEYSELECT: OnBandActivate(); break;
                case WM_CONTEXTMENU: ShowMenu({GET_X_LPARAM(wp), GET_Y_LPARAM(wp)}); break;
            }
            return 0;
        case kMsgShowSettings: OnBandActivate(); return 0;
        case WM_SETTINGCHANGE:
            ApplyUserDecimalSeparator();
            band_.OnSystemChange();
            settings_.SetDarkTheme(SettingsUseDarkTheme());
            if (settings_.IsOpen()) settings_.Post(BuildInitMessage());  // tema de la barra o separador decimal
            return 0;
        case WM_THEMECHANGED:
        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED: band_.OnSystemChange(); return 0;
        case WM_ENDSESSION:
            if (wp) store_.Save(config_);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace wram
