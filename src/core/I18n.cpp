#include "I18n.h"

#include <array>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace wram {
namespace {

Lang g_lang = Lang::Es;

constexpr size_t kStrCount = static_cast<size_t>(Str::Count);

// Filas: idioma (orden de Lang). Columnas: texto (orden de Str).
constexpr std::array<std::array<const wchar_t*, kStrCount>, kLangCount> kText{{
    {{L"Configuración", L"Salir de WRAM",
      L"Falta \"Microsoft Edge WebView2 Runtime\", que la configuración necesita para mostrarse.",
      L"No se pudo iniciar WebView2. Instala o repara \"Microsoft Edge WebView2 Runtime\".",
      L"No se pudo crear la vista de configuración.",
      L"El ejecutable está dañado: falta la interfaz de configuración."}},
    {{L"Settings", L"Exit WRAM",
      L"\"Microsoft Edge WebView2 Runtime\" is missing. The settings window needs it.",
      L"WebView2 could not start. Install or repair \"Microsoft Edge WebView2 Runtime\".",
      L"The settings view could not be created.",
      L"The executable is damaged: the settings interface is missing."}},
    {{L"Configurações", L"Sair do WRAM",
      L"Falta o \"Microsoft Edge WebView2 Runtime\", necessário para exibir as configurações.",
      L"Não foi possível iniciar o WebView2. Instale ou repare o \"Microsoft Edge WebView2 Runtime\".",
      L"Não foi possível criar a janela de configurações.",
      L"O executável está danificado: falta a interface de configurações."}},
    {{L"Paramètres", L"Quitter WRAM",
      L"« Microsoft Edge WebView2 Runtime » est manquant. La fenêtre des paramètres en a besoin.",
      L"Impossible de démarrer WebView2. Installez ou réparez « Microsoft Edge WebView2 Runtime ».",
      L"Impossible de créer la fenêtre des paramètres.",
      L"L'exécutable est endommagé : l'interface des paramètres est absente."}},
    {{L"设置", L"退出 WRAM",
      L"缺少 \"Microsoft Edge WebView2 Runtime\"，设置窗口需要它才能显示。",
      L"无法启动 WebView2。请安装或修复 \"Microsoft Edge WebView2 Runtime\"。",
      L"无法创建设置窗口。",
      L"可执行文件已损坏：缺少设置界面。"}},
    {{L"設定", L"WRAM を終了",
      L"\"Microsoft Edge WebView2 Runtime\" がありません。設定ウィンドウの表示に必要です。",
      L"WebView2 を起動できませんでした。\"Microsoft Edge WebView2 Runtime\" をインストールまたは修復してください。",
      L"設定ウィンドウを作成できませんでした。",
      L"実行ファイルが破損しています。設定画面がありません。"}},
}};

}  // namespace

Lang ResolveLang(int setting) {
    switch (setting) {
        case 1: return Lang::Es;
        case 2: return Lang::En;
        case 3: return Lang::Pt;
        case 4: return Lang::Fr;
        case 5: return Lang::Zh;
        case 6: return Lang::Ja;
        default: break;
    }
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
        case LANG_SPANISH: return Lang::Es;
        case LANG_PORTUGUESE: return Lang::Pt;
        case LANG_FRENCH: return Lang::Fr;
        case LANG_CHINESE: return Lang::Zh;
        case LANG_JAPANESE: return Lang::Ja;
        default: return Lang::En;
    }
}

void SetLang(Lang lang) { g_lang = lang; }
Lang CurrentLang() { return g_lang; }

const char* LangCode(Lang lang) {
    static constexpr const char* kCodes[] = {"es", "en", "pt", "fr", "zh", "ja"};
    return kCodes[static_cast<size_t>(lang)];
}

const wchar_t* Text(Str id) { return kText[static_cast<size_t>(g_lang)][static_cast<size_t>(id)]; }

}  // namespace wram
