#pragma once

#include "Core/Error/Result.h"
#include "Core/IO/public/IFileSystem.h"

// ConfigFormat
#include "Engine/config/EngineConfig.h"
#include "GraphicsCore/public/GraphicsConfig.h"

namespace Drama::Core::IO
{
    class Exporter final
    {
        using Result = Core::Error::Result;
    public:
        explicit Exporter(IFileSystem& fs) noexcept
            : m_fs(fs)
        {
        }
        ~Exporter() = default;
        [[nodiscard]] Result export_engine_config(const std::string_view& path, const Drama::EngineConfig& config) noexcept;
        [[nodiscard]] Result export_graphics_config(const std::string_view& path, const Drama::Graphics::GraphicsConfig& config) noexcept;
    private:
        IFileSystem& m_fs;
    };
}

