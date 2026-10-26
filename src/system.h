#pragma once

#include <windows.h>

#include <string>
#include <string_view>

std::wstring ErrorText(DWORD code);
std::string ToUtf8(std::wstring_view s);
std::wstring FromUtf8(std::string_view s);

bool IsElevated();
// Starts this exe again through UAC. False if the user declined or it failed.
bool RelaunchElevated(const wchar_t* params);

// Tells running apps (Explorer, taskbar, ...) to re-read settings that support it live.
void NotifySettingsChanged();
// Kills the shell's Explorer and waits for Windows to bring it back.
void RestartExplorer();
