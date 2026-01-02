#pragma once
#include "Core/include/IFileSystem.h"

namespace Drama::Platform::IO
{
    class WinFileSystem final : public Drama::Core::IO::IFileSystem
    {
    public:
        Drama::Core::IO::FsResult Exists(std::string_view path) noexcept override;
        Drama::Core::IO::FsResult CreateDirectories(std::string_view path) noexcept override;

        /// @brief 上書き
        Drama::Core::IO::FsResult WriteAllBytes(std::string_view path, const void* data, size_t size) noexcept override;
        /// @brief 読み取り
        Drama::Core::IO::FsResult ReadAllBytes(std::string_view path, std::vector<uint8_t>& out) noexcept override;
        /// @brief 追記
        Drama::Core::IO::FsResult AppendAllBytes(std::string_view path, const void* data, size_t size) noexcept override;
        /// @brief 安全な保存
        Drama::Core::IO::FsResult WriteAllBytesAtomic(std::string_view path, const void* data, size_t size) noexcept override;

        std::string currentPath() noexcept override;
    };
}
