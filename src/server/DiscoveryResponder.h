#pragma once

#include <atomic>
#include <thread>

#include <winsock2.h>

/// Отвечает на UDP discovery datagram для поиска Windows-сервера в локальной сети.
class DiscoveryResponder {
public:
    /// Запускает UDP responder в отдельном потоке.
    void Start();

    /// Останавливает UDP responder и закрывает его сокет.
    void Stop();

private:
    /// Слушает UDP сокет и отправляет JSON-ответы на валидные discovery-запросы.
    void RunLoop();

    std::atomic_bool running_{false};
    SOCKET socket_{INVALID_SOCKET};
    std::thread thread_;
};
