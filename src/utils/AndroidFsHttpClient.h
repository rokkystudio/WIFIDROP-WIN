#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * Выполняет HTTP-запросы к файловому API Android-клиента.
 */
class AndroidFsHttpClient {
public:
    struct NodeMetadata {
        bool exists{false};
        bool directory{false};
        std::uint64_t size{0};
        std::uint64_t lastModified{0};
        bool writable{false};
        std::string name;
        std::string etag;
    };

    struct DirectoryEntry {
        std::string name;
        bool directory{false};
        std::uint64_t size{0};
        std::uint64_t lastModified{0};
        bool writable{false};
    };

    AndroidFsHttpClient(std::string host, int port);

    bool GetMetadata(const std::string &path, NodeMetadata &metadata, std::string *errorMessage) const;
    bool ListDirectory(const std::string &path, std::vector<DirectoryEntry> &entries, std::string *errorMessage) const;
    bool ReadFileRange(const std::string &path,
                       std::uint64_t offset,
                       std::uint32_t size,
                       std::vector<char> &bytes,
                       std::string *errorMessage) const;
    bool UploadFile(const std::string &path, const std::wstring &localFilePath, std::string *errorMessage) const;
    bool DeletePath(const std::string &path, std::string *errorMessage) const;
    /**
     * Создает каталог на удаленной файловой системе через MKCOL-запрос.
     *
     * Возвращает true при HTTP 201. При ошибке транспорта или неожиданном HTTP-статусе
     * возвращает false и записывает описание в errorMessage, если указатель не равен nullptr.
     */
    bool CreateRemoteDirectory(const std::string &path, std::string *errorMessage) const;
    bool MovePath(const std::string &sourcePath,
                  const std::string &destinationPath,
                  bool overwrite,
                  std::string *errorMessage) const;

private:
    struct HttpResponse {
        std::uint32_t statusCode{0};
        std::vector<char> body;
    };

    bool SendRequest(const std::wstring &method,
                     const std::wstring &pathAndQuery,
                     const std::vector<std::wstring> &headers,
                     HttpResponse &response,
                     std::string *errorMessage) const;
    bool SendFileRequest(const std::wstring &method,
                         const std::wstring &pathAndQuery,
                         const std::vector<std::wstring> &headers,
                         const std::wstring &localFilePath,
                         HttpResponse &response,
                         std::string *errorMessage) const;

    static std::wstring BuildMetaPath(const std::string &path);
    static std::wstring BuildListPath(const std::string &path);
    static std::wstring BuildFilePath(const std::string &path);
    std::wstring BuildDestinationHeaderValue(const std::string &path) const;
    static std::wstring UrlEncode(const std::string &value);
    static std::wstring UrlEncodePath(const std::string &value);
    static std::string UrlDecode(const std::string &value);
    static std::string NarrowBody(const std::vector<char> &body);
    static bool ParseMetadataBody(const std::string &body, NodeMetadata &metadata);
    static bool ParseListBody(const std::string &body, std::vector<DirectoryEntry> &entries);
    static bool ParseBool(std::string_view value);
    static std::uint64_t ParseUnsigned(std::string_view value);

    std::wstring host_;
    int port_{0};
};
