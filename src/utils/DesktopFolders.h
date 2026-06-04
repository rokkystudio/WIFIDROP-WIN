#pragma once

#include <filesystem>

/// Возвращает служебные директории WiFiDrop в профиле текущего пользователя.
class DesktopFolders {
public:
    /// Возвращает путь к папке входящих файлов на рабочем столе и создаёт её при отсутствии.
    static std::filesystem::path EnsureIncomingFolder();

    /// Возвращает путь к директории логов WiFiDrop и создаёт её при отсутствии.
    static std::filesystem::path EnsureLogFolder();
};
