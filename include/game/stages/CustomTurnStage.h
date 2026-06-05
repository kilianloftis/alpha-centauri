#pragma once

#include "game/TurnStages.h"
#include "game/HookContext.h"
#include <string>
#include <stdexcept>

namespace ac
{

class CustomTurnStage : public TurnStageBase
{
public:
    CustomTurnStage(std::shared_ptr<HookContext> hookContext, const std::string& name);
    ~CustomTurnStage() = default;

    void Execute_() override {}

private:
    std::string m_name;
};

} // namespace ac
