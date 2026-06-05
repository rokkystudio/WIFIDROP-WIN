#include "WifiDropServer.h"

#include "Protocol.h"
#include "utils/Log.h"
#include "utils/WebDavDriveMapper.h"

#include <ws2tcpip.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>

namespace {

constexpr std::size_t kMaxHeaderSize = 64 * 1024;
constexpr int kListenBacklog = SOMAXCONN;

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char symbol) {
        return static_cast<char>(std::tolower(symbol));
    });
    return value;
}

bool ReceiveHttpRequest(SOCKET socketHandle, HttpRequest &request) {
    std::string buffer;
    buffer.reserve(8192);
    std::array<char, 4096> chunk{};

    std::size_t headerEnd = std::string::npos;
    while ((headerEnd = buffer.find("\r\n\r\n")) == std::string::npos) {
        const int received = recv(socketHandle, chunk.data(), static_cast<int>(chunk.size()), 0);
        if (received <= 0) {
            return false;
        }
        buffer.append(chunk.data(), received);
        if (buffer.size() > kMaxHeaderSize) {
            return false;
        }
    }

    std::string headerBlock = buffer.substr(0, headerEnd);
    std::istringstream headerStream(headerBlock);
    std::string requestLine;
    if (!std::getline(headerStream, requestLine)) {
        return false;
    }
    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }

    std::istringstream requestLineStream(requestLine);
    if (!(requestLineStream >> request.method >> request.target >> request.httpVersion)) {
        return false;
    }

    if (const auto queryPosition = request.target.find('?'); queryPosition != std::string::npos) {
        request.path = request.target.substr(0, queryPosition);
        request.query = request.target.substr(queryPosition + 1);
    } else {
        request.path = request.target;
        request.query.clear();
    }

    for (std::string headerLine; std::getline(headerStream, headerLine);) {
        if (!headerLine.empty() && headerLine.back() == '\r') {
            headerLine.pop_back();
        }
        const auto separator = headerLine.find(':');
        if (separator == std::string::npos) {
            continue;
        }
        std::string key = ToLowerAscii(headerLine.substr(0, separator));
        std::string value = headerLine.substr(separator + 1);
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
            value.erase(value.begin());
        }
        request.headers.emplace(std::move(key), std::move(value));
    }

    request.body = buffer.substr(headerEnd + 4);

    const auto contentLengthIterator = request.headers.find("content-length");
    if (contentLengthIterator != request.headers.end()) {
        std::size_t expectedBodySize = 0;
        try {
            expectedBodySize = static_cast<std::size_t>(std::stoull(contentLengthIterator->second));
        } catch (...) {
            return false;
        }

        while (request.body.size() < expectedBodySize) {
            const int received = recv(socketHandle, chunk.data(), static_cast<int>(chunk.size()), 0);
            if (received <= 0) {
                return false;
            }
            request.body.append(chunk.data(), received);
        }
        request.body.resize(expectedBodySize);
        return true;
    }

    if (request.method == "PUT" || request.method == "POST") {
        for (;;) {
            const int received = recv(socketHandle, chunk.data(), static_cast<int>(chunk.size()), 0);
            if (received == 0) {
                break;
            }
            if (received < 0) {
                return false;
            }
            request.body.append(chunk.data(), received);
        }
    }

    return true;
}

bool SendAll(SOCKET socketHandle, std::string_view bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const int sent = send(socketHandle,
                              bytes.data() + offset,
                              static_cast<int>(bytes.size() - offset),
                              0);
        if (sent <= 0) {
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

std::string BuildHttpResponse(const HttpResponse &response) {
    std::ostringstream stream;
    stream << "HTTP/1.1 " << response.statusCode << ' ' << response.reasonPhrase << "\r\n";
    for (const auto &[key, value] : response.headers) {
        stream << key << ": " << value << "\r\n";
    }
    stream << "Content-Length: " << response.body.size() << "\r\n";
    stream << "Connection: close\r\n\r\n";
    stream << response.body;
    return stream.str();
}

std::string SocketAddressToIp(const sockaddr_in &address) {
    char textBuffer[INET_ADDRSTRLEN]{};
    if (!InetNtopA(AF_INET, const_cast<IN_ADDR *>(&address.sin_addr), textBuffer, INET_ADDRSTRLEN)) {
        return "unknown";
    }
    return textBuffer;
}

HttpResponse MakePlainResponse(int statusCode, std::string reasonPhrase, std::string body) {
    HttpResponse response;
    response.statusCode = statusCode;
    response.reasonPhrase = std::move(reasonPhrase);
    response.headers.emplace("Content-Type", "text/plain; charset=utf-8");
    response.body = std::move(body);
    return response;
}

}  // namespace

WifiDropServer::WifiDropServer(ClientManager &clientManager)
    : clientManager_(clientManager),
      uploadController_(),
      controlServer_(clientManager),
      discoveryResponder_() {
}

bool WifiDropServer::Start() {
    if (running_.exchange(true)) {
        return true;
    }

    WSADATA winsockData{};
    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
        running_ = false;
        Log::Error("WSAStartup failed");
        return false;
    }

    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET) {
        Log::Error("socket(AF_INET, SOCK_STREAM) failed");
        WSACleanup();
        running_ = false;
        return false;
    }

    BOOL exclusiveAddress = TRUE;
    setsockopt(listenSocket_, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&exclusiveAddress), sizeof(exclusiveAddress));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(Protocol::kTcpPort);

    if (bind(listenSocket_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == SOCKET_ERROR) {
        Log::Error("bind failed on TCP port " + std::to_string(Protocol::kTcpPort));
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        WSACleanup();
        running_ = false;
        return false;
    }

    if (listen(listenSocket_, kListenBacklog) == SOCKET_ERROR) {
        Log::Error("listen failed");
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        WSACleanup();
        running_ = false;
        return false;
    }

    discoveryResponder_.Start();

    Log::Info("TCP server listening on port " + std::to_string(Protocol::kTcpPort));
    acceptThread_ = std::thread(&WifiDropServer::AcceptLoop, this);
    maintenanceThread_ = std::thread(&WifiDropServer::MaintenanceLoop, this);
    return true;
}

void WifiDropServer::Stop() {
    if (!running_.exchange(false)) {
        return;
    }

    discoveryResponder_.Stop();

    if (listenSocket_ != INVALID_SOCKET) {
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }

    CloseActiveClientSockets();

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }
    if (maintenanceThread_.joinable()) {
        maintenanceThread_.join();
    }

    const auto removedClients = clientManager_.Clear();
    for (const auto &client : removedClients) {
        WebDavDriveMapper::Unmount(client);
    }
    WSACleanup();
}

void WifiDropServer::AcceptLoop() {
    while (running_) {
        sockaddr_in remoteAddress{};
        int remoteAddressSize = sizeof(remoteAddress);
        const SOCKET clientSocket = accept(
            listenSocket_,
            reinterpret_cast<sockaddr *>(&remoteAddress),
            &remoteAddressSize);
        if (clientSocket == INVALID_SOCKET) {
            if (running_) {
                Log::Warn("accept returned INVALID_SOCKET");
            }
            continue;
        }

        const std::string remoteIp = SocketAddressToIp(remoteAddress);
        {
            std::lock_guard lock(activeSocketsMutex_);
            activeSockets_.insert(clientSocket);
        }

        std::thread(&WifiDropServer::HandleClient, this, clientSocket, remoteIp).detach();
    }
}

void WifiDropServer::HandleClient(SOCKET clientSocket, std::string remoteIp) {
    HttpRequest request;
    request.remoteIp = std::move(remoteIp);

    if (!ReceiveHttpRequest(clientSocket, request)) {
        Log::Warn("Failed to parse HTTP request");
        closesocket(clientSocket);
        std::lock_guard lock(activeSocketsMutex_);
        activeSockets_.erase(clientSocket);
        return;
    }

    HttpResponse response;
    const auto disposition = controlServer_.HandleRequest(request, clientSocket, response);
    if (disposition == ControlRequestDisposition::NotHandled) {
        if (!uploadController_.HandleRequest(request, response)) {
            response = MakePlainResponse(404, "Not Found", "Not Found");
        }
        const std::string bytes = BuildHttpResponse(response);
        SendAll(clientSocket, bytes);
        closesocket(clientSocket);
    } else if (disposition == ControlRequestDisposition::ResponseReady) {
        const std::string bytes = BuildHttpResponse(response);
        SendAll(clientSocket, bytes);
        closesocket(clientSocket);
    }

    std::lock_guard lock(activeSocketsMutex_);
    activeSockets_.erase(clientSocket);
}

void WifiDropServer::MaintenanceLoop() {
    while (running_) {
        std::this_thread::sleep_for(Protocol::kControlHeartbeatInterval);
        const auto removedClients = clientManager_.RemoveInactive(Protocol::kControlDisconnectTimeout);
        for (const auto &client : removedClients) {
            WebDavDriveMapper::Unmount(client);
            Log::Info("Client disconnected by timeout: " + client.clientId);
        }
    }
}

void WifiDropServer::CloseActiveClientSockets() {
    std::lock_guard lock(activeSocketsMutex_);
    for (const SOCKET clientSocket : activeSockets_) {
        shutdown(clientSocket, SD_BOTH);
        closesocket(clientSocket);
    }
    activeSockets_.clear();
}
