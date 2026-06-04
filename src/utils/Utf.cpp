#include "Utf.h"

#include <windows.h>

#include <stdexcept>

std::wstring Utf::Utf8ToWide(const std::string &value) {
    if (value.empty()) {
        return L"";
    }

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        throw std::runtime_error("MultiByteToWideChar failed");
    }

    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string Utf::WideToUtf8(const std::wstring &value) {
    if (value.empty()) {
        return "";
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        throw std::runtime_error("WideCharToMultiByte failed");
    }

    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::string Utf::JsonUnescape(const std::string &value) {
    std::string result;
    result.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '\\' || index + 1 >= value.size()) {
            result.push_back(value[index]);
            continue;
        }

        const char escape = value[++index];
        switch (escape) {
            case '"':
                result.push_back('"');
                break;
            case '\\':
                result.push_back('\\');
                break;
            case '/':
                result.push_back('/');
                break;
            case 'b':
                result.push_back('\b');
                break;
            case 'f':
                result.push_back('\f');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            case 'u':
                if (index + 4 >= value.size()) {
                    throw std::runtime_error("Invalid JSON unicode escape");
                }

                {
                    const std::string hexValue = value.substr(index + 1, 4);
                    const unsigned int codePoint = std::stoul(hexValue, nullptr, 16);
                    wchar_t wideCharacter = static_cast<wchar_t>(codePoint);
                    result += WideToUtf8(std::wstring(1, wideCharacter));
                    index += 4;
                }
                break;
            default:
                result.push_back(escape);
                break;
        }
    }

    return result;
}
