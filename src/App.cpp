#include "App.h"

#include "AutoStart.h"
#include "clients/AndroidClient.h"
#include "clients/ClientManager.h"
#include "server/WifiDropServer.h"
#include "TrayIcon.h"
#include "utils/DesktopFolders.h"
#include "utils/Log.h"
#include "utils/Utf.h"
#include "utils/WebClientService.h"

#include <shellapi.h>

#include <algorithm>
#include <exception>
#include <sstream>
#include <vector>

namespace {

bool CanOpenClientLocation(const AndroidClient &client) {
    return !client.driveLetter.empty() || (client.mountReady && !client.webDavHost.empty() && client.webDavPort > 0);
}

std::wstring BuildClientOpenPath(const AndroidClient &client) {
    if (!client.driveLetter.empty()) {
        return Utf::Utf8ToWide(client.driveLetter) + L":\\";
    }

    if (client.webDavHost.empty() || client.webDavPort <= 0) {
        return L"";
    }

    std::wstring basePath = Utf::Utf8ToWide(client.webDavBasePath);
    std::replace(basePath.begin(), basePath.end(), L'/', L'\\');
    while (!basePath.empty() && basePath.front() == L'\\') {
        basePath.erase(basePath.begin());
    }
    while (!basePath.empty() && basePath.back() == L'\\') {
        basePath.pop_back();
    }

    std::wstring uncPath = L"\\\\";
    uncPath += Utf::Utf8ToWide(client.webDavHost);
    uncPath += L"@";
    uncPath += std::to_wstring(client.webDavPort);
    uncPath += L"\\DavWWWRoot";
    if (!basePath.empty()) {
        uncPath += L"\\" + basePath;
    }

    return uncPath;
}

std::wstring BuildConnectedDevicesText(const std::vector<AndroidClient> &clients) {
    if (clients.empty()) {
        return L"Активных Android-устройств нет.";
    }

    std::wstringstream stream;
    stream << L"Активные устройства: " << clients.size() << L"\r\n\r\n";
    for (const AndroidClient &client : clients) {
        stream << L"Имя: " << client.driveName << L"\r\n";
        stream << L"Client ID: " << Utf::Utf8ToWide(client.clientId) << L"\r\n";
        stream << L"IP: " << Utf::Utf8ToWide(client.remoteIp) << L"\r\n";
        stream << L"WebDAV: " << Utf::Utf8ToWide(client.webDavHost) << L":" << client.webDavPort << L"\r\n";
        stream << L"WebDAV готов: " << (client.mountReady ? L"да" : L"нет") << L"\r\n";
        stream << L"Буква диска: ";
        if (client.driveLetter.empty()) {
            stream << L"не назначена";
        } else {
            stream << Utf::Utf8ToWide(client.driveLetter);
        }
        stream << L"\r\n\r\n";
    }

    stream << L"Открытие из трея использует смонтированный диск, если он есть, "
              L"или прямой WebDAV-путь устройства.";
    return stream.str();
}

}  // namespace

App::~App() = default;

int App::Run(HINSTANCE instanceHandle, int) {
    instanceHandle_ = instanceHandle;
    if (!Initialize(instanceHandle)) {
        Shutdown();
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    Shutdown();
    return static_cast<int>(message.wParam);
}

bool App::Initialize(HINSTANCE instanceHandle) {
    Log::Initialize();
    Log::Info("WiFiDrop starting");
    WebClientService::EnsureRunning();

    autoStart_ = std::make_unique<AutoStart>();
    clientManager_ = std::make_unique<ClientManager>();
    trayIcon_ = std::make_unique<TrayIcon>();
    server_ = std::make_unique<WifiDropServer>(*clientManager_);

    if (!trayIcon_->Initialize(
            instanceHandle,
            L"WiFiDrop",
            [this]() { OpenIncomingFolder(); },
            [this]() { ShowConnectedDevices(); },
            [this]() { ToggleAutostart(); },
            [this]() { ExitApplication(); },
            [this]() { HandleTrayDoubleClick(); },
            [this]() { return autoStart_->IsEnabled(); },
            [this]() { return BuildTrayDeviceMenuItems(); },
            [this](const std::string &clientId) { OpenClientDrive(clientId); })) {
        Log::Error("Failed to initialize tray icon");
        ShowMessage(L"WiFiDrop", L"Не удалось создать иконку в системном трее.", MB_OK | MB_ICONERROR);
        return false;
    }

    if (!server_->Start()) {
        Log::Error("Failed to start server");
        ShowMessage(L"WiFiDrop", L"Не удалось запустить локальный сервер WiFiDrop.", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

void App::Shutdown() {
    if (server_) {
        server_->Stop();
    }
    if (trayIcon_) {
        trayIcon_->Shutdown();
    }
    Log::Info("WiFiDrop stopped");
    Log::Shutdown();
}

void App::OpenIncomingFolder() {
    try {
        const auto folder = DesktopFolders::EnsureIncomingFolder();
        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        if (result <= 32) {
            Log::Error("ShellExecuteW failed for incoming folder");
            ShowMessage(L"WiFiDrop", L"Не удалось открыть папку входящих файлов.", MB_OK | MB_ICONERROR);
        }
    } catch (const std::exception &exception) {
        Log::Error(std::string("Failed to open incoming folder: ") + exception.what());
        ShowMessage(L"WiFiDrop", L"Не удалось подготовить папку входящих файлов.", MB_OK | MB_ICONERROR);
    }
}

void App::ShowConnectedDevices() {
    const auto clients = clientManager_->ListClients();
    ShowMessage(L"Connected Devices", BuildConnectedDevicesText(clients), MB_OK | MB_ICONINFORMATION);
}

std::vector<TrayDeviceMenuItem> App::BuildTrayDeviceMenuItems() const {
    const auto clients = clientManager_->ListClients();
    std::vector<TrayDeviceMenuItem> menuItems;
    menuItems.reserve(clients.size());
    for (const AndroidClient &client : clients) {
        menuItems.push_back({
            .clientId = client.clientId,
            .displayName = client.driveName,
            .canOpen = CanOpenClientLocation(client),
        });
    }
    return menuItems;
}

void App::OpenClientDrive(const std::string &clientId) {
    const auto clients = clientManager_->ListClients();
    for (const AndroidClient &client : clients) {
        if (client.clientId != clientId) {
            continue;
        }

        if (client.driveLetter.empty()) {
            if (!client.mountReady) {
                ShowMessage(L"WiFiDrop",
                            L"Android-устройство подключено, но WebDAV-сервер на нём пока не запущен.",
                            MB_OK | MB_ICONINFORMATION);
                return;
            }

            const bool webClientReady = WebClientService::EnsureRunning();
            const std::wstring webDavPath = BuildClientOpenPath(client);
            if (webDavPath.empty()) {
                ShowMessage(L"WiFiDrop",
                            L"Устройство подключено, но для него недоступен путь открытия.",
                            MB_OK | MB_ICONINFORMATION);
                return;
            }

            const auto webDavResult = reinterpret_cast<INT_PTR>(
                ShellExecuteW(nullptr, L"open", webDavPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
            if (webDavResult <= 32) {
                Log::Error("ShellExecuteW failed for client WebDAV path");
                ShowMessage(L"WiFiDrop",
                            webClientReady
                                ? L"Не удалось открыть WebDAV-путь устройства."
                                : L"Не удалось запустить службу WebClient и открыть WebDAV-путь устройства.",
                            MB_OK | MB_ICONERROR);
            }
            return;
        }

        const std::wstring rootPath = BuildClientOpenPath(client);
        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(nullptr, L"open", rootPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
        if (result <= 32) {
            Log::Error("ShellExecuteW failed for client drive");
            ShowMessage(L"WiFiDrop", L"Не удалось открыть смонтированный диск устройства.", MB_OK | MB_ICONERROR);
        }
        return;
    }

    ShowMessage(L"WiFiDrop", L"Устройство уже отключено.", MB_OK | MB_ICONINFORMATION);
}

void App::ToggleAutostart() {
    const bool enabled = autoStart_->IsEnabled();
    const bool targetState = !enabled;
    if (!autoStart_->SetEnabled(targetState)) {
        Log::Error("Failed to update autostart registry value");
        ShowMessage(L"WiFiDrop", L"Не удалось изменить автозапуск.", MB_OK | MB_ICONERROR);
        return;
    }

    Log::Info(targetState ? "Autostart enabled" : "Autostart disabled");
}

void App::HandleTrayDoubleClick() {
    const auto clients = clientManager_->ListClients();
    if (clients.empty()) {
        OpenIncomingFolder();
        return;
    }

    if (clients.size() == 1U) {
        OpenClientDrive(clients.front().clientId);
        return;
    }

    ShowConnectedDevices();
}

void App::ShowMessage(const std::wstring &title, const std::wstring &message, UINT flags) const {
    MessageBoxW(nullptr, message.c_str(), title.c_str(), flags);
}

void App::ExitApplication() {
    PostQuitMessage(0);
}
