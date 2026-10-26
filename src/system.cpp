#include "system.h"

#include "registry.h"

#include <shellapi.h>

std::wstring ErrorText(DWORD code) {
    wchar_t* buf = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, code, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::wstring s = n ? std::wstring(buf, n) : L"chyba " + std::to_wstring(code);
    LocalFree(buf);
    while (!s.empty() && (s.back() == L'\n' || s.back() == L'\r' || s.back() == L' ' || s.back() == L'.')) s.pop_back();
    return s;
}

std::string ToUtf8(std::wstring_view s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring FromUtf8(std::string_view s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

bool IsElevated() {
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
    CloseHandle(token);
    return elevated;
}

bool RelaunchElevated(const wchar_t* params) {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return false;
    SHELLEXECUTEINFOW sei{sizeof(sei)};
    sei.lpVerb = L"runas";
    sei.lpFile = path;
    sei.lpParameters = params;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

void NotifySettingsChanged() {
    // The menu delay only applies from the registry at logon, so push it live as well.
    if (auto d = reg::Read(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"MenuShowDelay"); d && d->type == REG_SZ) {
        UINT ms = static_cast<UINT>(_wtoi(reinterpret_cast<const wchar_t*>(d->bytes.data())));
        SystemParametersInfoW(SPI_SETMENUSHOWDELAY, ms, nullptr, SPIF_SENDCHANGE);
    }
    for (const wchar_t* area : {L"ImmersiveColorSet", L"TraySettings", L"Policy", static_cast<const wchar_t*>(nullptr)}) {
        SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(area), SMTO_ABORTIFHUNG,
                            1000, nullptr);
    }
}

void RestartExplorer() {
    DWORD pid = 0;
    if (HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr)) GetWindowThreadProcessId(tray, &pid);
    if (pid) {
        if (HANDLE p = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid)) {
            // A non-zero exit code makes Winlogon restart the shell on its own.
            TerminateProcess(p, 1);
            WaitForSingleObject(p, 5000);
            CloseHandle(p);
        }
    }
    for (int i = 0; i < 40; ++i) {
        if (FindWindowW(L"Shell_TrayWnd", nullptr)) return;
        Sleep(150);
    }
    ShellExecuteW(nullptr, nullptr, L"explorer.exe", nullptr, nullptr, SW_SHOWNORMAL);
}
