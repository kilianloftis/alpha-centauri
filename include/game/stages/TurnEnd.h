#pragma once

#include "game/TurnStages.h"

namespace ac
{

class TurnEnd : public TurnStageBase
{
public:
    TurnEnd(std::shared_ptr<HookContext> hookContext);
    ~TurnEnd() = default;

    void Execute_() override;
};

} // namespace ac
