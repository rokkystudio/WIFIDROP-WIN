#pragma once

#include <chrono>
#include <string>

/// Описывает состояние persistent session Android-клиента.
enum class AndroidSessionState {
    Pending,
    Active,
};

/// Хранит параметры подключенного Android-устройства и его control session.
class AndroidClient {
public:
    /// Формирует отображаемое имя диска по правилам протокола WiFiDrop.
    static std::wstring BuildDriveName(const std::string &deviceNameUtf8, const std::string &deviceNumberUtf8);

    std::string clientId;
    std::string deviceNameUtf8;
    std::string deviceNumberUtf8;
    std::string webDavHost;
    int webDavPort{0};
    std::string webDavBasePath;
    bool readOnly{false};
    bool mountReady{false};
    std::string remoteIp;
    std::string driveLetter;
    std::wstring driveName;
    AndroidSessionState sessionState{AndroidSessionState::Pending};
    std::chrono::steady_clock::time_point lastActivity{std::chrono::steady_clock::now()};
};
