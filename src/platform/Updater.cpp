#include "Updater.h"

#include <bcrypt.h>
#include <winhttp.h>

#include <cwchar>
#include <string>
#include <vector>

namespace wram::updater {
namespace {

constexpr wchar_t kApiHost[] = L"api.github.com";
constexpr wchar_t kLatestPath[] = L"/repos/Ju4nMarin/WRAM/releases/latest";
constexpr wchar_t kDownloadHost[] = L"github.com";
constexpr wchar_t kDownloadPrefix[] = L"/Ju4nMarin/WRAM/releases/download/";
constexpr size_t kMaxJson = 512 * 1024;
constexpr size_t kMaxExe = 16 * 1024 * 1024;
constexpr size_t kMaxHashFile = 1024;

class Internet {
public:
    explicit Internet(HINTERNET h) : h_(h) {}
    ~Internet() {
        if (h_) WinHttpCloseHandle(h_);
    }
    Internet(const Internet&) = delete;
    Internet& operator=(const Internet&) = delete;
    HINTERNET get() const { return h_; }
    explicit operator bool() const { return h_ != nullptr; }

private:
    HINTERNET h_;
};

// GET por HTTPS con verificación de certificado de WinHTTP. Sigue redirecciones HTTPS (las descargas de
// GitHub pasan por su CDN) y nunca baja a HTTP. Limita el tamaño de la respuesta.
bool HttpsGet(const wchar_t* host, const std::wstring& path, std::string& body, size_t maxBytes) {
    body.clear();
    Internet session(WinHttpOpen(L"WRAM-Updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) return false;
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (!WinHttpSetOption(session.get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols)) {
        protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;  // sistemas sin TLS 1.3 en WinHTTP
        WinHttpSetOption(session.get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols);
    }
    WinHttpSetTimeouts(session.get(), 10000, 10000, 15000, 30000);
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(session.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof redirect);

    Internet connection(WinHttpConnect(session.get(), host, INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) return false;
    Internet request(WinHttpOpenRequest(connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                        WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) return false;
    const wchar_t* headers = L"Accept: application/vnd.github+json, application/octet-stream\r\n";
    if (!WinHttpSendRequest(request.get(), headers, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        return false;
    }
    DWORD status = 0;
    DWORD size = sizeof status;
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) ||
        status != 200) {
        return false;
    }
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available)) return false;
        if (available == 0) break;
        if (body.size() + available > maxBytes) return false;
        const size_t offset = body.size();
        body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), body.data() + offset, available, &read)) return false;
        body.resize(offset + read);
    }
    return true;
}

std::string Sha256Hex(const std::string& data) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};
    unsigned char digest[32]{};
    const NTSTATUS st = BCryptHash(alg, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
                                   static_cast<ULONG>(data.size()), digest, sizeof digest);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (st != 0) return {};
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    for (unsigned char b : digest) {
        out += kHex[b >> 4];
        out += kHex[b & 15];
    }
    return out;
}

// El archivo .sha256 publicado empieza con los 64 caracteres hexadecimales del hash.
std::string ExpectedHash(const std::string& file) {
    std::string out;
    for (char ch : file) {
        if (out.size() == 64) break;
        if (ch >= 'A' && ch <= 'F') ch = static_cast<char>(ch - 'A' + 'a');
        if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')) out += ch;
        else break;
    }
    return out.size() == 64 ? out : std::string();
}

bool ReadFileVersion(const std::wstring& path, Version& out) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0) return false;
    std::vector<unsigned char> buffer(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, buffer.data())) return false;
    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<void**>(&info), &len) || !info) return false;
    out = {static_cast<int>(HIWORD(info->dwFileVersionMS)), static_cast<int>(LOWORD(info->dwFileVersionMS)),
           static_cast<int>(HIWORD(info->dwFileVersionLS))};
    return true;
}

std::wstring ExePath() {
    wchar_t path[MAX_PATH * 2]{};
    const DWORD n = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    return (n == 0 || n >= std::size(path)) ? std::wstring() : std::wstring(path, n);
}

bool WriteWholeFile(const std::wstring& path, const std::string& data) {
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) return false;
    DWORD written = 0;
    return WriteFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
           written == data.size() && FlushFileBuffers(file.get());
}

}  // namespace

CheckResult CheckLatest() {
    CheckResult result;
    std::string json;
    if (!HttpsGet(kApiHost, kLatestPath, json, kMaxJson)) return result;
    const std::string tag = JsonStringField(json, "tag_name");
    if (tag.empty() || tag.front() != 'v' || !ParseVersion(tag, result.version)) return result;
    result.tag = tag;
    result.ok = true;
    return result;
}

Error Install(const std::string& tag, const Version& expected) {
    Version parsed;
    if (!ParseVersion(tag, parsed) || CompareVersion(parsed, expected) != 0) return Error::Verify;
    const std::wstring wtag(tag.begin(), tag.end());  // solo "v", dígitos y puntos
    const std::wstring base = std::wstring(kDownloadPrefix) + wtag + L"/WRAM.exe";

    std::string exe;
    std::string hashFile;
    if (!HttpsGet(kDownloadHost, base, exe, kMaxExe) || !HttpsGet(kDownloadHost, base + L".sha256", hashFile, kMaxHashFile)) {
        return Error::Network;
    }
    const std::string expectedHash = ExpectedHash(hashFile);
    if (expectedHash.empty() || Sha256Hex(exe) != expectedHash) return Error::Verify;
    if (exe.size() < 1024 || exe[0] != 'M' || exe[1] != 'Z') return Error::Verify;

    const std::wstring current = ExePath();
    if (current.empty()) return Error::Write;
    const std::wstring fresh = current + L".new";
    const std::wstring old = current + L".old";
    if (!WriteWholeFile(fresh, exe)) {
        DeleteFileW(fresh.c_str());
        return Error::Write;
    }
    Version fileVersion;
    if (!ReadFileVersion(fresh, fileVersion) || CompareVersion(fileVersion, expected) != 0) {
        DeleteFileW(fresh.c_str());
        return Error::Verify;
    }
    // Windows permite renombrar el ejecutable en uso: el actual pasa a .old y el nuevo ocupa su lugar.
    DeleteFileW(old.c_str());
    if (!MoveFileExW(current.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(fresh.c_str());
        return Error::Write;
    }
    if (!MoveFileExW(fresh.c_str(), current.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        MoveFileExW(old.c_str(), current.c_str(), MOVEFILE_REPLACE_EXISTING);  // se deja todo como estaba
        DeleteFileW(fresh.c_str());
        return Error::Write;
    }
    return Error::None;
}

bool LaunchUpdated() {
    const std::wstring exe = ExePath();
    if (exe.empty()) return false;
    std::wstring cmd = L"\"" + exe + L"\" --updated " + std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void RemoveLeftovers() {
    const std::wstring exe = ExePath();
    if (exe.empty()) return;
    DeleteFileW((exe + L".old").c_str());
    DeleteFileW((exe + L".new").c_str());
}

bool WaitForPreviousInstance(const wchar_t* commandLine) {
    const wchar_t* flag = commandLine ? wcsstr(commandLine, L"--updated ") : nullptr;
    if (!flag) return false;
    const unsigned long pid = wcstoul(flag + 10, nullptr, 10);
    if (pid != 0 && pid != GetCurrentProcessId()) {
        UniqueHandle previous(OpenProcess(SYNCHRONIZE, FALSE, pid));
        if (previous) WaitForSingleObject(previous.get(), 15000);
    }
    return true;
}

}  // namespace wram::updater
