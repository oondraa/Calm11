#pragma once

#include "registry.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Backup;

enum Preset : unsigned { kNoPreset = 0, kMac = 1, kGnome = 2, kBoth = kMac | kGnome };
enum class State { Off, Partial, On };

struct Entry {
    HKEY root;
    const wchar_t* path;
    const wchar_t* name;
    RegData on;
    std::optional<RegData> off;  // Windows default when there is no backup; nullopt = value absent
};

struct Tweak {
    const wchar_t* id;
    const wchar_t* group;
    const wchar_t* title;
    const wchar_t* note;
    std::vector<Entry> entries;
    const wchar_t* ownedKey;  // HKCU key the tweak creates; removed on revert unless it existed before
    unsigned presets;
    bool needsExplorerRestart;
};

const std::vector<Tweak>& AllTweaks();
unsigned PresetFromName(std::wstring_view name);
bool NeedsAdmin(const Tweak& t);
State GetState(const Tweak& t);

struct ApplyReport {
    int changed = 0;
    std::vector<std::wstring> failed;
    bool needExplorerRestart = false;
};

// Makes the system match `want` (one flag per AllTweaks() entry) and saves the backup.
ApplyReport ApplyDesired(const std::vector<bool>& want, Backup& backup);
