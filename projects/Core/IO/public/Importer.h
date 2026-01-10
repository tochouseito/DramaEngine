#pragma once
#include "Core/Error/Result.h"
#include "Core/IO/public/IFileSystem.h"

// ConfigFormat
#include "Engine/config/EngineConfig.h"
#include "GraphicsCore/public/GraphicsConfig.h"

namespace Drama::Core::IO
{
    class Importer final
    {
        using Result = Core::Error::Result;
    public:
        explicit Importer(IFileSystem& fs) noexcept
            : m_fs(fs)
        {
        }
        ~Importer() = default;

        [[nodiscard]] Result import_engine_config(const std::string_view& path, Drama::EngineConfig& outConfig) noexcept;
        [[nodiscard]] Result import_graphics_config(const std::string_view& path, Drama::Graphics::GraphicsConfig& outConfig) noexcept;
    private:
        IFileSystem& m_fs;
    };
}
