#pragma once

#include "game/ecology/EcoDamageCalculator.h"

#include <cstdint>
#include <vector>

namespace ac
{

class BaseManager;
class GameState;
struct AtrocitiesConfig_t;

// One base's eco-damage score: assembles EcoDamageInputs_t from the base's tiles, effects and
// the session ledgers, and hands them to the GameDataContext's EcoDamageCalculator. Memoized
// on every revision an input reads — including the atrocity and ecology ledgers, which move
// the score without touching any effect pool.
class BaseEcology
{
public:
    // rBase owns this and outlives it.
    BaseEcology(const BaseManager& rBase, const EcoDamageCalculator& rCalculator,
                const AtrocitiesConfig_t& rAtrocities);

    // The fungal-pop percentage. Throws when the owning faction is not bound to a GameState:
    // the score reads session ledgers.
    int GetDamage() const;

private:
    const GameState& RequireGameState_() const;
    EcoDamageInputs_t CollectInputs_(const GameState& rGameState) const;
    double TerraformRaw_() const;
    void CollectRevisions_(const GameState& rGameState, std::vector<uint64_t>& rOut) const;

    const BaseManager& m_rBase;
    const EcoDamageCalculator& m_rCalculator;
    const AtrocitiesConfig_t& m_rAtrocities;
    // Empty never matches a real collection, so the first query builds.
    mutable std::vector<uint64_t> m_cachedStamp;
    mutable std::vector<uint64_t> m_scratchRevisions;
    mutable int m_cachedDamage = 0;
};

} // namespace ac
