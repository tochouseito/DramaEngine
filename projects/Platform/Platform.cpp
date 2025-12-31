#include "pch.h"
#include "Platform.h"

#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーからほとんど使用されていない部分を除外する
#define NOMINMAX                        // min と max マクロを無効にする

#include <Windows.h>
#include <wrl.h>
#include <timeapi.h>

#pragma comment(lib, "winmm.lib") // timeBeginPeriod, timeEndPeriod

namespace Drama::Platform
{
    struct Windows::Impl
    {
        HWND hwnd = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;

        LRESULT OnMessage(HWND _hwnd, UINT _msg, WPARAM _wp, LPARAM _lp)
        {
            switch (_msg)
            {
            case WM_SIZE:
                width = static_cast<uint32_t>(LOWORD(_lp));
                height = static_cast<uint32_t>(HIWORD(_lp));
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            }
            return DefWindowProcW(_hwnd, _msg, _wp, _lp);
        }

        static LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _msg, WPARAM _wp, LPARAM _lp)
        {
            Windows* self = nullptr;

            if (_msg == WM_NCCREATE)
            {
                auto cs = reinterpret_cast<CREATESTRUCTW*>(_lp);
                self = static_cast<Windows*>(cs->lpCreateParams);

                SetWindowLongPtrW(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
                self->pImpl->hwnd = _hwnd;

                // WM_NCCREATE は継続可否に影響するので、ここは素直に DefWindowProc を返す
                return DefWindowProcW(_hwnd, _msg, _wp, _lp);
            }

            self = reinterpret_cast<Windows*>(GetWindowLongPtrW(_hwnd, GWLP_USERDATA));
            if (self && self->pImpl)
            {
                return self->pImpl->OnMessage(_hwnd, _msg, _wp, _lp);
            }

            return DefWindowProcW(_hwnd, _msg, _wp, _lp);
        }
    };

    Windows::Windows() : pImpl(std::make_unique<Impl>())
    {
        // COM初期化
        [[maybe_unused]] HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        timeBeginPeriod(1);
    }

    Windows::~Windows()
    {
        if(pImpl && pImpl->hwnd)
        {
            Shutdown();
        }
        // COM終了処理
        CoUninitialize();
    }

    bool Windows::Create(uint32_t w, uint32_t h)
    {
        pImpl->width = w;
        pImpl->height = h;

        const wchar_t* kClassName = L"DramaWindowClass";
        HINSTANCE hInst = GetModuleHandleW(nullptr);

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Impl::WindowProc;
        wc.lpszClassName = kClassName;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

        if (!RegisterClassExW(&wc))
        {
            const DWORD err = GetLastError();
            if (err != ERROR_CLASS_ALREADY_EXISTS)
            {
                return false;
            }
        }

        RECT rc{ 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        HWND hwnd = CreateWindowExW(
            0,
            kClassName,
            L"Drama",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left,
            rc.bottom - rc.top,
            nullptr, nullptr,
            hInst,
            this // WM_NCCREATEで拾う
        );

        if (!hwnd)
        {
            return false;
        }

        pImpl->hwnd = hwnd;

        return true;
    }

    void Windows::Show(bool isMaxSize)
    {
        // ウィンドウを表示
        ShowWindow(pImpl->hwnd, isMaxSize ? SW_MAXIMIZE : SW_SHOW);
    }

    void Windows::Shutdown()
    {
        timeEndPeriod(1);

        if (pImpl && pImpl->hwnd)
        {
            DestroyWindow(pImpl->hwnd);
            pImpl->hwnd = nullptr;
        }
    }

    bool Windows::PumpMessages()
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

    uint32_t Windows::Width() const noexcept
    {
        return pImpl->width;
    }

    uint32_t Windows::Height() const noexcept
    {
        return pImpl->height;
    }

    void* Windows::NativeHandle() const noexcept
    {
        return reinterpret_cast<void*>(pImpl->hwnd);
    }

    bool Init()
    {
        return false;
    }

    void Update()
    {
    }

    void Shutdown()
    {
        
    }
}
