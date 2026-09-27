#pragma once

#include "game/effects/EffectConfig.h"

#include <string>
#include <vector>

namespace ac
{

// Non-effect difficulty knobs: settings with no stat to attach to, so they cannot ride the
// effects system. Both atrocity thresholds are read by CommitAtrocity. The other fields are
// stubbed
// with TODO(difficulty) at their call sites.
//
// Difficulty is changeable mid-campaign, so readers re-resolve the session level from
// GameSettings rather than snapshotting these fields at Faction construction.
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
    // Counted Simple acts a perpetrator may reach and still be answered for as Simple. The
    // next counted Simple act, which would pass this number, is Major. Zero disables
    // escalation. Both are required in config/difficulty.json and read from the session level
    // at commit time, so a mid-campaign difficulty change moves them for the next act.
    //
    // Shipping player values are 4 * (8 - difficulty) with Citizen = 0: Citizen 32,
    // Specialist 28, Talent 24, Librarian 20, Thinker 16, Transcend 12. The AI answers at one
    // number the session difficulty does not move, so every shipping level states the same 20
    // — a mod is free to vary it.
    int playerAtrocityThreshold = 0;
    int aiAtrocityThreshold = 0;
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
