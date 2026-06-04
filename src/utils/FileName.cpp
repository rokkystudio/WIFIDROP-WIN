#include "FileName.h"

#include "Utf.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace {

bool IsHexDigit(char symbol) {
    return std::isxdigit(static_cast<unsigned char>(symbol)) != 0;
}

bool LooksLikeTraversal(const std::string &value) {
    if (value.find('/') != std::string::npos || value.find('\\') != std::string::npos || value.find(':') != std::string::npos) {
        return true;
    }

    return value == "." || value == "..";
}

std::string SanitizeFileName(const std::string &value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char symbol : value) {
        if (symbol < 32) {
            continue;
        }
        switch (symbol) {
            case '<':
            case '>':
            case ':':
            case '"':
            case '/':
            case '\\':
            case '|':
            case '?':
            case '*':
                continue;
            default:
                result.push_back(static_cast<char>(symbol));
                break;
        }
    }

    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }

    while (!result.empty() && result.front() == ' ') {
        result.erase(result.begin());
    }

    return result;
}

}  // namespace

std::string FileName::UrlDecode(const std::string &value) {
    std::string decoded;
    decoded.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2 < value.size() && IsHexDigit(value[index + 1]) && IsHexDigit(value[index + 2])) {
            const std::string hexValue = value.substr(index + 1, 2);
            decoded.push_back(static_cast<char>(std::stoi(hexValue, nullptr, 16)));
            index += 2;
            continue;
        }

        if (value[index] == '+') {
            decoded.push_back(' ');
            continue;
        }

        decoded.push_back(value[index]);
    }

    return decoded;
}

UploadFileNameValidationResult FileName::ValidateUploadFileName(const std::string &decodedName) {
    if (decodedName.empty()) {
        return {.ok = false, .sanitizedName = "", .error = "Empty file name"};
    }

    if (LooksLikeTraversal(decodedName)) {
        return {.ok = false, .sanitizedName = "", .error = "Path traversal is not allowed"};
    }

    const auto sanitized = SanitizeFileName(decodedName);
    if (sanitized.empty()) {
        return {.ok = false, .sanitizedName = "", .error = "File name is empty after sanitization"};
    }

    const std::filesystem::path path(Utf::Utf8ToWide(sanitized));
    if (path.has_parent_path() || path.has_root_name() || path.has_root_directory()) {
        return {.ok = false, .sanitizedName = "", .error = "Path traversal is not allowed"};
    }

    return {.ok = true, .sanitizedName = sanitized, .error = ""};
}

std::filesystem::path FileName::MakeUniquePath(const std::filesystem::path &directory, const std::string &sanitizedFileName) {
    const std::filesystem::path originalName = Utf::Utf8ToWide(sanitizedFileName);
    const std::wstring stem = originalName.stem().native();
    const std::wstring extension = originalName.extension().native();

    std::filesystem::path candidate = directory / originalName;
    for (int index = 1; std::filesystem::exists(candidate); ++index) {
        std::wstringstream nameStream;
        nameStream << stem << L" (" << index << L")" << extension;
        candidate = directory / std::filesystem::path(nameStream.str());
    }

    return candidate;
}
