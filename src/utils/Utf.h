#pragma once

#include <string>

/// Выполняет преобразования между UTF-8, UTF-16 и строками JSON.
class Utf {
public:
    /// Преобразует UTF-8 строку в wide string для WinAPI.
    static std::wstring Utf8ToWide(const std::string &value);

    /// Преобразует wide string WinAPI в UTF-8.
    static std::string WideToUtf8(const std::wstring &value);

    /// Декодирует escape-последовательности JSON в UTF-8 строку.
    static std::string JsonUnescape(const std::string &value);
};
