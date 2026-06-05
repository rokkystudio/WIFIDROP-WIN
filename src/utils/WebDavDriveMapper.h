#pragma once

#include "clients/AndroidClient.h"

#include <cstdint>
#include <optional>
#include <string>

/// Монтирует WebDAV endpoint Android-устройства в букву диска Windows.
class WebDavDriveMapper {
public:
    struct MountResult {
        std::optional<std::string> driveLetter;
        std::string errorMessage;
        std::uint32_t errorCode{0};
    };

    /// Пытается смонтировать WebDAV в первую свободную букву диска.
    /// Возвращает букву без двоеточия, например "Z", либо std::nullopt при ошибке.
    static MountResult Mount(const AndroidClient &client);

    /// Размонтирует ранее выданную букву диска, если она есть у клиента.
    static void Unmount(const AndroidClient &client);

private:
    static std::wstring BuildRemoteName(const AndroidClient &client);
    static std::optional<std::wstring> FindFreeDriveLetter();
};
