#pragma once
#include "Core/include/IFileSystem.h"

namespace Drama::Platform::IO
{
    class WinFileSystem final : public Drama::Core::IO::IFileSystem
    {
    public:
        Drama::Core::IO::FsResult exists(std::string_view path) noexcept override;
        Drama::Core::IO::FsResult create_directories(std::string_view path) noexcept override;

        /// @brief バイト列を上書き保存する
        Drama::Core::IO::FsResult write_all_bytes(std::string_view path, const void* data, size_t size) noexcept override;
        /// @brief バイト列を全読み込みする
        Drama::Core::IO::FsResult read_all_bytes(std::string_view path, std::vector<uint8_t>& out) noexcept override;
        /// @brief バイト列を追記する
        Drama::Core::IO::FsResult append_all_bytes(std::string_view path, const void* data, size_t size) noexcept override;
        /// @brief 一時ファイル経由で安全に上書き保存する
        Drama::Core::IO::FsResult write_all_bytes_atomic(std::string_view path, const void* data, size_t size) noexcept override;

        std::string current_path() noexcept override;
    };
}
