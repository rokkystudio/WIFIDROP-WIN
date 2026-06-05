#include "ClientManager.h"

void ClientManager::AddClient(const AndroidClient &client) {
    std::lock_guard lock(mutex_);
    clients_[client.clientId] = client;
}

bool ClientManager::MarkSessionStarted(const std::string &clientId) {
    std::lock_guard lock(mutex_);
    const auto iterator = clients_.find(clientId);
    if (iterator == clients_.end()) {
        return false;
    }

    iterator->second.sessionState = AndroidSessionState::Active;
    iterator->second.lastActivity = std::chrono::steady_clock::now();
    return true;
}

bool ClientManager::TouchClient(const std::string &clientId) {
    std::lock_guard lock(mutex_);
    const auto iterator = clients_.find(clientId);
    if (iterator == clients_.end()) {
        return false;
    }

    iterator->second.lastActivity = std::chrono::steady_clock::now();
    return true;
}

bool ClientManager::Contains(const std::string &clientId) const {
    std::lock_guard lock(mutex_);
    return clients_.contains(clientId);
}

std::optional<AndroidClient> ClientManager::RemoveClient(const std::string &clientId) {
    std::lock_guard lock(mutex_);
    const auto iterator = clients_.find(clientId);
    if (iterator == clients_.end()) {
        return std::nullopt;
    }

    AndroidClient removedClient = iterator->second;
    clients_.erase(iterator);
    return removedClient;
}

std::vector<AndroidClient> ClientManager::ListClients() const {
    std::lock_guard lock(mutex_);
    std::vector<AndroidClient> result;
    result.reserve(clients_.size());
    for (const auto &[clientId, client] : clients_) {
        result.push_back(client);
    }
    return result;
}

std::vector<AndroidClient> ClientManager::RemoveInactive(std::chrono::steady_clock::duration timeout) {
    const auto now = std::chrono::steady_clock::now();
    std::vector<AndroidClient> removedClients;

    std::lock_guard lock(mutex_);
    for (auto iterator = clients_.begin(); iterator != clients_.end();) {
        if (now - iterator->second.lastActivity >= timeout) {
            removedClients.push_back(iterator->second);
            iterator = clients_.erase(iterator);
        } else {
            ++iterator;
        }
    }

    return removedClients;
}

std::vector<AndroidClient> ClientManager::Clear() {
    std::lock_guard lock(mutex_);
    std::vector<AndroidClient> removedClients;
    removedClients.reserve(clients_.size());
    for (const auto &[clientId, client] : clients_) {
        removedClients.push_back(client);
    }
    clients_.clear();
    return removedClients;
}
