#pragma once

#include "game/TurnStages.h"

namespace ac
{

class WorldEvents : public TurnStageBase
{
public:
    WorldEvents(std::shared_ptr<HookContext> hookContext);
    ~WorldEvents() = default;

    void Execute_() override;
};

} // namespace ac
