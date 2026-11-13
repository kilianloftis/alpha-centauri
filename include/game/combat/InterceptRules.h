#pragma once

#include "game/base/BaseTypes.h"

#include <optional>
#include <string>
#include <variant>
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
    Tile,
};

struct InterceptNoneIdentity_t
{
    std::string sourceId;
};

struct InterceptBuildingIdentity_t
{
    std::string sourceId;
    // A WorldGlobal charge bills the grantor; every other building charge bills the defender.
    FactionId_t ledgerFactionId = 0;
    // The base a ThisBase charge came from. Faction-wide charges belong to no single base.
    std::optional<BaseId_t> baseId;
};

struct InterceptUnitIdentity_t
{
    std::string sourceId;
    // Always the unit whose latch is charged and that fail-destroy may remove (UnitId_t).
    int unitId = 0;
};

struct InterceptTileIdentity_t
{
    std::string sourceId;
    TileCoord_t hostTile{0, 0};
};

using InterceptIdentity_t = std::variant<InterceptNoneIdentity_t, InterceptBuildingIdentity_t,
                                         InterceptUnitIdentity_t, InterceptTileIdentity_t>;

// One intercept charge on the defender's side. pIntercept carries the hit chance, the chance
// that a miss destroys the source, and the cooldown.
struct InterceptSource_t
{
    const InterceptEffect_t* pIntercept = nullptr;
    // The ledger charged for the cooldown; None when the effect opts out of cooldowns.
    InterceptSourceKind_t deployKind = InterceptSourceKind_t::None;
    // Physical source: which identity alternative is set, and what fail-destroy removes.
    InterceptIdentity_t identity;
};

inline const std::string& InterceptSourceId(const InterceptSource_t& rSource)
{
    return std::visit([](const auto& rId) -> const std::string& { return rId.sourceId; },
                      rSource.identity);
}

inline InterceptSourceKind_t InterceptPhysicalKind(const InterceptSource_t& rSource)
{
    if (std::holds_alternative<InterceptBuildingIdentity_t>(rSource.identity))
    {
        return InterceptSourceKind_t::Building;
    }
    if (std::holds_alternative<InterceptUnitIdentity_t>(rSource.identity))
    {
        return InterceptSourceKind_t::Unit;
    }
    if (std::holds_alternative<InterceptTileIdentity_t>(rSource.identity))
    {
        return InterceptSourceKind_t::Tile;
    }
    return InterceptSourceKind_t::None;
}

// Every source in the order they are tried: the defender faction's pool, the base on the
// defender's tile, the defender's live unit effects (ThisUnit, FactionUnits,
// ProducedAtThisBase), each faction's WorldGlobal grants, council, tile.
std::vector<InterceptSource_t> CollectInterceptSources(const Unit& rAttacker,
                                                       const Unit& rDefender);

// Off cooldown at missionYear. A source with no ledger is always ready.
bool InterceptSourceReady(const InterceptSource_t& rSource, int missionYear);

// The base a building charge is billed to and lost from: the firing base, or the ledger's
// first base holding the building. Empty when no base holds it.
std::optional<BaseId_t> InterceptChargeBase(const InterceptSource_t& rSource);

} // namespace ac
