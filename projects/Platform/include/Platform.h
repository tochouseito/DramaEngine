#pragma once
//====================================
// C++ standard library includes
#include <cstdint>
#include <memory>
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
        bool Create(uint32_t w = 1280, uint32_t h = 720);
        void Show(bool isMaxSize = false);
        void Shutdown();

        /// @brief ウィンドウメッセージ処理
        /// @return 終了ならtrue、継続ならfalse
        [[nodiscard]] bool PumpMessages();

        uint32_t Width() const noexcept;
        uint32_t Height() const noexcept;
        void* NativeHandle() const noexcept;
    private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
    };

    [[nodiscard]] bool Init();
    void Update();
    void Shutdown();
}
