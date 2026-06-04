#pragma once

/// Управляет системной службой WebClient, от которой зависит открытие WebDAV путей в Explorer.
class WebClientService {
public:
    /// Пытается убедиться, что служба WebClient запущена.
    /// Возвращает true, если служба уже работала или была успешно запущена.
    static bool EnsureRunning();
};
