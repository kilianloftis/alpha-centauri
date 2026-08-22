#pragma once

#include "game/effects/EffectConfig.h"

#include <string>
#include <vector>

namespace ac
{

// Non-effect difficulty knobs: settings with no stat to attach to, so they cannot ride the
// effects system. Parsed and validated here; the systems that read them do not exist yet and
// are stubbed with TODO(difficulty) at their call sites.
//
// The first real consumer should snapshot what it needs at Faction construction (see
// RequireSessionDifficulty_ in Faction.cpp) rather than re-resolving from GameSettings, which
// would drift from the difficulty effects the faction's pool was built from.
struct DifficultyRules_t
{
    int randomEventsAfterTurn = 0;
    int researchDisabledTurns = 0;
    bool aiSecretProjectsRequireHumanPrereq = false;
    bool colonyPodPreservesSize1Base = false;
    bool noPowerOverloads = false;
    bool noIncitedPactTreatyScripts = false;
    // Default on when omitted (harder levels). Citizen/Specialist set false.
    bool aiAutoPersonality = true;
    // Magnitude unknown — mode stub only.
    bool combatHandicap = false;
    bool combatHandicapNativesOnly = false;
};

struct DifficultyLevel_t
{
    std::string id;
    std::string name;
    DifficultyRules_t rules;
    std::vector<EffectConfig_t> effects;
};

struct DifficultyConfig_t
{
    std::string defaultId;
    std::vector<DifficultyLevel_t> levels;

    const DifficultyLevel_t* FindById(const std::string& rId) const;
    // Session lookup: empty rDifficultyId selects defaultId. Throws when the id is unknown.
    const DifficultyLevel_t& RequireForSession(const std::string& rDifficultyId) const;
};

} // namespace ac
