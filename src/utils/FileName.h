#pragma once

#include <filesystem>
#include <string>

/// Содержит результат проверки имени файла для upload endpoint.
struct UploadFileNameValidationResult {
    bool ok{false};
    std::string sanitizedName;
    std::string error;
};

/// Выполняет декодирование и валидацию имён файлов, приходящих по HTTP upload.
class FileName {
public:
    /// Декодирует urlencoded строку в UTF-8 представление имени файла.
    static std::string UrlDecode(const std::string &value);

    /// Проверяет имя файла на path traversal и недопустимые символы Windows.
    static UploadFileNameValidationResult ValidateUploadFileName(const std::string &decodedName);

    /// Подбирает путь без перезаписи существующего файла по правилу "name (n).ext".
    static std::filesystem::path MakeUniquePath(const std::filesystem::path &directory, const std::string &sanitizedFileName);
};
