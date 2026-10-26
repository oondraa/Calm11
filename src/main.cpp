#include "app.h"
#include "backup.h"
#include "gui.h"
#include "system.h"
#include "tweaks.h"

#include <objbase.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace {

// GUI-subsystem exe: write to a redirected pipe if there is one, otherwise to the parent console.
void Out(const std::wstring& text) {
    static HANDLE out = [] {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if ((!h || h == INVALID_HANDLE_VALUE) && AttachConsole(ATTACH_PARENT_PROCESS))
            h = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        return h;
    }();
    if (!out || out == INVALID_HANDLE_VALUE) return;
    DWORD mode, written;
    if (GetConsoleMode(out, &mode)) {
        WriteConsoleW(out, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
    } else {
        std::string utf8 = ToUtf8(text);
        WriteFile(out, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    }
}

std::wstring Pad(std::wstring s, size_t width) {
    if (s.size() < width) s.append(width - s.size(), L' ');
    return s;
}

int PrintStatus() {
    Out(APP_NAME L" " APP_VERSION L"   správce: " + std::wstring(IsElevated() ? L"ano" : L"ne") + L"\n\n");
    for (const Tweak& t : AllTweaks()) {
        State s = GetState(t);
        const wchar_t* mark = s == State::On ? L"[x]" : s == State::Partial ? L"[~]" : L"[ ]";
        Out(std::wstring(L"  ") + mark + L" " + Pad(t.id, 28) + t.title + L"\n");
    }
    return 0;
}

int ApplyPreset(const std::wstring& name) {
    unsigned preset = PresetFromName(name);
    if (preset == kNoPreset) {
        Out(L"Neznámý preset: " + name + L" (mac, gnome)\n");
        return 2;
    }
    std::vector<bool> want;
    for (const Tweak& t : AllTweaks()) want.push_back((t.presets & preset) || GetState(t) == State::On);

    Backup backup;
    ApplyReport r = ApplyDesired(want, backup);
    NotifySettingsChanged();
    Out(L"Změněno úprav: " + std::to_wstring(r.changed) + L"\n");
    for (const auto& f : r.failed) Out(L"  selhalo: " + f + L"\n");
    if (r.needExplorerRestart) Out(L"Pro dokončení restartuj Průzkumník (nebo se odhlas).\n");
    return r.failed.empty() ? 0 : 1;
}

int RevertAll() {
    Backup backup;
    Backup::Result r = backup.RestoreAll();
    backup.Save();
    NotifySettingsChanged();
    Out(L"Vráceno hodnot: " + std::to_wstring(r.restored) + L"\n");
    for (const auto& f : r.failed) Out(L"  selhalo: " + f + L"\n");
    return r.failed.empty() ? 0 : 1;
}

int RunCli(const std::vector<std::wstring>& args) {
    const std::wstring& cmd = args[0];
    if (cmd == L"--status") return PrintStatus();
    if (cmd == L"--preset" && args.size() >= 2) return ApplyPreset(args[1]);
    if (cmd == L"--revert") return RevertAll();

    Out(APP_NAME L" " APP_VERSION L"\n\n"
        L"  (bez parametrů)      otevře okno\n"
        L"  --status             vypíše stav všech úprav\n"
        L"  --preset mac|gnome   zapne úpravy z presetu\n"
        L"  --revert             vrátí vše, co " APP_NAME L" změnil\n");
    return cmd == L"--help" || cmd == L"-h" || cmd == L"/?" ? 0 : 2;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args(argv + 1, argv + argc);
    LocalFree(argv);

    if (!args.empty() && args[0] != L"--gui") return RunCli(args);

    // HKLM tweaks need admin. Ask once at start; if UAC is declined, run anyway with HKCU tweaks only.
    if (args.empty() && !IsElevated() && RelaunchElevated(L"--gui")) return 0;
    return RunGui(instance, show);
}
