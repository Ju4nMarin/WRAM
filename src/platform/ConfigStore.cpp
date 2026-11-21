#include "ConfigStore.h"

#include "Win.h"

#include <shlobj.h>

namespace wram {
namespace {
constexpr DWORD kMaxFileBytes = 64 * 1024;
}

ConfigStore::ConfigStore() {
    PWSTR appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &appData))) {
        dir_ = appData;
        CoTaskMemFree(appData);
    }
    dir_ += L"\\WRAM";
    path_ = dir_ + L"\\config.ini";
}

Config ConfigStore::Load() const {
    UniqueHandle file(CreateFileW(path_.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) return Config::Defaults();
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart > kMaxFileBytes) return Config::Defaults();
    std::string data(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!data.empty() && !ReadFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &read, nullptr)) {
        return Config::Defaults();
    }
    data.resize(read);
    return ConfigCodec::Parse(data);
}

bool ConfigStore::Save(const Config& config) const {
    CreateDirectoryW(dir_.c_str(), nullptr);
    const std::string data = ConfigCodec::Serialize(config);
    // Escritura atómica: archivo temporal y reemplazo, para no dejar un INI a medias.
    const std::wstring tmp = path_ + L".tmp";
    {
        UniqueHandle file(CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                      nullptr));
        if (!file) return false;
        DWORD written = 0;
        if (!WriteFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &written, nullptr) ||
            written != data.size()) {
            return false;
        }
        FlushFileBuffers(file.get());
    }
    return MoveFileExW(tmp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

}  // namespace wram
