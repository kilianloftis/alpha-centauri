#pragma once

#include "game/TurnStages.h"

namespace ac
{

class Population : public TurnStageBase
{
public:
    Population(std::shared_ptr<HookContext> hookContext);
    ~Population() = default;

    void Execute_() override;
};

} // namespace ac
