#include "App.h"

#include <windows.h>

namespace {

int RunWifiDrop(HINSTANCE instanceHandle, int showCommand) {
    App app;
    return app.Run(instanceHandle, showCommand);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instanceHandle, HINSTANCE, PWSTR, int showCommand) {
    return RunWifiDrop(instanceHandle, showCommand);
}

int main() {
    return RunWifiDrop(GetModuleHandleW(nullptr), SW_HIDE);
}
