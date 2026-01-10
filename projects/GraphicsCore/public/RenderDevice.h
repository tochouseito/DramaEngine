#pragma once
// DirectX includes
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>

// Microsoft WRL includes
#include <wrl.h>

// Drama includes

// C++ standard library includes
#include <memory>
#include <array>

namespace Drama::Graphics::DX12
{
    template<typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

}

