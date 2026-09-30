#pragma once

#include "game/effects/EffectConfig.h"

#include <string>
#include <vector>

namespace ac
{

// One planet-wide native life abundance setting (rare / normal / abundant). A campaign
// property like difficulty: GameRulesConfig_t::nativeLifeLevelId names it, and its continuous
// effects enter every faction's pool.
struct NativeLifeLevel_t
{
    std::string id;
    std::string name;
    std::vector<EffectConfig_t> effects;
};

struct NativeLifeLevelConfig_t
{
    std::string defaultId;
    std::vector<NativeLifeLevel_t> levels;

    const NativeLifeLevel_t* FindById(const std::string& rId) const;
    // Session lookup: empty rLevelId selects defaultId. Throws when the id is unknown.
    const NativeLifeLevel_t& RequireForSession(const std::string& rLevelId) const;
};

class NativeLifeLevelConfigParser
{
public:
    // Load config/native_life_levels.json. Every key is required. Throws on a missing file,
    // unknown keys, duplicate ids, a default naming no level, or invalid effects.
    NativeLifeLevelConfig_t ParseConfig(const std::string& rConfigPath);
};

} // namespace ac
