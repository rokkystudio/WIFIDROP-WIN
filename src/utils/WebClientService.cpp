#include "WebClientService.h"

#include "Log.h"

#include <windows.h>

#include <winsvc.h>

#include <string>

namespace {

constexpr wchar_t kWebClientServiceName[] = L"WebClient";
constexpr DWORD kStartTimeoutMs = 15000;
constexpr DWORD kPollIntervalMs = 250;

bool QueryRunningState(SC_HANDLE serviceHandle, SERVICE_STATUS_PROCESS &status) {
    DWORD bytesNeeded = 0;
    if (!QueryServiceStatusEx(serviceHandle,
                              SC_STATUS_PROCESS_INFO,
                              reinterpret_cast<LPBYTE>(&status),
                              sizeof(status),
                              &bytesNeeded)) {
        Log::Warn("QueryServiceStatusEx(WebClient) failed");
        return false;
    }
    return true;
}

bool WaitUntilRunning(SC_HANDLE serviceHandle) {
    const DWORD startTick = GetTickCount();
    SERVICE_STATUS_PROCESS status{};
    while (GetTickCount() - startTick < kStartTimeoutMs) {
        if (!QueryRunningState(serviceHandle, status)) {
            return false;
        }

        if (status.dwCurrentState == SERVICE_RUNNING) {
            return true;
        }

        if (status.dwCurrentState != SERVICE_START_PENDING) {
            return false;
        }

        Sleep(kPollIntervalMs);
    }

    Log::Warn("Timed out while waiting for WebClient service to start");
    return false;
}

}  // namespace

bool WebClientService::EnsureRunning() {
    SC_HANDLE scmHandle = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scmHandle) {
        Log::Warn("OpenSCManagerW failed for WebClient");
        return false;
    }

    SC_HANDLE serviceHandle = OpenServiceW(scmHandle,
                                           kWebClientServiceName,
                                           SERVICE_QUERY_STATUS | SERVICE_START);
    if (!serviceHandle) {
        Log::Warn("OpenServiceW(WebClient) failed");
        CloseServiceHandle(scmHandle);
        return false;
    }

    SERVICE_STATUS_PROCESS status{};
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
