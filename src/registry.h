#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

struct RegData {
    DWORD type = REG_NONE;
    std::vector<BYTE> bytes;
};

namespace reg {

RegData Dword(DWORD value);
RegData Sz(const std::wstring& value);

// REG_SZ values compare without trailing NULs, everything else byte-for-byte.
bool Equal(const RegData& a, const RegData& b);

// nullopt = value (or its key) does not exist. An empty name is the key's default value.
std::optional<RegData> Read(HKEY root, const std::wstring& path, const std::wstring& name);
LSTATUS Write(HKEY root, const std::wstring& path, const std::wstring& name, const RegData& data);
// Deleting something that is already gone counts as success.
LSTATUS Delete(HKEY root, const std::wstring& path, const std::wstring& name);
bool KeyExists(HKEY root, const std::wstring& path);
LSTATUS DeleteTree(HKEY root, const std::wstring& path);

const wchar_t* RootName(HKEY root);
HKEY RootFromName(const std::wstring& name);

}  // namespace reg
