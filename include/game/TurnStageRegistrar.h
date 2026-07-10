#pragma once

#include "game/TurnStageFactory.h"
#include "game/TurnStages.h"
#include <memory>
#include <string>
#include <utility>

namespace ac
{

// A file-scope instance of TurnStageRegistrar<T>(id) registers T with TurnStageFactory
// at static-init time, so adding a new built-in stage never requires editing the factory.
template <typename T>
struct TurnStageRegistrar
{
    explicit TurnStageRegistrar(std::string id)
    {
        TurnStageFactory::RegisterCreator(std::move(id), [](std::shared_ptr<HookContext> pHookContext)
        {
            return std::make_unique<T>(std::move(pHookContext));
        });
    }
};

} // namespace ac
