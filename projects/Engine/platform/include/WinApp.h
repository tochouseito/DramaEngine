#pragma once
// Windows
#include <Windows.h>

namespace Drama
{
    namespace Platform
    {
        class WinApp final
        {
        public:
            /// @brief ウィンドウプロシージャ
            static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

            /// @brief ウィンドウ作成
            /// @return 成功ならtrue、失敗ならfalse
            static bool CreateWindowApp();

            /// @brief ウィンドウ表示
            static void ShowWindowApp();

            /// @brief ウィンドウ破棄
            static void TerminateWindow();

            /// @brief ウィンドウメッセージ処理
            /// @return 終了ならfalse、継続ならtrue
            [[nodiscard]] static bool ProcessMessage();
        public:
            static UINT64       m_WindowWidth;  ///< ウィンドウ幅
            static UINT         m_WindowHeight; ///< ウィンドウ高さ
        private:
            static HWND         m_HWND; ///< ウィンドウハンドル
            static WNDCLASS     m_WC;   ///< ウィンドウクラス
        };
    }
}
