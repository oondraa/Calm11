#include "registry.h"

#include <algorithm>
#include <cstring>

namespace reg {

namespace {

REGSAM Sam(REGSAM access) { return access | KEY_WOW64_64KEY; }

size_t TrimmedSize(const RegData& d) {
    size_t n = d.bytes.size() & ~size_t(1);
    while (n >= 2 && d.bytes[n - 1] == 0 && d.bytes[n - 2] == 0) n -= 2;
    return n;
}

bool IsMissing(LSTATUS st) { return st == ERROR_FILE_NOT_FOUND || st == ERROR_PATH_NOT_FOUND; }

}  // namespace

RegData Dword(DWORD value) {
    RegData d;
    d.type = REG_DWORD;
    d.bytes.resize(sizeof(value));
    std::memcpy(d.bytes.data(), &value, sizeof(value));
    return d;
}

RegData Sz(const std::wstring& value) {
    RegData d;
    d.type = REG_SZ;
    auto p = reinterpret_cast<const BYTE*>(value.c_str());
    d.bytes.assign(p, p + (value.size() + 1) * sizeof(wchar_t));
    return d;
}

bool Equal(const RegData& a, const RegData& b) {
    if (a.type != b.type) return false;
    if (a.type == REG_SZ || a.type == REG_EXPAND_SZ) {
        size_t na = TrimmedSize(a), nb = TrimmedSize(b);
        return na == nb && std::equal(a.bytes.begin(), a.bytes.begin() + na, b.bytes.begin());
    }
    return a.bytes == b.bytes;
}

std::optional<RegData> Read(HKEY root, const std::wstring& path, const std::wstring& name) {
    HKEY key;
    if (RegOpenKeyExW(root, path.c_str(), 0, Sam(KEY_QUERY_VALUE), &key) != ERROR_SUCCESS) return std::nullopt;

    std::optional<RegData> out;
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &size) == ERROR_SUCCESS) {
        RegData d;
        d.type = type;
        d.bytes.resize(size);
        if (RegQueryValueExW(key, name.c_str(), nullptr, &type, size ? d.bytes.data() : nullptr, &size) ==
            ERROR_SUCCESS) {
            d.bytes.resize(size);
            out = std::move(d);
        }
    }
    RegCloseKey(key);
    return out;
}

LSTATUS Write(HKEY root, const std::wstring& path, const std::wstring& name, const RegData& data) {
    HKEY key;
    LSTATUS st = RegCreateKeyExW(root, path.c_str(), 0, nullptr, 0, Sam(KEY_SET_VALUE), nullptr, &key, nullptr);
    if (st != ERROR_SUCCESS) return st;
    st = RegSetValueExW(key, name.c_str(), 0, data.type, data.bytes.empty() ? nullptr : data.bytes.data(),
                        static_cast<DWORD>(data.bytes.size()));
    RegCloseKey(key);
    return st;
}

LSTATUS Delete(HKEY root, const std::wstring& path, const std::wstring& name) {
    HKEY key;
    LSTATUS st = RegOpenKeyExW(root, path.c_str(), 0, Sam(KEY_SET_VALUE), &key);
    if (IsMissing(st)) return ERROR_SUCCESS;
    if (st != ERROR_SUCCESS) return st;
    st = RegDeleteValueW(key, name.c_str());
    RegCloseKey(key);
    return IsMissing(st) ? ERROR_SUCCESS : st;
}

bool KeyExists(HKEY root, const std::wstring& path) {
    HKEY key;
    if (RegOpenKeyExW(root, path.c_str(), 0, Sam(KEY_READ), &key) != ERROR_SUCCESS) return false;
    RegCloseKey(key);
    return true;
}

LSTATUS DeleteTree(HKEY root, const std::wstring& path) {
    HKEY parent;
    size_t slash = path.find_last_of(L'\\');
    std::wstring parentPath = slash == std::wstring::npos ? L"" : path.substr(0, slash);
    std::wstring leaf = slash == std::wstring::npos ? path : path.substr(slash + 1);

    LSTATUS st = RegOpenKeyExW(root, parentPath.c_str(), 0, Sam(KEY_READ | KEY_WRITE | DELETE), &parent);
    if (IsMissing(st)) return ERROR_SUCCESS;
    if (st != ERROR_SUCCESS) return st;
    st = RegDeleteTreeW(parent, leaf.c_str());
    if (st == ERROR_SUCCESS) st = RegDeleteKeyExW(parent, leaf.c_str(), KEY_WOW64_64KEY, 0);
    RegCloseKey(parent);
    return IsMissing(st) ? ERROR_SUCCESS : st;
}

const wchar_t* RootName(HKEY root) {
    if (root == HKEY_LOCAL_MACHINE) return L"HKLM";
    return L"HKCU";
}

HKEY RootFromName(const std::wstring& name) {
    return name == L"HKLM" ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

}  // namespace reg
