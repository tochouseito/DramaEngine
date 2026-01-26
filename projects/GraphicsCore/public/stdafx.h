#pragma once
// C++ standard library includes
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

// DirectX includes
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#ifndef NDEBUG
#include <dxgidebug.h>
#include <d3d12sdklayers.h>
#endif

// Microsoft WRL includes
#include <wrl.h>

#include "ExportsMacro.h"

// Drama Engine includes
#include "Core/Error/Result.h"

#ifndef D3D12_GPU_VIRTUAL_ADDRESS_NULL
#define D3D12_GPU_VIRTUAL_ADDRESS_NULL ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#endif

namespace Drama::Graphics::DX12
{
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    using Result = Core::Error::Result;

    static const inline std::wstring engineName = L"Drama";

    inline void SetDXGIName([[maybe_unused]] IDXGIObject* obj, [[maybe_unused]] const wchar_t* name)
    {
#ifndef NDEBUG
        if (obj)
        {
            std::wstring fullName = engineName + L"." + name;
            obj->SetPrivateData(
                WKPDID_D3DDebugObjectName,
                static_cast<UINT>((wcslen(fullName.c_str()) + 1) * sizeof(wchar_t)),
                fullName.c_str());
        }
#endif
    }

    inline void SetD3D12Name([[maybe_unused]] ID3D12Object* obj, [[maybe_unused]] const wchar_t* name)
    {
#ifndef NDEBUG
        if (obj)
        {
            std::wstring fullName = engineName + L"." + name;
            obj->SetName(fullName.c_str());
        }
#endif
    }

    std::string to_utf8(std::wstring_view w) noexcept;
    std::wstring to_utf16(std::string_view s) noexcept;
}
