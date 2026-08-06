#pragma once

#include "game/TurnStageFactory.h"
#include "game/TurnStages.h"
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace ac
{

// A file-scope instance of TurnStageRegistrar<T>(id) registers T with TurnStageFactory
// at static-init time, so adding a new built-in stage never requires editing the factory.
// Kind (global vs per-faction) is fixed at compile time from T's base.
template <typename T>
struct TurnStageRegistrar
{
    explicit TurnStageRegistrar(std::string id)
    {
        if constexpr (std::is_base_of_v<GlobalTurnStage, T>)
        {
            static_assert(!std::is_base_of_v<PerFactionTurnStage, T>,
                          "Turn stage type cannot derive from both Global and PerFaction bases");
            TurnStageFactory::RegisterGlobalCreator(
                std::move(id), [](HookContext hookContext)
                {
                    return std::make_unique<T>(std::move(hookContext));
                });
        }
        else if constexpr (std::is_base_of_v<PerFactionTurnStage, T>)
        {
            TurnStageFactory::RegisterPerFactionCreator(
                std::move(id), [](HookContext hookContext)
                {
                    return std::make_unique<T>(std::move(hookContext));
                });
        }
        else
        {
            static_assert(std::is_base_of_v<GlobalTurnStage, T>
                              || std::is_base_of_v<PerFactionTurnStage, T>,
                          "TurnStageRegistrar<T> requires T to derive from GlobalTurnStage "
                          "or PerFactionTurnStage");
        }
    }
};

} // namespace ac
