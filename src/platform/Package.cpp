#include "Package.h"

#include "Win.h"

#include <appmodel.h>
#include <shlobj.h>

namespace wram::package {

bool IsPackaged() {
    static const bool packaged = [] {
        UINT32 length = 0;
        return GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
    }();
    return packaged;
}

std::wstring LocalStateDir() {
    if (!IsPackaged()) return {};
    UINT32 length = 0;
    if (GetCurrentPackageFamilyName(&length, nullptr) != ERROR_INSUFFICIENT_BUFFER || length == 0) return {};
    std::wstring family(length, L'\0');
    if (GetCurrentPackageFamilyName(&length, family.data()) != ERROR_SUCCESS) return {};
    family.resize(length - 1);
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return {};
    std::wstring dir = local;
    CoTaskMemFree(local);
    return dir + L"\\Packages\\" + family + L"\\LocalState";
}

}  // namespace wram::package
