#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

/// Описывает элемент меню для подключенного Android-устройства в системном трее.
struct TrayDeviceMenuItem {
    std::string clientId;
    std::wstring displayName;
    bool canOpen{false};
};

/// Управляет скрытым окном сообщений WinAPI, иконкой в трее
/// и контекстным меню приложения.
class TrayIcon {
public:
    using ActionCallback = std::function<void()>;
    using QueryBoolCallback = std::function<bool()>;
    using QueryDevicesCallback = std::function<std::vector<TrayDeviceMenuItem>()>;
    using OpenDeviceCallback = std::function<void(const std::string &)>;

    /// Создаёт скрытое окно, регистрирует иконку в трее и сохраняет обработчики меню.
    bool Initialize(HINSTANCE instanceHandle,
                    const std::wstring &tooltip,
                    ActionCallback openIncomingFolder,
                    ActionCallback showConnectedDevices,
                    ActionCallback toggleAutostart,
                    ActionCallback exitApplication,
                    ActionCallback trayDoubleClick,
                    QueryBoolCallback isAutostartEnabled,
                    QueryDevicesCallback queryDevices,
                    OpenDeviceCallback openDevice);

    /// Удаляет иконку из трея и уничтожает скрытое окно.
    void Shutdown();

private:
    static constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    static constexpr UINT_PTR kTrayIconId = 1;
    static constexpr UINT kMenuOpenIncomingFolder = 1001;
    static constexpr UINT kMenuConnectedDevices = 1002;
    static constexpr UINT kMenuToggleAutostart = 1003;
    static constexpr UINT kMenuExit = 1004;
    static constexpr UINT kMenuDeviceBase = 2000;

    /// Обрабатывает сообщения скрытого окна.
    LRESULT WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

    /// Создаёт скрытое окно сообщений.
    bool CreateWindowClassAndHandle(HINSTANCE instanceHandle);

    /// Регистрирует иконку приложения в системном трее.
    bool AddNotifyIcon(const std::wstring &tooltip);

    /// Загружает иконки приложения и пунктов контекстного меню из ресурсов.
    bool LoadResources();

    /// Показывает контекстное меню у системного трея.
    void ShowContextMenu();

    /// Добавляет один текстовый пункт в контекстное меню.
    void AppendMenuItem(HMENU menuHandle, UINT commandId, const std::wstring &text, bool enabled, HBITMAP bitmapHandle) const;

    /// Создаёт bitmap меню из icon resource.
    HBITMAP CreateMenuBitmap(HICON iconHandle) const;

    /// Выполняет команду из контекстного меню.
    void HandleCommand(UINT commandId);

    static LRESULT CALLBACK StaticWindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

    HINSTANCE instanceHandle_{nullptr};
    HWND windowHandle_{nullptr};
    HICON trayIconHandle_{nullptr};
    HICON appIconHandle_{nullptr};
    HICON deviceMenuIconHandle_{nullptr};
    HICON exitMenuIconHandle_{nullptr};
    HBITMAP deviceMenuBitmapHandle_{nullptr};
    HBITMAP exitMenuBitmapHandle_{nullptr};
    ActionCallback openIncomingFolder_;
    ActionCallback showConnectedDevices_;
    ActionCallback toggleAutostart_;
    ActionCallback exitApplication_;
    ActionCallback trayDoubleClick_;
    QueryBoolCallback isAutostartEnabled_;
    QueryDevicesCallback queryDevices_;
    OpenDeviceCallback openDevice_;
    std::unordered_map<UINT, std::string> deviceCommands_;
};
