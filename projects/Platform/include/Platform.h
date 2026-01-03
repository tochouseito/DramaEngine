#pragma once
//====================================
// C++ standard library includes
#include <cstdint>
#include <memory>
#include <string>
//====================================

namespace Drama::Platform
{
    class Windows final
    {
    public:
        Windows();
        ~Windows();

        /// @brief ウィンドウ作成
        /// @return 成功ならtrue、失敗ならfalse
        [[nodiscard]] bool create(uint32_t w = 1280, uint32_t h = 720);
        void show(bool isMaximized = false);
        void shutdown();

        /// @brief ウィンドウメッセージ処理
        /// @return 終了ならtrue、継続ならfalse
        [[nodiscard]] bool pump_messages();

        uint32_t width() const noexcept;
        uint32_t height() const noexcept;
        void* native_handle() const noexcept;

        static std::string to_utf8(const std::wstring& utf16Str);
        static std::wstring to_utf16(const std::string& utf8Str);
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

    [[nodiscard]] bool init();
    void update();
    void shutdown();
}
