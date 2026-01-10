#include "pch.h"
#include "IO/public/Importer.h"

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

    // バイト列→文字列（UTF-8を想定）
    std::string bytes_to_string(const std::vector<uint8_t>& bytes)
    {
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    bool parse_engine_config(const Json& root, EngineConfig& out) noexcept
    {
        // 1) ルート検証
        if (!root.is_object())
        {
            return false;
        }

        const auto itPayload = root.find("payload");
        if (itPayload == root.end() || !itPayload->is_object())
        {
            return false;
        }

        const Json& p = *itPayload;

        // 2) 探して復元する

        // bufferingCount
        {
            const auto it = p.find("bufferingCount");
            if (it == p.end() || !it->is_number_integer())
            {
                return false;
            }
            out.bufferingCount = it->get<uint32_t>();
        }
        
        // enableDebugLayer
        {
            const auto it = p.find("enableDebugLayer");
            if (it == p.end() || !it->is_boolean())
            {
                return false;
            }
            out.enableDebugLayer = it->get<bool>();
        }

        //// 2) id
        //{
        //    const auto it = p.find("id");
        //    if (it == p.end() || !it->is_number_integer())
        //    {
        //        return false;
        //    }
        //    out.id = it->get<int32_t>();
        //}

        //// 3) name
        //{
        //    const auto it = p.find("name");
        //    if (it == p.end() || !it->is_string())
        //    {
        //        return false;
        //    }
        //    out.name = it->get<std::string>();
        //}

        //// 4) pos
        //{
        //    const auto it = p.find("pos");
        //    if (it == p.end() || !it->is_array() || it->size() != 3)
        //    {
        //        return false;
        //    }

        //    for (size_t i = 0; i < 3; ++i)
        //    {
        //        if ((*it)[i].is_number_float())
        //        {
        //            out.pos[i] = (*it)[i].get<float>();
        //        }
        //        else if ((*it)[i].is_number_integer())
        //        {
        //            out.pos[i] = static_cast<float>((*it)[i].get<int32_t>());
        //        }
        //        else
        //        {
        //            return false;
        //        }
        //    }
        //}

        //// 5) inventory
        //{
        //    const auto it = p.find("inventory");
        //    if (it == p.end() || !it->is_array())
        //    {
        //        return false;
        //    }

        //    out.inventory.clear();
        //    out.inventory.reserve(it->size());

        //    for (const auto& e : *it)
        //    {
        //        if (!e.is_number_integer())
        //        {
        //            return false;
        //        }
        //        out.inventory.push_back(e.get<int32_t>());
        //    }
        //}

        //// 6) enabled
        //{
        //    const auto it = p.find("enabled");
        //    if (it == p.end() || !it->is_boolean())
        //    {
        //        return false;
        //    }
        //    out.enabled = it->get<bool>();
        //}

        return true;
    }
}

namespace Drama::Core::IO
{
    Result Importer::import_engine_config(const std::string_view& path, Drama::EngineConfig& outConfig) noexcept
    {
        // 1) ファイルが存在するか確認する
        Result result = m_fs.exists(path);
        if (!result)
        {
            return result;
        }
        // 2) ファイルを全読み込みする
        std::vector<uint8_t> fileData;
        result = m_fs.read_all_bytes(path, fileData);
        if (!result)
        {
            return result;
        }

        // 3) JSONパースする
        Json parsed = Json::parse(fileData, nullptr, false);
        if(parsed.is_discarded())
        {
            return Result::fail(
                Facility::IO,
                Code::InvalidArg,
                Severity::Error,
                0,
                "Failed to parse engine config JSON.");
        }
        // 4) 復元
        EngineConfig config{};
        if (!parse_engine_config(parsed, config))
        {
            return Result::fail(
                Facility::IO,
                Code::InvalidArg,
                Severity::Error,
                0,
                "Failed to extract engine config from JSON.");
        }
        outConfig = config;

        return Result::ok();
    }
    Result Importer::import_graphics_config(const std::string_view& path, Drama::Graphics::GraphicsConfig& outConfig) noexcept
    {
        path; outConfig;
        return Result::ok();
    }
}
