#pragma once

#include "AndroidClient.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/// Управляет подключенными Android-устройствами и жизненным циклом
/// их control session на первом этапе MVP.
class ClientManager {
public:
    /// Добавляет клиента или заменяет существующую запись с тем же clientId.
    void AddClient(const AndroidClient &client);

    /// Помечает начало persistent session для клиента.
    bool MarkSessionStarted(const std::string &clientId);

    /// Обновляет timestamp активности клиента.
    bool TouchClient(const std::string &clientId);

    /// Возвращает признак наличия клиента в менеджере.
    bool Contains(const std::string &clientId) const;

    /// Удаляет клиента по идентификатору.
    bool RemoveClient(const std::string &clientId);

    /// Возвращает все активные клиентские записи.
    std::vector<AndroidClient> ListClients() const;

    /// Удаляет клиентов, у которых истёк timeout активности.
    std::vector<AndroidClient> RemoveInactive(std::chrono::steady_clock::duration timeout);

    /// Очищает все клиентские записи.
    void Clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, AndroidClient> clients_;
};
