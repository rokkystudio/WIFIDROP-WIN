#include "WebClientService.h"

#include "Log.h"

#include <windows.h>

#include <winsvc.h>

#include <string>

namespace {

constexpr wchar_t kWebClientServiceName[] = L"WebClient";
constexpr wchar_t kWebClientParametersKey[] = L"SYSTEM\\CurrentControlSet\\Services\\WebClient\\Parameters";
constexpr wchar_t kBasicAuthLevelValue[] = L"BasicAuthLevel";
constexpr wchar_t kServerNotFoundCacheLifetimeValue[] = L"ServerNotFoundCacheLifeTimeInSec";
constexpr DWORD kStartTimeoutMs = 15000;
constexpr DWORD kPollIntervalMs = 250;

bool QueryRunningState(SC_HANDLE serviceHandle, SERVICE_STATUS_PROCESS &status) {
    DWORD bytesNeeded = 0;
    if (!QueryServiceStatusEx(serviceHandle,
                              SC_STATUS_PROCESS_INFO,
                              reinterpret_cast<LPBYTE>(&status),
                              sizeof(status),
                              &bytesNeeded)) {
        Log::Warn("QueryServiceStatusEx(WebClient) failed with error " + std::to_string(GetLastError()));
        return false;
    }
    return true;
}

bool WaitForState(SC_HANDLE serviceHandle,
                  DWORD targetState,
                  DWORD pendingState,
                  const char *timeoutMessage) {
    const DWORD startTick = GetTickCount();
    SERVICE_STATUS_PROCESS status{};
    while (GetTickCount() - startTick < kStartTimeoutMs) {
        if (!QueryRunningState(serviceHandle, status)) {
            return false;
        }

        if (status.dwCurrentState == targetState) {
            return true;
        }

        if (status.dwCurrentState != pendingState) {
            return false;
        }

        Sleep(kPollIntervalMs);
    }

    Log::Warn(timeoutMessage);
    return false;
}

bool WaitUntilRunning(SC_HANDLE serviceHandle) {
    return WaitForState(serviceHandle,
                        SERVICE_RUNNING,
                        SERVICE_START_PENDING,
                        "Timed out while waiting for WebClient service to start");
}

bool WaitUntilStopped(SC_HANDLE serviceHandle) {
    return WaitForState(serviceHandle,
                        SERVICE_STOPPED,
                        SERVICE_STOP_PENDING,
                        "Timed out while waiting for WebClient service to stop");
}

bool RestartWebClientService() {
    SC_HANDLE scmHandle = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scmHandle) {
        Log::Warn("OpenSCManagerW failed while restarting WebClient with error " + std::to_string(GetLastError()));
        return false;
    }

    SC_HANDLE serviceHandle = OpenServiceW(scmHandle,
                                           kWebClientServiceName,
                                           SERVICE_QUERY_STATUS | SERVICE_START | SERVICE_STOP);
    if (!serviceHandle) {
        Log::Warn("OpenServiceW(WebClient) failed while restarting with error " + std::to_string(GetLastError()));
        CloseServiceHandle(scmHandle);
        return false;
    }

    SERVICE_STATUS_PROCESS status{};
    const bool hasStatus = QueryRunningState(serviceHandle, status);
    if (hasStatus && status.dwCurrentState != SERVICE_STOPPED) {
        SERVICE_STATUS serviceStatus{};
        if (!ControlService(serviceHandle, SERVICE_CONTROL_STOP, &serviceStatus)) {
            const DWORD error = GetLastError();
            if (error != ERROR_SERVICE_NOT_ACTIVE) {
                Log::Warn("ControlService(WebClient, STOP) failed with error " + std::to_string(error));
                CloseServiceHandle(serviceHandle);
                CloseServiceHandle(scmHandle);
                return false;
            }
        } else if (!WaitUntilStopped(serviceHandle)) {
            CloseServiceHandle(serviceHandle);
            CloseServiceHandle(scmHandle);
            return false;
        }
    }

    if (!StartServiceW(serviceHandle, 0, nullptr)) {
        const DWORD error = GetLastError();
        if (error != ERROR_SERVICE_ALREADY_RUNNING) {
            Log::Warn("StartServiceW(WebClient) failed after configuration update with error " + std::to_string(error));
            CloseServiceHandle(serviceHandle);
            CloseServiceHandle(scmHandle);
            return false;
        }
    }

    const bool started = WaitUntilRunning(serviceHandle);
    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(scmHandle);
    return started;
}

WebClientService::HttpBasicOverHttpStatus MapRegistryAccessError(LSTATUS status) {
    if (status == ERROR_ACCESS_DENIED) {
        return WebClientService::HttpBasicOverHttpStatus::AccessDenied;
    }
    return WebClientService::HttpBasicOverHttpStatus::Failed;
}

}  // namespace

bool WebClientService::EnsureRunning() {
    SC_HANDLE scmHandle = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scmHandle) {
        Log::Warn("OpenSCManagerW failed for WebClient with error " + std::to_string(GetLastError()));
        return false;
    }

    SERVICE_STATUS_PROCESS status{};
    SC_HANDLE queryHandle = OpenServiceW(scmHandle, kWebClientServiceName, SERVICE_QUERY_STATUS);
    if (queryHandle && QueryRunningState(queryHandle, status) && status.dwCurrentState == SERVICE_RUNNING) {
        CloseServiceHandle(queryHandle);
        CloseServiceHandle(scmHandle);
        return true;
    }
    if (queryHandle) {
        CloseServiceHandle(queryHandle);
    }

    SC_HANDLE serviceHandle = OpenServiceW(scmHandle,
                                           kWebClientServiceName,
                                           SERVICE_QUERY_STATUS | SERVICE_START);
    if (!serviceHandle) {
        Log::Warn("OpenServiceW(WebClient) failed with error " + std::to_string(GetLastError()));
        CloseServiceHandle(scmHandle);
        return false;
    }

    const bool hasStatus = QueryRunningState(serviceHandle, status);
    if (hasStatus && status.dwCurrentState == SERVICE_RUNNING) {
        CloseServiceHandle(serviceHandle);
        CloseServiceHandle(scmHandle);
        return true;
    }

    if (!StartServiceW(serviceHandle, 0, nullptr)) {
        const DWORD error = GetLastError();
        if (error != ERROR_SERVICE_ALREADY_RUNNING) {
            Log::Warn("StartServiceW(WebClient) failed with error " + std::to_string(error));
            CloseServiceHandle(serviceHandle);
            CloseServiceHandle(scmHandle);
            return false;
        }
    }

    const bool started = WaitUntilRunning(serviceHandle);
    if (started) {
        Log::Info("WebClient service is running");
    }

    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(scmHandle);
    return started;
}

WebClientService::HttpBasicOverHttpStatus WebClientService::EnsureHttpBasicOverHttpEnabled() {
    HKEY queryKey = nullptr;
    LSTATUS status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, kWebClientParametersKey, 0, KEY_QUERY_VALUE, &queryKey);
    if (status != ERROR_SUCCESS) {
        Log::Warn("RegOpenKeyExW(WebClient Parameters, query) failed with error " + std::to_string(status));
        return MapRegistryAccessError(status);
    }

    DWORD basicAuthLevel = 0;
    DWORD type = 0;
    DWORD size = sizeof(basicAuthLevel);
    status = RegQueryValueExW(queryKey, kBasicAuthLevelValue, nullptr, &type, reinterpret_cast<LPBYTE>(&basicAuthLevel), &size);
    if (status != ERROR_SUCCESS || type != REG_DWORD) {
        RegCloseKey(queryKey);
        Log::Warn("RegQueryValueExW(BasicAuthLevel) failed with error " + std::to_string(status));
        return status == ERROR_ACCESS_DENIED ? HttpBasicOverHttpStatus::AccessDenied : HttpBasicOverHttpStatus::Failed;
    }

    DWORD serverNotFoundCacheLifetime = 60;
    type = 0;
    size = sizeof(serverNotFoundCacheLifetime);
    status = RegQueryValueExW(queryKey,
                              kServerNotFoundCacheLifetimeValue,
                              nullptr,
                              &type,
                              reinterpret_cast<LPBYTE>(&serverNotFoundCacheLifetime),
                              &size);
    RegCloseKey(queryKey);
    if (status != ERROR_SUCCESS || type != REG_DWORD) {
        Log::Warn("RegQueryValueExW(ServerNotFoundCacheLifeTimeInSec) failed with error " + std::to_string(status));
        return status == ERROR_ACCESS_DENIED ? HttpBasicOverHttpStatus::AccessDenied : HttpBasicOverHttpStatus::Failed;
    }

    if (basicAuthLevel >= 2 && serverNotFoundCacheLifetime == 0) {
        Log::Info("WebClient registry already allows HTTP WebDAV authentication and disables negative server cache");
        return HttpBasicOverHttpStatus::Enabled;
    }

    HKEY updateKey = nullptr;
    status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                           kWebClientParametersKey,
                           0,
                           KEY_QUERY_VALUE | KEY_SET_VALUE,
                           &updateKey);
    if (status != ERROR_SUCCESS) {
        Log::Warn("RegOpenKeyExW(WebClient Parameters, update) failed with error " + std::to_string(status));
        return MapRegistryAccessError(status);
    }

    basicAuthLevel = 2;
    status = RegSetValueExW(updateKey,
                            kBasicAuthLevelValue,
                            0,
                            REG_DWORD,
                            reinterpret_cast<const BYTE *>(&basicAuthLevel),
                            sizeof(basicAuthLevel));
    if (status != ERROR_SUCCESS) {
        RegCloseKey(updateKey);
        Log::Warn("RegSetValueExW(BasicAuthLevel=2) failed with error " + std::to_string(status));
        return MapRegistryAccessError(status);
    }

    serverNotFoundCacheLifetime = 0;
    status = RegSetValueExW(updateKey,
                            kServerNotFoundCacheLifetimeValue,
                            0,
                            REG_DWORD,
                            reinterpret_cast<const BYTE *>(&serverNotFoundCacheLifetime),
                            sizeof(serverNotFoundCacheLifetime));
    RegCloseKey(updateKey);
    if (status != ERROR_SUCCESS) {
        Log::Warn("RegSetValueExW(ServerNotFoundCacheLifeTimeInSec=0) failed with error " + std::to_string(status));
        return MapRegistryAccessError(status);
    }

    Log::Info("Updated WebClient registry for HTTP WebDAV authentication and disabled negative server cache");
    if (!RestartWebClientService()) {
        Log::Warn("WebClient registry was updated but WebClient could not be restarted automatically");
        return HttpBasicOverHttpStatus::AccessDenied;
    }

    Log::Info("WebClient registry was updated and WebClient was restarted");
    return HttpBasicOverHttpStatus::Updated;
}
