#include "AutoStart.h"

#include "utils/Log.h"

#include <windows.h>

namespace {

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"WiFiDrop";

}  // namespace

bool AutoStart::IsEnabled() const {
    HKEY keyHandle = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_READ, &keyHandle) != ERROR_SUCCESS) {
        return false;
    }

    DWORD valueType = 0;
    wchar_t valueBuffer[32768]{};
    DWORD valueSize = sizeof(valueBuffer);
    const LONG result = RegQueryValueExW(
        keyHandle,
        kValueName,
        nullptr,
        &valueType,
        reinterpret_cast<LPBYTE>(valueBuffer),
        &valueSize);
    RegCloseKey(keyHandle);

    if (result != ERROR_SUCCESS || valueType != REG_SZ) {
        return false;
    }

    return BuildCommandLine() == valueBuffer;
}

bool AutoStart::SetEnabled(bool enabled) const {
    HKEY keyHandle = nullptr;
    const LONG openResult = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        0,
        nullptr,
        0,
        KEY_SET_VALUE | KEY_QUERY_VALUE,
        nullptr,
        &keyHandle,
        nullptr);
    if (openResult != ERROR_SUCCESS) {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring commandLine = BuildCommandLine();
        result = RegSetValueExW(
            keyHandle,
            kValueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE *>(commandLine.c_str()),
            static_cast<DWORD>((commandLine.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(keyHandle, kValueName);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }

    RegCloseKey(keyHandle);
    return result == ERROR_SUCCESS;
}

std::wstring AutoStart::BuildCommandLine() const {
    wchar_t modulePath[MAX_PATH]{};
    constexpr DWORD modulePathCapacity = static_cast<DWORD>(sizeof(modulePath) / sizeof(modulePath[0]));
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, modulePathCapacity);
    if (length == 0 || length == modulePathCapacity) {
        Log::Error("GetModuleFileNameW failed while building autostart command");
        return L"";
    }

    return L"\"" + std::wstring(modulePath) + L"\" --tray";
}
