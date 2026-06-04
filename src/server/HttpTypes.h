#pragma once

#include <string>
#include <unordered_map>

/// Описывает разобранный HTTP-запрос для внутренних обработчиков WiFiDrop.
struct HttpRequest {
    std::string method;
    std::string target;
    std::string path;
    std::string query;
    std::string httpVersion;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::string remoteIp;
};

/// Описывает HTTP-ответ для внутренних обработчиков WiFiDrop.
struct HttpResponse {
    int statusCode{200};
    std::string reasonPhrase{"OK"};
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};
