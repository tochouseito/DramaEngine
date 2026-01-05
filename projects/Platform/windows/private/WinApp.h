#pragma once
//====================================
// C++ standard library includes
#include <cstdint>
#include <memory>
//====================================

namespace Drama::Platform::Win
{
    class WinApp final
    {
    public:
        WinApp();
        ~WinApp();
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
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
