#pragma once

#include <cstddef>
#include <cstdint>

namespace wram {

enum class Lang : uint8_t { Es, En, Pt, Fr, Zh, Ja, Count };
constexpr size_t kLangCount = static_cast<size_t>(Lang::Count);

// Ajuste de idioma: 0 sistema, 1 español, 2 inglés, 3 portugués, 4 francés, 5 chino simplificado, 6 japonés.
constexpr int kMaxLanguageSetting = 6;
Lang ResolveLang(int setting);
void SetLang(Lang lang);
Lang CurrentLang();
const char* LangCode(Lang lang);  // "es", "en", "pt", "fr", "zh", "ja"

// Textos de la parte nativa (menú y avisos). La interfaz web trae los suyos.
enum class Str : uint8_t { MenuSettings, MenuExit, ErrWebViewMissing, ErrWebViewStart, ErrView, ErrCorrupt, UpdateTitle, UpdateText, Count };
const wchar_t* Text(Str id);

}  // namespace wram
