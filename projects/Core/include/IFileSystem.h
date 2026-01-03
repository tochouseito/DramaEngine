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

    /// @brief ファイルシステム操作の結果
    struct FsResult
    {
        FsError error = FsError::Ok;
        std::error_code ec{};
        std::string message{};

        constexpr explicit operator bool() const noexcept { return error == FsError::Ok; }
        static FsResult Ok() noexcept { return {}; }
    };

    /// @brief ファイルシステム抽象インターフェース
    struct IFileSystem
    {
        virtual ~IFileSystem() = default;

        /// @brief パスが存在するか確認する
        virtual FsResult exists(std::string_view path) noexcept = 0;
        /// @brief ディレクトリを再帰的に作成する
        virtual FsResult create_directories(std::string_view path) noexcept = 0;

        /// @brief バイト列を上書き保存する
        virtual FsResult write_all_bytes(std::string_view path, const void* data, size_t size) noexcept = 0;
        /// @brief バイト列を全読み込みする
        virtual FsResult read_all_bytes(std::string_view path, std::vector<uint8_t>& out) noexcept = 0;
        /// @brief バイト列を追記する
        virtual FsResult append_all_bytes(std::string_view path, const void* data, size_t size) noexcept = 0;
        /// @brief 一時ファイル経由で安全に上書き保存する
        virtual FsResult write_all_bytes_atomic(std::string_view path, const void* data, size_t size) noexcept = 0;

        /// @brief カレントディレクトリを取得する
        virtual std::string current_path() noexcept = 0;
    };
}
