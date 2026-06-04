#include "AndroidClient.h"

#include "utils/Utf.h"

std::wstring AndroidClient::BuildDriveName(const std::string &deviceNameUtf8, const std::string &deviceNumberUtf8) {
    const std::wstring deviceName = deviceNameUtf8.empty() ? L"Unknown Android" : Utf::Utf8ToWide(deviceNameUtf8);
    const std::wstring deviceNumber = deviceNumberUtf8.empty() ? L"UNKNOWN" : Utf::Utf8ToWide(deviceNumberUtf8);
    return deviceName + L" #" + deviceNumber;
}
