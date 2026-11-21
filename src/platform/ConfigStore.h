#pragma once

#include <string>

#include "../core/Config.h"

namespace wram {

// Persistencia de la configuración en %APPDATA%\WRAM\config.ini.
class ConfigStore {
public:
    ConfigStore();
    Config Load() const;
    bool Save(const Config& config) const;
    const std::wstring& Path() const { return path_; }

private:
    std::wstring dir_;
    std::wstring path_;
};

}  // namespace wram
