#pragma once

#include <memory>

#include "Core/Error/Result.h"
#include "Core/Threading/IThread.h"

namespace Drama::Core::Threading
{
    class IThreadFactory
    {
    public:
        virtual ~IThreadFactory() = default;

        virtual Core::Error::Result create_thread(
            const ThreadDesc& desc,
            ThreadProc proc,
            void* user,
            std::unique_ptr<IThread>& outThread) noexcept = 0;
    };
}
