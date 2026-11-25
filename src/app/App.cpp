#include "App.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <thread>
#include <utility>

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
constexpr UINT_PTR kTimerSettle = 3;
constexpr UINT_PTR kTimerUpdate = 4;
constexpr UINT kFirstUpdateCheckMs = 3 * 60 * 1000;  // a los 3 minutos de arrancar
constexpr UINT kUpdateCheckEveryMs = 60 * 60 * 1000;  // luego cada hora se mira si ya pasó un día
constexpr long long kUpdateCheckSpacing = 24 * 60 * 60;
constexpr UINT kCmdSettings = 100;
constexpr UINT kCmdExit = 101;
constexpr char kVersion[] = "1.7.4";
constexpr char kRepoUrl[] = "https://github.com/Ju4nMarin/WRAM";
constexpr wchar_t kRepoUrlW[] = L"https://github.com/Ju4nMarin/WRAM";
constexpr wchar_t kLicenseUrlW[] = L"https://github.com/Ju4nMarin/WRAM/blob/main/LICENSE";
constexpr wchar_t kDonateUrlW[] = L"https://www.paypal.com/paypalme/evopl";

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

// Consulta de la última versión en un hilo aparte. Ninguna excepción sale del hilo: cualquier fallo llega a la
// ventana como resultado vacío (lParam 0), y el resultado se libera aunque el mensaje no se pueda enviar.
bool CheckInBackground(HWND host) {
    try {
        std::thread([host]() noexcept {
            std::unique_ptr<updater::CheckResult> result;
            try {
                result = std::make_unique<updater::CheckResult>(updater::CheckLatest());
            } catch (...) {
                result.reset();
            }
            if (PostMessageW(host, kMsgUpdateChecked, 0, reinterpret_cast<LPARAM>(result.get()))) {
                (void)result.release();  // ahora es de la ventana, que lo libera al recibirlo
            }
        }).detach();
        return true;
    } catch (...) {
        return false;  // no se pudo crear el hilo
    }
}

bool InstallInBackground(HWND host, const std::string& tag, const Version& version) {
    try {
        std::thread([host, tag, version]() noexcept {
            updater::Error error = updater::Error::Network;
            try {
                error = updater::Install(tag, version);
            } catch (...) {
                error = updater::Error::Write;
            }
            PostMessageW(host, kMsgUpdateInstalled, static_cast<WPARAM>(error), 0);
        }).detach();
        return true;
    } catch (...) {
        return false;
    }
}

// Ícono de la bandeja con un punto azul abajo a la derecha: hay una versión nueva.
HICON MakeBadgeIcon(HICON base) {
    ICONINFO info{};
    if (!base || !GetIconInfo(base, &info)) return nullptr;
    BITMAP bm{};
    GetObjectW(info.hbmColor ? info.hbmColor : info.hbmMask, sizeof bm, &bm);
    const int w = bm.bmWidth;
    const int h = info.hbmColor ? bm.bmHeight : bm.bmHeight / 2;
    HICON result = nullptr;
    BITMAPINFO bi{};
    bi.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB, static_cast<DWORD>(w) * static_cast<DWORD>(h) * 4};
    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (color && bits && info.hbmColor && w > 0 && h > 0 &&
        GetDIBits(screen, info.hbmColor, 0, h, bits, &bi, DIB_RGB_COLORS) == h) {
        auto* px = static_cast<uint32_t*>(bits);
        const float r = w * 0.22f;
        const float cx = w - r - 0.5f;
        const float cy = h - r - 0.5f;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
                const float ring = std::clamp(r + 1.0f - d, 0.0f, 1.0f);  // borde blanco de 1 px
                const float dot = std::clamp(r - d, 0.0f, 1.0f);
                if (ring <= 0) continue;
                const uint32_t old = px[y * w + x];
                const float oa = ((old >> 24) & 0xFF) / 255.0f;
                // Mezcla sin premultiplicar: primero el borde blanco, encima el punto azul (#0078D4).
                auto mix = [](float under, float over, float a) { return over * a + under * (1 - a); };
                float rr = mix(((old >> 16) & 0xFF) / 255.0f, 1.0f, ring);
                float gg = mix(((old >> 8) & 0xFF) / 255.0f, 1.0f, ring);
                float bb = mix((old & 0xFF) / 255.0f, 1.0f, ring);
                rr = mix(rr, 0x00 / 255.0f, dot);
                gg = mix(gg, 0x78 / 255.0f, dot);
                bb = mix(bb, 0xD4 / 255.0f, dot);
                const float aa = std::max(oa, ring);
                auto to8 = [](float v) { return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
                px[y * w + x] = (to8(aa) << 24) | (to8(rr) << 16) | (to8(gg) << 8) | to8(bb);
            }
        }
        const std::vector<BYTE> zeros(static_cast<size_t>((w + 15) / 16) * 2 * h, 0);  // máscara vacía: manda el alfa
        HBITMAP mask = CreateBitmap(w, h, 1, 1, zeros.data());
        ICONINFO out{TRUE, 0, 0, mask, color};
        result = CreateIconIndirect(&out);
        if (mask) DeleteObject(mask);
    }
    ReleaseDC(nullptr, screen);
    if (color) DeleteObject(color);
    if (info.hbmColor) DeleteObject(info.hbmColor);
    if (info.hbmMask) DeleteObject(info.hbmMask);
    return result;
}

}  // namespace

App::App(HINSTANCE instance, bool justUpdated) : instance_(instance), justUpdated_(justUpdated), settings_(*this) {
    sampler_.Add(std::make_unique<PdhProvider>());
    sampler_.Add(std::make_unique<MemoryProvider>());
    sampler_.Add(std::make_unique<GpuThermalProvider>());
    sampler_.Add(std::make_unique<PowerProvider>());
    sampler_.Add(std::make_unique<UptimeProvider>());
}

App::~App() {
    tray_.Remove();
    if (badgeIcon_) DestroyIcon(badgeIcon_);
    if (largeIcon_) DestroyIcon(largeIcon_);
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

    auto primary = std::make_unique<TaskbarBand>(*this);
    if (!primary->Create(instance_, FindWindowW(L"Shell_TrayWnd", nullptr), true)) return 1;
    bands_.push_back(std::move(primary));
    icon_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                          GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    tray_.Add(host_, kMsgTray, icon_, L"WRAM");
    badgeIcon_ = MakeBadgeIcon(icon_);
    largeIcon_ = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON) * 2,
                                               GetSystemMetrics(SM_CYICON) * 2, 0));
    SetTimer(host_, kTimerUpdate, kFirstUpdateCheckMs, nullptr);

    ApplyConfig(std::move(config), firstRun);
    RestartTimer();
    Tick();
    updater::RemoveLeftovers();  // ejecutable anterior de una actualización
    if (firstRun || justUpdated_) settings_.Show(instance_);
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
    for (auto& band : bands_) {
        band->Update(frame);
        band->Reposition();
    }
    // Si la barra principal cambió de lugar o de tamaño (posición, tamaño o pantallas), se reajusta enseguida.
    RECT bar{};
    if (HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr); tray && GetWindowRect(tray, &bar) &&
                                                              !EqualRect(&bar, &lastBarRect_)) {
        const bool first = IsRectEmpty(&lastBarRect_);
        lastBarRect_ = bar;
        if (!first) StartSettle();
    }
    // Alineación de los íconos y tamaño de la barra: no siempre llega un aviso del sistema al cambiarlos.
    DWORD align = 1, size = 1, bytes = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"TaskbarAl",
                 RRF_RT_REG_DWORD, nullptr, &align, &bytes);
    bytes = sizeof(DWORD);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"TaskbarSi",
                 RRF_RT_REG_DWORD, nullptr, &size, &bytes);
    const DWORD layout = (align << 8) | size;
    if (layout != lastTaskbarLayout_) {
        const bool first = lastTaskbarLayout_ == kUnknownLayout;
        lastTaskbarLayout_ = layout;
        if (!first) StartSettle();
    }
    // Los íconos de la barra cambian al abrir o cerrar programas. Con la configuración abierta se mide más
    // seguido para que el límite de "Distancia a la bandeja" esté al día.
    const ULONGLONG now = GetTickCount64();
    if (now - lastAppsScan_ >= (settings_.IsOpen() ? 3000u : 20000u)) ScanTaskbars();
    if (settings_.IsOpen()) settings_.Post(ValuesMessage(frame));
}

void App::ScanTaskbars() {
    lastAppsScan_ = GetTickCount64();
    for (auto& band : bands_) band->RefreshTaskbarApps();
}

namespace {
BOOL CALLBACK CollectSecondaryTaskbars(HWND hwnd, LPARAM lp) {
    wchar_t cls[64]{};
    GetClassNameW(hwnd, cls, 64);
    if (wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0 && IsWindowVisible(hwnd)) {
        reinterpret_cast<std::vector<HWND>*>(lp)->push_back(hwnd);
    }
    return TRUE;
}
}  // namespace

void App::SyncBands() {
    // Una franja por cada barra de las demás pantallas cuando la opción está activa.
    std::vector<HWND> wanted;
    if (config_.allDisplays) EnumWindows(&CollectSecondaryTaskbars, reinterpret_cast<LPARAM>(&wanted));
    bands_.erase(std::remove_if(bands_.begin() + 1, bands_.end(),
                                [&](const std::unique_ptr<TaskbarBand>& b) {
                                    return std::find(wanted.begin(), wanted.end(), b->Taskbar()) == wanted.end();
                                }),
                 bands_.end());
    for (HWND bar : wanted) {
        const bool exists = std::any_of(bands_.begin(), bands_.end(), [&](const auto& b) { return b->Taskbar() == bar; });
        if (exists) continue;
        auto band = std::make_unique<TaskbarBand>(*this);
        if (!band->Create(instance_, bar, false)) continue;
        band->ApplyConfig(config_);
        band->RefreshTaskbarApps();
        band->Update(sampler_.Last());
        bands_.push_back(std::move(band));
    }
}

void App::StartSettle() {
    // Al cambiar alineación, posición o tamaño de la barra, Windows anima los íconos durante un momento:
    // se vuelve a medir cada 150 ms por 3 s para que la franja quede en su lugar sin esperar al escaneo normal.
    settleUntil_ = GetTickCount64() + 3000;
    SetTimer(host_, kTimerSettle, 150, nullptr);
}

void App::OnSystemChange() {
    SyncBands();
    for (auto& band : bands_) band->OnSystemChange();
    StartSettle();
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
    SyncBands();
    for (auto& band : bands_) band->ApplyConfig(config_);
    // Medir los íconos de la barra es una consulta entre procesos: al arrastrar un control llegan muchos
    // cambios seguidos y se reutiliza la última medición para que la franja responda al instante.
    if (GetTickCount64() - lastAppsScan_ >= 3000) ScanTaskbars();
    for (auto& band : bands_) band->Update(sampler_.Last());
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
    } else if (target == "donate") {
        ShellExecuteW(nullptr, L"open", kDonateUrlW, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void App::OnSettingsUpdate(std::string_view action) {
    // La red se usa solo aquí, cuando la persona lo pide, y siempre fuera del hilo de la interfaz.
    if (updateBusy_.exchange(true)) return;
    if (action == "check") {
        PostUpdateState("checking", {});
        if (!CheckInBackground(host_)) {
            updateBusy_ = false;
            PostUpdateState("error", {}, "network");
        }
    } else if (action == "install" && !updateTag_.empty()) {
        PostUpdateState("installing", VersionToString(updateVersion_));
        if (!InstallInBackground(host_, updateTag_, updateVersion_)) {
            updateBusy_ = false;
            PostUpdateState("error", VersionToString(updateVersion_), "write");
        }
    } else {
        updateBusy_ = false;
    }
}

void App::MaybeCheckForUpdate() {
    // Aviso automático: como mucho una consulta al día, nunca con pantalla completa, juegos,
    // presentaciones ni "No molestar" activos (se reintenta en la próxima hora).
    if (!config_.updateNotify || updateBusy_) return;
    QUERY_USER_NOTIFICATION_STATE quns{};
    if (FAILED(SHQueryUserNotificationState(&quns)) || quns != QUNS_ACCEPTS_NOTIFICATIONS) return;
    if (updater::UnixNow() - updater::LoadNotifyState().lastCheck < kUpdateCheckSpacing) return;
    if (updateBusy_.exchange(true)) return;
    backgroundCheck_ = true;
    if (!CheckInBackground(host_)) {
        backgroundCheck_ = false;
        updateBusy_ = false;
    }
}

void App::OnUpdateChecked(const updater::CheckResult& result) {
    updateBusy_ = false;
    const bool background = std::exchange(backgroundCheck_, false);
    if (!result.ok) {
        if (!background) PostUpdateState("error", {}, "network");  // en segundo plano, se reintenta otro día
        return;
    }
    updater::NotifyState state = updater::LoadNotifyState();
    state.lastCheck = updater::UnixNow();
    Version current;
    ParseVersion(kVersion, current);
    if (CompareVersion(result.version, current) > 0) {
        updateTag_ = result.tag;
        updateVersion_ = result.version;
        if (badgeIcon_) tray_.SetIcon(badgeIcon_);
        PostUpdateState("available", VersionToString(result.version));
        // Cada versión se notifica una sola vez; el punto en la bandeja y en la ventana queda hasta actualizar.
        if (background && state.notifiedTag != result.tag) {
            wchar_t title[64]{};
            const std::string v = VersionToString(result.version);
            swprintf_s(title, Text(Str::UpdateTitle), std::wstring(v.begin(), v.end()).c_str());
            tray_.Notify(title, Text(Str::UpdateText), largeIcon_);
            state.notifiedTag = result.tag;
        }
    } else {
        updateTag_.clear();
        tray_.SetIcon(icon_);
        PostUpdateState("latest", kVersion);
    }
    updater::SaveNotifyState(state);
}

void App::OpenSettingsAt(const char* section) {
    openSection_ = section;
    if (settings_.IsOpen()) {
        settings_.Post(std::string("{\"t\":\"go\",\"section\":\"") + section + "\"}");
        openSection_.clear();
    }
    OnBandActivate();
}

void App::OnUpdateInstalled(updater::Error error) {
    if (error == updater::Error::None) {
        // El ejecutable nuevo se lanza al salir, cuando esta instancia ya soltó el bloqueo de instancia única.
        restartAfterExit_ = true;
        store_.Save(config_);
        settings_.Close();
        PostQuitMessage(0);
        return;
    }
    updateBusy_ = false;
    const char* code = error == updater::Error::Network ? "network" : error == updater::Error::Verify ? "verify" : "write";
    PostUpdateState("error", VersionToString(updateVersion_), code);
}

void App::PostUpdateState(const char* state, const std::string& version, const char* code) {
    if (!settings_.IsOpen()) return;
    std::string json = "{\"t\":\"update\",\"state\":\"";
    json += state;
    json += "\",\"version\":\"" + version + "\"";
    if (code) json += std::string(",\"code\":\"") + code + "\"";
    json += "}";
    settings_.Post(json);
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
    std::string out = "{\"t\":\"v\",\"omax\":" + std::to_string(Primary().MaxOffsetDip()) +
                      ",\"agap\":" + std::to_string(Primary().AppsGapDip()) + ",\"v\":{";
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
    out += ",\"fullscreen\":" + std::to_string(config_.fullscreenMode);
    out += ",\"overlayPos\":" + std::to_string(config_.overlayPosition);
    out += ",\"overlayOpacity\":" + std::to_string(config_.overlayOpacity);
    out += ",\"overlayAlpha\":" + std::to_string(config_.overlayAlpha);
    out += ",\"allDisplays\":";
    out += config_.allDisplays ? "true" : "false";
    out += ",\"updateNotify\":";
    out += config_.updateNotify ? "true" : "false";
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
    out += "]}";
    // Versión nueva ya encontrada y sección a abrir (por ejemplo, desde la notificación).
    if (!updateTag_.empty()) out += ",\"update\":{\"state\":\"available\",\"version\":\"" + VersionToString(updateVersion_) + "\"}";
    if (!openSection_.empty()) out += ",\"section\":\"" + std::exchange(openSection_, std::string()) + "\"";
    out += ",\"catalog\":[";
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
    SetTimer(host_, kTimerTrim, 5000, nullptr);  // tiempo para que terminen los procesos de WebView2
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
        OnSystemChange();
        return 0;
    }
    switch (msg) {
        case WM_TIMER:
            if (wp == kTimerSample) Tick();
            else if (wp == kTimerSettle) {
                ScanTaskbars();
                for (auto& band : bands_) band->Reposition();
                if (GetTickCount64() >= settleUntil_) KillTimer(hwnd, kTimerSettle);
            } else if (wp == kTimerUpdate) {
                SetTimer(hwnd, kTimerUpdate, kUpdateCheckEveryMs, nullptr);
                MaybeCheckForUpdate();
            } else if (wp == kTimerTrim) {
                KillTimer(hwnd, kTimerTrim);
                if (!settings_.IsOpen()) SettingsWindow::PurgeUserData();
                TrimWorkingSet();
            }
            return 0;
        case kMsgTray:
            switch (LOWORD(lp)) {
                case NIN_SELECT:
                case NIN_KEYSELECT: OnBandActivate(); break;
                case NIN_BALLOONUSERCLICK: OpenSettingsAt("settings"); break;
                case WM_CONTEXTMENU: ShowMenu({GET_X_LPARAM(wp), GET_Y_LPARAM(wp)}); break;
            }
            return 0;
        case kMsgShowSettings: OnBandActivate(); return 0;
        case kMsgUpdateChecked: {
            std::unique_ptr<updater::CheckResult> result(reinterpret_cast<updater::CheckResult*>(lp));
            OnUpdateChecked(result ? *result : updater::CheckResult{});  // vacío: la consulta falló
            return 0;
        }
        case kMsgUpdateInstalled: OnUpdateInstalled(static_cast<updater::Error>(wp)); return 0;
        case WM_SETTINGCHANGE:
            ApplyUserDecimalSeparator();
            OnSystemChange();
            settings_.SetDarkTheme(SettingsUseDarkTheme());
            if (settings_.IsOpen()) settings_.Post(BuildInitMessage());  // tema de la barra o separador decimal
            return 0;
        case WM_THEMECHANGED:
        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED: OnSystemChange(); return 0;
        case WM_ENDSESSION:
            if (wp) store_.Save(config_);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace wram
