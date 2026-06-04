#pragma once

#include "clients/ClientManager.h"
#include "ControlServer.h"
#include "DiscoveryResponder.h"
#include "UploadController.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_set>

#include <winsock2.h>

/// Поднимает TCP/HTTP сервер WiFiDrop, маршрутизирует запросы и
/// поддерживает фоновый responder discovery.
class WifiDropServer {
public:
    /// Создаёт сервер и связывает его с менеджером Android-клиентов.
    explicit WifiDropServer(ClientManager &clientManager);

    /// Открывает сетевые сокеты и запускает фоновые потоки сервера.
    bool Start();

    /// Останавливает сетевые сокеты и завершает фоновые потоки.
    void Stop();

private:
    /// Принимает входящие TCP-подключения и создаёт обработчики запросов.
    void AcceptLoop();

    /// Обрабатывает один TCP-клиентский сокет до завершения HTTP-запроса.
    void HandleClient(SOCKET clientSocket, std::string remoteIp);

    /// Обходит менеджер клиентов и удаляет сессии с истекшим timeout.
    void MaintenanceLoop();

    /// Закрывает все активные клиентские сокеты.
    void CloseActiveClientSockets();

    ClientManager &clientManager_;
    UploadController uploadController_;
    ControlServer controlServer_;
    DiscoveryResponder discoveryResponder_;
    std::atomic_bool running_{false};
    SOCKET listenSocket_{INVALID_SOCKET};
    std::thread acceptThread_;
    std::thread maintenanceThread_;
    std::mutex activeSocketsMutex_;
    std::unordered_set<SOCKET> activeSockets_;
};
