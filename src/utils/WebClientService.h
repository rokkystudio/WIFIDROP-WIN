#pragma once

/// Управляет системной службой WebClient, от которой зависит открытие WebDAV путей в Explorer.
class WebClientService {
public:
    enum class HttpBasicOverHttpStatus {
        Enabled,
        Updated,
        AccessDenied,
        Failed,
    };

    /// Пытается убедиться, что служба WebClient запущена.
    /// Возвращает true, если служба уже работала или была успешно запущена.
    static bool EnsureRunning();

    /// Пытается включить поддержку Basic-аутентификации WebDAV по обычному HTTP.
    /// Для изменения системного параметра и перезапуска WebClient могут потребоваться права администратора.
    static HttpBasicOverHttpStatus EnsureHttpBasicOverHttpEnabled();
};
