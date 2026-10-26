#pragma once

#include "registry.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

// Remembers what every touched registry value looked like before the app first changed it,
// so any tweak - or everything at once - can be put back exactly. Lives in
// %LOCALAPPDATA%\<app>\backup.txt and survives reinstalls of the app.
class Backup {
public:
    Backup();

    // Records the current state unless a record already exists. Returns true if a new record was added.
    bool RememberValue(HKEY root, const std::wstring& path, const std::wstring& name);
    void RememberKey(HKEY root, const std::wstring& path);
    void ForgetValue(HKEY root, const std::wstring& path, const std::wstring& name);

    // nullopt = nothing recorded. Otherwise the restore result; the record is dropped on success.
    std::optional<LSTATUS> RestoreValue(HKEY root, const std::wstring& path, const std::wstring& name);
    std::optional<LSTATUS> RestoreKey(HKEY root, const std::wstring& path);

    struct Result {
        int restored = 0;
        std::vector<std::wstring> failed;
    };
    Result RestoreAll();

    bool Empty() const { return values_.empty() && keys_.empty(); }
    bool Save() const;

private:
    struct ValueRec {
        HKEY root;
        std::wstring path, name;
        std::optional<RegData> data;
    };
    struct KeyRec {
        HKEY root;
        std::wstring path;
        bool existed;
    };

    void Load();

    std::wstring file_;
    std::map<std::wstring, ValueRec> values_;
    std::map<std::wstring, KeyRec> keys_;
};
