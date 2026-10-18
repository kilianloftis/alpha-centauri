#pragma once

#include "game/ecology/EcoDamageCalculator.h"

#include <cstdint>
#include <vector>

namespace ac
{

class BaseManager;
class GameState;

// One base's eco-damage score: EcoDamageInputs (EcologyRules) through the GameDataContext's
// EcoDamageCalculator. Memoized on every revision an input reads — including the atrocity and
// ecology ledgers, which move the score without touching any effect pool.
class BaseEcology
{
public:
    // rBase owns this and outlives it.
    BaseEcology(const BaseManager& rBase, const EcoDamageCalculator& rCalculator);

    // The fungal-pop percentage. Throws when the owning faction is not bound to a GameState:
    // the score reads session ledgers.
    int GetDamage() const;

private:
    void CollectRevisions_(const GameState& rGameState, std::vector<uint64_t>& rOut) const;

    const BaseManager& m_rBase;
    const EcoDamageCalculator& m_rCalculator;
    // Empty never matches a real collection, so the first query builds.
    mutable std::vector<uint64_t> m_cachedStamp;
    mutable std::vector<uint64_t> m_scratchRevisions;
    mutable int m_cachedDamage = 0;
};

} // namespace ac
