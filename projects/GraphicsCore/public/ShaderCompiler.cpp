#include "pch.h"
#include "ShaderCompiler.h"
#include "Core/IO/public/LogAssert.h"

namespace Drama::Graphics::DX12
{
    ShaderCompiler::ShaderCompiler()
    {
        HRESULT hr = S_OK;
        // DXC ユーティリティの生成
        hr = DxcCreateInstance(
            CLSID_DxcUtils,
            IID_PPV_ARGS(&m_dxcUtils)
        );
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to create DxcUtils instance.");
        }
        // DXC コンパイラの生成
        hr = DxcCreateInstance(
            CLSID_DxcCompiler,
            IID_PPV_ARGS(&m_dxcCompiler)
        );
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to create DxcCompiler instance.");
        }
        // インクルードハンドラの生成
        hr = m_dxcUtils->CreateDefaultIncludeHandler(&m_dxcIncludeHandler);
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to create DxcIncludeHandler instance.");
        }
    }
    ComPtr<IDxcBlob> ShaderCompiler::get_compile_shader(const ShaderCompileDesc& desc)
    {
        ComPtr<IDxcBlob> shaderBlob = compile_shader_raw(desc);
        return shaderBlob;
    }
    ComPtr<IDxcBlob> ShaderCompiler::compile_shader_raw(const ShaderCompileDesc& desc)
    {
        HRESULT hr = {};
        ComPtr<IDxcBlobEncoding> pSource = nullptr;
        hr = m_dxcUtils.Get()->LoadFile(desc.filePath.c_str(), nullptr, &pSource);
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to load shader file");
            // Core::IO::LogAssert::assert(false, "Failed to load shader file: " + std::string(Core::IO::Path::to_utf8(desc.filePath)));
        }
        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr = pSource->GetBufferPointer();
        sourceBuffer.Size = pSource->GetBufferSize();
        sourceBuffer.Encoding = DXC_CP_UTF8;
        LPCWSTR arguments[] = {
            desc.filePath.c_str(),              //コンパイル対象のhlslファイル名
            L"-E",desc.entryPoint.c_str(),                      // エントリーポイントの指定。基本的にmain以外にはしない
            L"-T",desc.targetProfile.c_str(),         // ShaderProfileの設定
            L"-Zi",L"-Qembed_debug",            // デバッグ用の情報を埋め込む
            L"-Od",                             // 最適化を外しておく
            L"-Zpr",                            // メモリレイアウトは行優先
        };

        std::vector<LPCWCH> args;
        args.push_back(desc.filePath.c_str());// コンパイル対象のhlslファイル名
        args.push_back(L"-E");
        args.push_back(desc.entryPoint.c_str());// エントリーポイントの指定。基本的にmain以外にはしない
        args.push_back(L"-T");
        args.push_back(desc.targetProfile.c_str());// ShaderProfileの設定
        args.push_back(L"-Zpr");// メモリレイアウトは行優先
        if (desc.enableDebugInfo)
        {
            args.push_back(L"-Zi");
            args.push_back(L"-Qembed_debug");// デバッグ情報埋め込み
            args.push_back(L"-Od");// デバッグビルドなら最適化外す
        }
        else
        {
            args.push_back(L"-O3");// リリースビルドなら最適化最大
        }

        ComPtr<IDxcResult> pResult = nullptr;
        hr = m_dxcCompiler.Get()->Compile(
            &sourceBuffer,			// 読み込んだファイル
            arguments,				// コンパイルオプション
            _countof(arguments),	// コンパイル結果
            m_dxcIncludeHandler.Get(),// includeが含まれた諸々
            IID_PPV_ARGS(&pResult)	// コンパイル結果
        );
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "DXC Compile Failed!!");
        }
        ComPtr<IDxcBlobUtf8> pErrors = nullptr;
        ComPtr<IDxcBlobUtf16> pErrorsUtf16;
        hr = pResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), &pErrorsUtf16);
        if (pErrors != nullptr && pErrors->GetStringLength() != 0)
        {
            Core::IO::LogAssert::log("DXC Compile Errors:\n{}", pErrors->GetStringPointer());
            Core::IO::LogAssert::assert(false, "Shader compilation failed with errors.");
        }
        IDxcBlob* pShader = nullptr;
        hr = pResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShader), &pErrorsUtf16);
        return pShader;
    }
    std::wstring shader_profile_to_wstring(D3D_SHADER_MODEL model)
    {
        switch (model)
        {
        case D3D_SHADER_MODEL_NONE:
            return L"Unknown Model";
            break;
        case D3D_SHADER_MODEL_5_1:
            return L"5_1";
            break;
        case D3D_SHADER_MODEL_6_0:
            return L"6_0";
            break;
        case D3D_SHADER_MODEL_6_1:
            return L"6_1";
            break;
        case D3D_SHADER_MODEL_6_2:
            return L"6_2";
            break;
        case D3D_SHADER_MODEL_6_3:
            return L"6_3";
            break;
        case D3D_SHADER_MODEL_6_4:
            return L"6_4";
            break;
        case D3D_SHADER_MODEL_6_5:
            return L"6_5";
            break;
        case D3D_SHADER_MODEL_6_6:
            return L"6_6";
            break;
        case D3D_SHADER_MODEL_6_7:
            return L"6_7";
            break;
        case D3D_SHADER_MODEL_6_8:
            return L"6_8";
            break;
        case D3D_SHADER_MODEL_6_9:
            return L"6_9";
            break;
        default:
            return L"Unknown Model";
            break;
        }
    }
}
