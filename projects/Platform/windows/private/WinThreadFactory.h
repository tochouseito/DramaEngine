#pragma once

#include <memory>
#include <new>

#include "Core/Error/Result.h"
#include "Core/Threading/IThreadFactory.h"
#include "WinThread.h"

namespace Drama::Platform::Threading
{
    class WinThreadFactory final : public Drama::Core::Threading::IThreadFactory
    {
    public:
        Drama::Core::Error::Result create_thread(
            const Drama::Core::Threading::ThreadDesc& desc,
            Drama::Core::Threading::ThreadProc proc,
            void* user,
            std::unique_ptr<Drama::Core::Threading::IThread>& outThread) noexcept override
        {
            // 1) WinThread を確保（例外禁止なので nothrow）
            auto th = std::unique_ptr<WinThread>(new (std::nothrow) WinThread{});
            if (!th)
            {
                return Drama::Core::Error::Result::fail(
                    Drama::Core::Error::Facility::Platform,
                    Drama::Core::Error::Code::OutOfMemory,
                    Drama::Core::Error::Severity::Error,
                    0,
                    "Failed to allocate WinThread.");
            }

            // 2) 実スレッド生成
            const auto r = WinThread::create(desc, proc, user, *th);
            if (!r)
            {
                return r;
            }

            // 3) baseへ移譲
            outThread = std::move(th);
            return Drama::Core::Error::Result::ok();
        }
    };
}
