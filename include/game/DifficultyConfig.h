#pragma once

#include "game/effects/EffectConfig.h"

#include <string>
#include <vector>

namespace ac
{

// Difficulty knobs that are not effects. Readers re-resolve the session level from GameSettings,
// since difficulty can change mid-campaign.
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
    // Counted Simple acts a perpetrator may reach and still be answered as Simple; the next one
    // is Major. Zero disables escalation. Read at commit time.
    int playerAtrocityThreshold = 0;
    int aiAtrocityThreshold = 0;
    // SMAC DiffLevel term in Planet fauna early_limit and boat-cargo formulas (Citizen…Transcend → 0…5).
    int planetFaunaDiff = 0;
    // Allows the near-base XMINDWORMS fauna burst (Librarian and up in shipping).
    bool planetFaunaSwarmEvent = false;
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
