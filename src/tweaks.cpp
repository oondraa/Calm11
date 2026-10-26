#include "tweaks.h"

#include "backup.h"
#include "system.h"

using reg::Dword;
using reg::Sz;

namespace {

const HKEY CU = HKEY_CURRENT_USER;
const HKEY LM = HKEY_LOCAL_MACHINE;

const wchar_t* const kAdvanced = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
const wchar_t* const kContent = L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager";
const wchar_t* const kPersonalize = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
const wchar_t* const kSystemPolicy = L"SOFTWARE\\Policies\\Microsoft\\Windows\\System";
// Win11 asks this CLSID for the new context menu; an empty InprocServer32 makes it fall back to the classic one.
const wchar_t* const kNewMenuClsid = L"Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}";

std::vector<Tweak> BuildTweaks() {
    const auto none = std::nullopt;
    return {
        // Taskbar
        {L"hide_widgets", L"Taskbar", L"Skrýt widgety (počasí a zprávy)", nullptr,
         {{LM, L"SOFTWARE\\Policies\\Microsoft\\Dsh", L"AllowNewsAndInterests", Dword(0), none}},
         nullptr, kBoth, true},
        {L"hide_search", L"Taskbar", L"Skrýt vyhledávací pole", L"Hledání dál funguje: Win a piš",
         {{CU, L"Software\\Microsoft\\Windows\\CurrentVersion\\Search", L"SearchboxTaskbarMode", Dword(0), none}},
         nullptr, kBoth, false},
        {L"hide_taskview", L"Taskbar", L"Skrýt tlačítko Zobrazení úloh", L"Win+Tab funguje dál",
         {{CU, kAdvanced, L"ShowTaskViewButton", Dword(0), none}},
         nullptr, kBoth, false},
        {L"hide_copilot", L"Taskbar", L"Vypnout Copilot na taskbaru", nullptr,
         {{CU, kAdvanced, L"ShowCopilotButton", Dword(0), none},
          {CU, L"Software\\Policies\\Microsoft\\Windows\\WindowsCopilot", L"TurnOffWindowsCopilot", Dword(1), none}},
         nullptr, kBoth, true},
        {L"taskbar_left", L"Taskbar", L"Zarovnat ikony vlevo", L"Jako Win10",
         {{CU, kAdvanced, L"TaskbarAl", Dword(0), none}},
         nullptr, kNoPreset, false},

        // Start
        {L"start_no_ads", L"Start", L"Start bez reklam a doporučených appek", nullptr,
         {{CU, kAdvanced, L"Start_IrisRecommendations", Dword(0), none},
          {CU, kAdvanced, L"Start_AccountNotifications", Dword(0), none}},
         nullptr, kBoth, true},
        {L"start_no_recent", L"Start", L"Start bez nedávných souborů", L"Vypne i nedávné v jump listech",
         {{CU, kAdvanced, L"Start_TrackDocs", Dword(0), none}},
         nullptr, kNoPreset, true},
        {L"no_web_search", L"Start", L"Hledat jen v PC, bez Bingu", L"Rychlejší hledání",
         {{CU, L"Software\\Policies\\Microsoft\\Windows\\Explorer", L"DisableSearchBoxSuggestions", Dword(1), none}},
         nullptr, kBoth, true},

        // Lock screen
        {L"lock_no_tips", L"Zamykací obrazovka", L"Bez tipů, „zajímavostí“ a reklam", nullptr,
         {{CU, kContent, L"RotatingLockScreenOverlayEnabled", Dword(0), Dword(1)},
          {CU, kContent, L"SubscribedContent-338387Enabled", Dword(0), Dword(1)}},
         nullptr, kBoth, false},
        {L"lock_no_notifications", L"Zamykací obrazovka", L"Bez notifikací appek na zamykací obrazovce", nullptr,
         {{LM, kSystemPolicy, L"DisableLockScreenAppNotifications", Dword(1), none}},
         nullptr, kBoth, false},
        {L"logon_no_blur", L"Zamykací obrazovka", L"Ostré pozadí při přihlášení (bez rozmazání)", L"Mac i GNOME blur mají",
         {{LM, kSystemPolicy, L"DisableAcrylicBackgroundOnLogon", Dword(1), none}},
         nullptr, kNoPreset, false},

        // Ads & nagging
        {L"no_suggestions", L"Bez reklam", L"Vypnout tipy a „návrhy“ ve Windows a Nastavení", nullptr,
         {{CU, kContent, L"SubscribedContent-338389Enabled", Dword(0), Dword(1)},
          {CU, kContent, L"SubscribedContent-338393Enabled", Dword(0), Dword(1)},
          {CU, kContent, L"SubscribedContent-353694Enabled", Dword(0), Dword(1)},
          {CU, kContent, L"SubscribedContent-353696Enabled", Dword(0), Dword(1)},
          {CU, kContent, L"SystemPaneSuggestionsEnabled", Dword(0), Dword(1)},
          {CU, kContent, L"SoftLandingEnabled", Dword(0), Dword(1)}},
         nullptr, kBoth, false},
        {L"explorer_no_ads", L"Bez reklam", L"Průzkumník bez reklam na OneDrive/M365", nullptr,
         {{CU, kAdvanced, L"ShowSyncProviderNotifications", Dword(0), none}},
         nullptr, kBoth, true},
        {L"desktop_no_spotlight_icon", L"Bez reklam", L"Skrýt ikonu „Další informace o obrázku“", L"Při Spotlight tapetě",
         {{CU, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel",
           L"{2cc5ca98-6485-489a-920e-b3e88a6ccce3}", Dword(1), none}},
         nullptr, kBoth, true},

        // Look & feel
        {L"dark_mode", L"Vzhled a odezva", L"Tmavý režim pro systém i appky", nullptr,
         {{CU, kPersonalize, L"AppsUseLightTheme", Dword(0), Dword(1)},
          {CU, kPersonalize, L"SystemUsesLightTheme", Dword(0), Dword(1)}},
         nullptr, kGnome, false},
        {L"faster_menus", L"Vzhled a odezva", L"Rychlejší otevírání menu", L"400 → 100 ms",
         {{CU, L"Control Panel\\Desktop", L"MenuShowDelay", Sz(L"100"), Sz(L"400")}},
         nullptr, kBoth, false},
        {L"classic_context_menu", L"Vzhled a odezva", L"Klasické kontextové menu (bez „Další možnosti“)",
         L"Rychlejší, ale vzhled z Win10",
         {{CU, L"Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\\InprocServer32", L"", Sz(L""), none}},
         kNewMenuClsid, kNoPreset, true},

        // Weak PCs
        {L"no_transparency", L"Slabší PC", L"Vypnout průhlednost", L"Ušetří GPU",
         {{CU, kPersonalize, L"EnableTransparency", Dword(0), Dword(1)}},
         nullptr, kNoPreset, false},
        {L"taskbar_no_anim", L"Slabší PC", L"Vypnout animace taskbaru", nullptr,
         {{CU, kAdvanced, L"TaskbarAnimations", Dword(0), none}},
         nullptr, kNoPreset, true},
    };
}

LSTATUS Enable(const Tweak& t, Backup& backup) {
    if (t.ownedKey) backup.RememberKey(CU, t.ownedKey);
    LSTATUS result = ERROR_SUCCESS;
    for (const Entry& e : t.entries) {
        bool fresh = backup.RememberValue(e.root, e.path, e.name);
        LSTATUS st = reg::Write(e.root, e.path, e.name, e.on);
        if (st != ERROR_SUCCESS) {
            // Nothing changed, so there is nothing to restore later either.
            if (fresh) backup.ForgetValue(e.root, e.path, e.name);
            if (result == ERROR_SUCCESS) result = st;
        }
    }
    return result;
}

LSTATUS Disable(const Tweak& t, Backup& backup) {
    LSTATUS result = ERROR_SUCCESS;
    for (const Entry& e : t.entries) {
        LSTATUS st;
        if (auto restored = backup.RestoreValue(e.root, e.path, e.name))
            st = *restored;
        else
            st = e.off ? reg::Write(e.root, e.path, e.name, *e.off) : reg::Delete(e.root, e.path, e.name);
        if (st != ERROR_SUCCESS && result == ERROR_SUCCESS) result = st;
    }
    if (t.ownedKey) {
        auto restored = backup.RestoreKey(CU, t.ownedKey);
        LSTATUS st = restored ? *restored : reg::DeleteTree(CU, t.ownedKey);
        if (st != ERROR_SUCCESS && result == ERROR_SUCCESS) result = st;
    }
    return result;
}

}  // namespace

const std::vector<Tweak>& AllTweaks() {
    static const std::vector<Tweak> tweaks = BuildTweaks();
    return tweaks;
}

unsigned PresetFromName(std::wstring_view name) {
    if (name == L"mac" || name == L"macos") return kMac;
    if (name == L"gnome" || name == L"debian") return kGnome;
    return kNoPreset;
}

bool NeedsAdmin(const Tweak& t) {
    for (const Entry& e : t.entries)
        if (e.root == HKEY_LOCAL_MACHINE) return true;
    return false;
}

State GetState(const Tweak& t) {
    size_t on = 0;
    for (const Entry& e : t.entries) {
        auto current = reg::Read(e.root, e.path, e.name);
        if (current && reg::Equal(*current, e.on)) ++on;
    }
    if (on == 0) return State::Off;
    return on == t.entries.size() ? State::On : State::Partial;
}

ApplyReport ApplyDesired(const std::vector<bool>& want, Backup& backup) {
    ApplyReport report;
    const auto& tweaks = AllTweaks();
    for (size_t i = 0; i < tweaks.size() && i < want.size(); ++i) {
        const Tweak& t = tweaks[i];
        State state = GetState(t);
        LSTATUS st;
        if (want[i] && state != State::On)
            st = Enable(t, backup);
        else if (!want[i] && state != State::Off)
            st = Disable(t, backup);
        else
            continue;

        if (st == ERROR_SUCCESS) {
            ++report.changed;
            report.needExplorerRestart |= t.needsExplorerRestart;
        } else {
            report.failed.push_back(std::wstring(t.title) + L" – " + ErrorText(st));
        }
    }
    backup.Save();
    return report;
}
