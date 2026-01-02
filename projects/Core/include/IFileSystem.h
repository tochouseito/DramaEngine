#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <system_error>

namespace Drama::Core::IO
{
    enum class FsError : uint32_t
    {
        Ok = 0,
        NotFound,
        AccessDenied,
        AlreadyExists,
        InvalidPath,
        IoError,
        Unknown,
    };

    struct FsResult
    {
        FsError error = FsError::Ok;
        std::error_code ec{};
        std::string message{};

        constexpr explicit operator bool() const noexcept { return error == FsError::Ok; }
        static FsResult Ok() noexcept { return {}; }
    };

    struct IFileSystem
    {
        virtual ~IFileSystem() = default;

        virtual FsResult Exists(std::string_view path) noexcept = 0;
        virtual FsResult CreateDirectories(std::string_view path) noexcept = 0;

        /// @brief 上書き
        virtual FsResult WriteAllBytes(std::string_view path, const void* data, size_t size) noexcept = 0;
        /// @brief 読み取り
        virtual FsResult ReadAllBytes(std::string_view path, std::vector<uint8_t>& out) noexcept = 0;
        /// @brief 追記
        virtual FsResult AppendAllBytes(std::string_view path, const void* data, size_t size) noexcept = 0;
        /// @brief 安全な保存
        virtual FsResult WriteAllBytesAtomic(std::string_view path, const void* data, size_t size) noexcept = 0;

        virtual std::string currentPath() noexcept = 0;
    };
}
