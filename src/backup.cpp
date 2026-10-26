#include "backup.h"

#include "app.h"
#include "system.h"

#include <shlobj.h>

#include <cwctype>

namespace {

std::wstring Lower(std::wstring s) {
    for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c));
    return s;
}

std::wstring ValueId(HKEY root, const std::wstring& path, const std::wstring& name) {
    return Lower(std::wstring(reg::RootName(root)) + L"|" + path + L"|" + name);
}

std::wstring KeyId(HKEY root, const std::wstring& path) {
    return Lower(std::wstring(reg::RootName(root)) + L"|" + path);
}

std::wstring ToHex(const std::vector<BYTE>& bytes) {
    static const wchar_t digits[] = L"0123456789abcdef";
    std::wstring s;
    for (BYTE b : bytes) {
        s += digits[b >> 4];
        s += digits[b & 15];
    }
    return s;
}

std::vector<BYTE> FromHex(const std::wstring& s) {
    std::vector<BYTE> out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back(static_cast<BYTE>(std::wcstoul(s.substr(i, 2).c_str(), nullptr, 16)));
    return out;
}

std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    for (;;) {
        size_t pos = s.find(sep, start);
        parts.push_back(s.substr(start, pos - start));
        if (pos == std::wstring::npos) return parts;
        start = pos + 1;
    }
}

std::wstring AppDataDir() {
    PWSTR base = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &base))) dir = std::wstring(base) + L"\\" APP_NAME;
    CoTaskMemFree(base);
    return dir;
}

}  // namespace

Backup::Backup() {
    std::wstring dir = AppDataDir();
    if (!dir.empty()) file_ = dir + L"\\backup.txt";
    Load();
}

void Backup::Load() {
    if (file_.empty()) return;
    HANDLE f = CreateFileW(file_.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    std::string raw;
    LARGE_INTEGER size{};
    if (GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < (16 << 20)) {
        raw.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ReadFile(f, raw.data(), static_cast<DWORD>(raw.size()), &read, nullptr);
        raw.resize(read);
    }
    CloseHandle(f);

    for (std::wstring line : Split(FromUtf8(raw), L'\n')) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        auto p = Split(line, L'\t');
        if (p.size() == 6 && p[0] == L"V") {
            ValueRec r{reg::RootFromName(p[1]), p[2], p[3], std::nullopt};
            if (p[4] != L"-") r.data = RegData{static_cast<DWORD>(std::wcstoul(p[4].c_str(), nullptr, 10)), FromHex(p[5])};
            values_[ValueId(r.root, r.path, r.name)] = std::move(r);
        } else if (p.size() == 4 && p[0] == L"K") {
            KeyRec r{reg::RootFromName(p[1]), p[2], p[3] == L"1"};
            keys_[KeyId(r.root, r.path)] = std::move(r);
        }
    }
}

bool Backup::Save() const {
    if (file_.empty()) return false;
    if (Empty()) {
        DeleteFileW(file_.c_str());
        return true;
    }

    std::wstring text;
    for (const auto& [id, r] : values_) {
        text += L"V\t" + std::wstring(reg::RootName(r.root)) + L"\t" + r.path + L"\t" + r.name + L"\t";
        text += r.data ? std::to_wstring(r.data->type) + L"\t" + ToHex(r.data->bytes) : L"-\t";
        text += L"\n";
    }
    for (const auto& [id, r] : keys_)
        text += L"K\t" + std::wstring(reg::RootName(r.root)) + L"\t" + r.path + L"\t" + (r.existed ? L"1" : L"0") + L"\n";

    CreateDirectoryW(AppDataDir().c_str(), nullptr);
    std::wstring tmp = file_ + L".tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    std::string utf8 = ToUtf8(text);
    DWORD written = 0;
    bool ok = WriteFile(f, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr) && written == utf8.size();
    CloseHandle(f);
    // Write-then-rename so a crash never leaves a half-written backup behind.
    return ok && MoveFileExW(tmp.c_str(), file_.c_str(), MOVEFILE_REPLACE_EXISTING);
}

bool Backup::RememberValue(HKEY root, const std::wstring& path, const std::wstring& name) {
    auto id = ValueId(root, path, name);
    if (values_.count(id)) return false;
    values_[id] = ValueRec{root, path, name, reg::Read(root, path, name)};
    return true;
}

void Backup::RememberKey(HKEY root, const std::wstring& path) {
    auto id = KeyId(root, path);
    if (!keys_.count(id)) keys_[id] = KeyRec{root, path, reg::KeyExists(root, path)};
}

void Backup::ForgetValue(HKEY root, const std::wstring& path, const std::wstring& name) {
    values_.erase(ValueId(root, path, name));
}

std::optional<LSTATUS> Backup::RestoreValue(HKEY root, const std::wstring& path, const std::wstring& name) {
    auto it = values_.find(ValueId(root, path, name));
    if (it == values_.end()) return std::nullopt;
    const ValueRec& r = it->second;
    LSTATUS st = r.data ? reg::Write(r.root, r.path, r.name, *r.data) : reg::Delete(r.root, r.path, r.name);
    if (st == ERROR_SUCCESS) values_.erase(it);
    return st;
}

std::optional<LSTATUS> Backup::RestoreKey(HKEY root, const std::wstring& path) {
    auto it = keys_.find(KeyId(root, path));
    if (it == keys_.end()) return std::nullopt;
    LSTATUS st = it->second.existed ? ERROR_SUCCESS : reg::DeleteTree(it->second.root, it->second.path);
    if (st == ERROR_SUCCESS) keys_.erase(it);
    return st;
}

Backup::Result Backup::RestoreAll() {
    Result result;
    auto values = values_;
    for (const auto& [id, r] : values) {
        LSTATUS st = *RestoreValue(r.root, r.path, r.name);
        if (st == ERROR_SUCCESS)
            ++result.restored;
        else
            result.failed.push_back(std::wstring(reg::RootName(r.root)) + L"\\" + r.path + L"\\" + r.name + L": " + ErrorText(st));
    }
    auto keys = keys_;
    for (const auto& [id, r] : keys) {
        LSTATUS st = *RestoreKey(r.root, r.path);
        if (st != ERROR_SUCCESS)
            result.failed.push_back(std::wstring(reg::RootName(r.root)) + L"\\" + r.path + L": " + ErrorText(st));
    }
    return result;
}
