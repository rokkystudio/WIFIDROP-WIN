#pragma once

#include "clients/AndroidClient.h"

#include <optional>
#include <string>

class WinFspDriveHost {
public:
    static std::optional<std::string> Mount(const AndroidClient &client, std::string *errorMessage);
    static void Unmount(const AndroidClient &client);
};
