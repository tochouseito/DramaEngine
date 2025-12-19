#include "pch.h"
#include "platform/include/WinApp.h"

#include <cstdint>
#include <timeapi.h>
#include <shellapi.h>
#include <ole2.h>
#pragma comment(lib,"winmm.lib")

HWND Drama::Platform::WinApp::m_HWND = nullptr;
WNDCLASS Drama::Platform::WinApp::m_WC = {};
UINT64 Drama::Platform::WinApp::m_WindowWidth = 1920;
UINT Drama::Platform::WinApp::m_WindowHeight = 1080;

LRESULT Drama::Platform::WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // ウィンドウのメッセージ処理
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    // 標準のメッセージ処理を行う
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

bool Drama::Platform::WinApp::CreateWindowApp()
{
    // ウィンドウプロシージャ
    m_WC.lpfnWndProc = WindowProc;
    // ウィンドウクラス名
    m_WC.lpszClassName = L"DramaWindowClass";
    // インスタンスハンドル
    m_WC.hInstance = GetModuleHandle(nullptr);
    // カーソル
    m_WC.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // アイコン
    // m_WC.hIcon = LoadIcon(m_WC.hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    // ウィンドウクラスを登録する
    RegisterClass(&m_WC);
    // ウィンドウサイズを表す構造体にクライアント領域を入れる
    RECT wrc = { 0,0,
        static_cast<LONG>(m_WindowWidth),
        static_cast<LONG>(m_WindowHeight) };
    // クライアント領域を元に実際のサイズにwrcを変更してもらう
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
    // ウィンドウの生成
    m_HWND = CreateWindow(
        m_WC.lpszClassName,     // 利用するクラス名
        L"Drama",               // タイトルバーの文字
        WS_OVERLAPPEDWINDOW,    // よく見るウィンドウスタイル
        CW_USEDEFAULT,          // ウィンドウ横位置
        CW_USEDEFAULT,          // ウィンドウ縦位置
        wrc.right - wrc.left,   // ウィンドウ幅
        wrc.bottom - wrc.top,   // ウィンドウ高
        nullptr,                // 親ウィンドウハンドル
        nullptr,                // メニューハンドル
        m_WC.hInstance,         // インスタンスハンドル
        nullptr);               // オプション
    // システムタイマーの分解能を上げる
    timeBeginPeriod(1);
    return true;
}

void Drama::Platform::WinApp::ShowWindowApp()
{
    // ウィンドウを最大化して表示
    ShowWindow(m_HWND, SW_MAXIMIZE);
    // ドラッグ＆ドロップを有効化
    DragAcceptFiles(m_HWND, TRUE);
}

void Drama::Platform::WinApp::TerminateWindow()
{
    // ウィンドウの破棄
    DestroyWindow(m_HWND);
    // ウィンドウクラスの登録解除
    UnregisterClass(m_WC.lpszClassName, m_WC.hInstance);
    // システムタイマーの分解能を元に戻す
    timeEndPeriod(1);
}

bool Drama::Platform::WinApp::ProcessMessage()
{
    MSG msg{};
    // メッセージループ
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            return false; // 終了メッセージが来たらfalseを返す
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}
