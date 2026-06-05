#pragma once

#include "AutoStart.h"
#include "ConnectedDevicesWindow.h"
#include "TrayIcon.h"
#include "clients/ClientManager.h"
#include "server/WifiDropServer.h"

#include <windows.h>

#include <memory>
#include <vector>

/// Координирует жизненный цикл tray-приложения, локального сервера
/// и пользовательских действий из системного трея.
class App {
public:
    /// Освобождает модули приложения после завершения цикла сообщений.
    ~App();

    /// Запускает приложение, инициализирует модули и обрабатывает цикл сообщений WinAPI.
    int Run(HINSTANCE instanceHandle, int showCommand);

private:
    /// Инициализирует долгоживущие сервисы приложения.
    bool Initialize(HINSTANCE instanceHandle);

    /// Завершает работу сервисов и освобождает системные ресурсы.
    void Shutdown();

    /// Открывает папку входящих файлов на рабочем столе текущего пользователя.
    void OpenIncomingFolder();

    /// Показывает список активных Android-устройств.
    void ShowConnectedDevices();

    /// Возвращает список устройств для контекстного меню системного трея.
    std::vector<TrayDeviceMenuItem> BuildTrayDeviceMenuItems() const;

    /// Открывает смонтированный диск выбранного Android-устройства.
    void OpenClientDrive(const std::string &clientId);

    /// Переключает состояние автозапуска в реестре текущего пользователя.
    void ToggleAutostart();

    /// Обрабатывает двойной клик по иконке в трее.
    void HandleTrayDoubleClick();

    /// Показывает сообщение пользователю из UI-потока.
    void ShowMessage(const std::wstring &title, const std::wstring &message, UINT flags) const;

    /// Завершает цикл сообщений приложения.
    void ExitApplication();

    HINSTANCE instanceHandle_{nullptr};
    std::unique_ptr<AutoStart> autoStart_;
    std::unique_ptr<ClientManager> clientManager_;
    std::unique_ptr<ConnectedDevicesWindow> connectedDevicesWindow_;
    std::unique_ptr<TrayIcon> trayIcon_;
    std::unique_ptr<WifiDropServer> server_;
};
