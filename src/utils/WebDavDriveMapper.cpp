#include "WebDavDriveMapper.h"

#include "Log.h"
#include "Utf.h"
#include "WinFspDriveHost.h"

#include <windows.h>
#include <winnetwk.h>

#include <optional>
#include <string>

namespace {

std::string TrimColon(const std::wstring &localName) {
    if (localName.empty()) {
        return "";
    }
    std::wstring value = localName;
    if (!value.empty() && value.back() == L':') {
        value.pop_back();
    }
    return Utf::WideToUtf8(value);
}

std::string BuildMountErrorMessage(const AndroidClient &client, DWORD errorCode) {
    std::string message = "WebDAV mount failed with error " + std::to_string(errorCode);
    if (errorCode == ERROR_BAD_NET_NAME) {
        message += ". Windows WebClient did not accept the Android WebDAV endpoint as a valid network location.";
    }
    return message;
}

}  // namespace

WebDavDriveMapper::MountResult WebDavDriveMapper::Mount(const AndroidClient &client) {
#if WIFIDROP_ENABLE_WINFSP
    std::string errorMessage;
    if (const auto mountedDrive = WinFspDriveHost::Mount(client, &errorMessage); mountedDrive.has_value()) {
        return MountResult{
            .driveLetter = *mountedDrive,
        };
    }

    Log::Warn(errorMessage);
    return MountResult{
        .driveLetter = std::nullopt,
        .errorMessage = errorMessage,
    };
#else
    if (client.webDavHost.empty() || client.webDavPort <= 0) {
        Log::Warn("WebDAV mount skipped because endpoint is incomplete");
        return MountResult{
            .driveLetter = std::nullopt,
            .errorMessage = "WebDAV endpoint is incomplete",
        };
    }

    const std::wstring remoteName = BuildRemoteName(client);
    if (remoteName.empty()) {
        Log::Warn("WebDAV mount skipped because remote name is empty");
        return MountResult{
            .driveLetter = std::nullopt,
            .errorMessage = "WebDAV remote name is empty",
        };
    }

    for (;;) {
        const auto localName = FindFreeDriveLetter();
        if (!localName.has_value()) {
            Log::Warn("WebDAV mount failed because no free drive letters are available");
            return MountResult{
                .driveLetter = std::nullopt,
                .errorMessage = "No free drive letters are available",
            };
        }

        NETRESOURCEW resource{};
        resource.dwType = RESOURCETYPE_DISK;
        resource.lpLocalName = const_cast<wchar_t *>(localName->c_str());
        resource.lpRemoteName = const_cast<wchar_t *>(remoteName.c_str());

        Log::Info("Attempting WebDAV drive mount: " + Utf::WideToUtf8(*localName) + " -> " + Utf::WideToUtf8(remoteName));
        const DWORD result = WNetAddConnection2W(&resource, nullptr, nullptr, CONNECT_TEMPORARY);
        if (result == NO_ERROR) {
            const std::string driveLetter = TrimColon(*localName);
            Log::Info("WebDAV drive mounted: " + driveLetter);
            return MountResult{
                .driveLetter = driveLetter,
            };
        }

        if (result == ERROR_ALREADY_ASSIGNED || result == ERROR_DEVICE_ALREADY_REMEMBERED) {
            Log::Warn("Drive letter collision while mounting WebDAV, trying another letter. Error=" + std::to_string(result));
            continue;
        }

        const std::string errorMessage = BuildMountErrorMessage(client, result);
        Log::Warn(errorMessage);
        return MountResult{
            .driveLetter = std::nullopt,
            .errorMessage = errorMessage,
            .errorCode = result,
        };
    }
#endif
}

void WebDavDriveMapper::Unmount(const AndroidClient &client) {
#if WIFIDROP_ENABLE_WINFSP
    WinFspDriveHost::Unmount(client);
#else
    if (client.driveLetter.empty()) {
        return;
    }

    const std::wstring localName = Utf::Utf8ToWide(client.driveLetter) + L":";
    const DWORD result = WNetCancelConnection2W(localName.c_str(), CONNECT_UPDATE_PROFILE, TRUE);
    if (result == NO_ERROR || result == ERROR_NOT_CONNECTED) {
        Log::Info("WebDAV drive unmounted: " + client.driveLetter);
        return;
    }

    Log::Warn("WebDAV drive unmount failed for " + client.driveLetter + " with error " + std::to_string(result));
#endif
}

std::wstring WebDavDriveMapper::BuildRemoteName(const AndroidClient &client) {
    std::wstring remoteName = L"http://";
    remoteName += Utf::Utf8ToWide(client.webDavHost);
    remoteName += L"@";
    remoteName += std::to_wstring(client.webDavPort);
    remoteName += L"/";
    return remoteName;
}

std::optional<std::wstring> WebDavDriveMapper::FindFreeDriveLetter() {
    const DWORD logicalDrives = GetLogicalDrives();
    for (wchar_t letter = L'Z'; letter >= L'D'; --letter) {
        const DWORD bit = 1u << (letter - L'A');
        if ((logicalDrives & bit) == 0) {
            return std::wstring(1, letter) + L":";
        }
    }
    return std::nullopt;
}
