#pragma once

#include "game/stages/HookContext.h"
#include <string>
#include <vector>

namespace ac
{

struct TurnStageConfig_t
{
    std::string id;
    std::string name;
    std::string description;
    bool bRepeatForEachFaction = false;
    HookContext hookContext;
};

class TurnStageConfigParser
{
public:
    TurnStageConfigParser() = default;

    // Throws on duplicate stage ids, unbound scriptPath hooks, or empty hook objects.
    std::vector<TurnStageConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
