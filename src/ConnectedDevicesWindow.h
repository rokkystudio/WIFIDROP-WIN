#pragma once

#include <windows.h>

#include <string>

/// Управляет окном со списком подключенных Android-устройств.
class ConnectedDevicesWindow {
public:
    /// Создает скрытое окно и дочернее поле просмотра списка устройств.
    bool Initialize(HINSTANCE instanceHandle);

    /// Уничтожает окно списка устройств и освобождает связанные WinAPI-ресурсы.
    void Shutdown();

    /// Показывает окно списка устройств и выводит переданный текст состояния.
    void Show(const std::wstring &content);

private:
    /// Обрабатывает сообщения окна списка устройств.
    LRESULT WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

    /// Создает класс окна и скрытый window handle.
    bool CreateWindowClassAndHandle();

    /// Создает read-only control для отображения списка устройств.
    bool CreateContentControl(HWND parentHandle);

    static LRESULT CALLBACK StaticWindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

    HINSTANCE instanceHandle_{nullptr};
    HWND windowHandle_{nullptr};
    HWND contentHandle_{nullptr};
};
