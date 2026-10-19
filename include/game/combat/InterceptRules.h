#pragma once

#include "game/base/BaseTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace ac
{

class Unit;
struct InterceptEffect_t;

enum class InterceptSourceKind_t
{
    None,
    Building,
    Unit,
};

// One intercept charge on the defender's side. pIntercept carries the hit chance, the chance
// that a miss destroys the source, and the cooldown.
struct InterceptSource_t
{
    const InterceptEffect_t* pIntercept = nullptr;
    // The ledger charged for the cooldown; None when the effect opts out of cooldowns.
    InterceptSourceKind_t deployKind = InterceptSourceKind_t::None;
    // What a failed roll may destroy. A Unit source is the defender itself.
    InterceptSourceKind_t sourceKind = InterceptSourceKind_t::None;
    // A building id on the Building lanes; a component or tile-feature id otherwise.
    std::string sourceId;
    // The base a ThisBase charge came from. Faction-wide charges belong to no single base.
    std::optional<BaseId_t> baseId;
    // A WorldGlobal charge bills the faction that granted it; every other charge the defender.
    FactionId_t ledgerFactionId = 0;
};

// Every source in the order they are tried: the defender faction's pool, the base on the
// defender's tile, the defender's design, each faction's WorldGlobal grants, council, tile.
std::vector<InterceptSource_t> CollectInterceptSources(const Unit& rAttacker,
                                                       const Unit& rDefender);

// Off cooldown at missionYear. A source with no ledger is always ready.
bool InterceptSourceReady(const InterceptSource_t& rSource, const Unit& rDefender,
                          int missionYear);

// The base a building charge is billed to and lost from: the firing base, or the ledger's
// first base holding the building. Empty when no base holds it.
std::optional<BaseId_t> InterceptChargeBase(const InterceptSource_t& rSource);

} // namespace ac
