#include "pch.h"
#include "IO/public/Exporter.h"

// Drama Engine includes

// External includes
#include <nlohmann/json.hpp>

namespace
{
    using namespace Drama::Core::Error;
    using Json = nlohmann::ordered_json;
    using Result = Drama::Core::Error::Result;
    using EngineConfig = Drama::EngineConfig;
    using GraphicsConfig = Drama::Graphics::GraphicsConfig;

    Json serialize_engine_config(const EngineConfig& config) noexcept
    {
        // 1) ルートオブジェクトを作成する
        Json root;

        // 2) payload オブジェクトを作成する
        Json payload;
        payload["bufferingCount"] = config.m_bufferingCount;
        payload["enableDebugLayer"] = config.m_enableDebugLayer;

        root["payload"] = payload;
        return root;
    }
}

namespace Drama::Core::IO
{
    Result Exporter::export_engine_config(const std::string_view& path, const Drama::EngineConfig& config) noexcept
    {
        // 1) シリアライズする
        const Json root = serialize_engine_config(config);
        const std::string text = root.dump(4) + "\n";// 読みやすく

        // 2) ファイルを書き込む
        Result result = m_fs.write_all_bytes(path, text.data(), text.size());
        return result;
    }
    Result Exporter::export_graphics_config(const std::string_view& path, const Drama::Graphics::GraphicsConfig& config) noexcept
    {
        path; config;
        return Result();
    }
}
