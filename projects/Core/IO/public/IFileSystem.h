#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "Core/Error/Result.h"

namespace Drama::Core::IO
{

    /// @brief ファイルシステム抽象インターフェース
    struct IFileSystem
    {
        virtual ~IFileSystem() = default;

        /// @brief パスが存在するか確認する
        virtual Error::Result exists(std::string_view path) noexcept = 0;
        /// @brief ディレクトリを再帰的に作成する
        virtual Error::Result create_directories(std::string_view path) noexcept = 0;

        /// @brief バイト列を上書き保存する
        virtual Error::Result write_all_bytes(std::string_view path, const void* data, size_t size) noexcept = 0;
        /// @brief バイト列を全読み込みする
        virtual Error::Result read_all_bytes(std::string_view path, std::vector<uint8_t>& out) noexcept = 0;
        /// @brief バイト列を追記する
        virtual Error::Result append_all_bytes(std::string_view path, const void* data, size_t size) noexcept = 0;
        /// @brief 一時ファイル経由で安全に上書き保存する
        virtual Error::Result write_all_bytes_atomic(std::string_view path, const void* data, size_t size) noexcept = 0;
        /// @brief カレントディレクトリを取得する
        virtual std::string current_path() noexcept = 0;
    };
}
