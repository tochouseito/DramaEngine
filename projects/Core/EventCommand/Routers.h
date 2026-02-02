#pragma once
#include "EventCommand.h"
#include "Events.h"
#include "Commands.h"
#include <string>

namespace Drama::Core::Routers
{
    class ShowWindowRouter final : public Drama::Core::EventCommand::IRouter
    {
    public:
        ShowWindowRouter(EventCommand::EventSystem& ev,
            EventCommand::CommandBuffer& quere)
            : IRouter(ev, quere)
        {
            Subscribe<Events::EveShowWindow>([this](const Events::EveShowWindow& e) {
                std::snprintf(msg.data(), msg.size(), "%s", e.msg);
                });
        }
        void Flush() override
        {
            Commands::CmdShowWindow cmd{};
            std::snprintf(cmd.msg, sizeof(cmd.msg), "%s", msg.c_str());
            m_CommandBuffer.Push(Commands::ExecShowWindow, cmd);
        }
    private:
        std::string msg{};
    };
} // namespace Drama::Core::Routers
