#include "game/map/OccupantCoexistence.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"

#include <algorithm>

namespace ac
{

namespace
{

using WaiverPair_t = std::pair<std::string, std::string>;

bool ExcludesId_(const ImprovementConfig_t& rFeature, std::string_view id)
{
    return std::find(rFeature.excludes.begin(), rFeature.excludes.end(), id)
        != rFeature.excludes.end();
}

bool AxisMatches_(const std::string& rAxis, std::string_view id)
{
    return rAxis.empty() || rAxis == id;
}

// The pair is unordered: either id may name either side, so a modder declares the
// relationship once without having to know which occupant the placement treats as incoming.
bool OverrideMatchesPair_(const CoexistenceOverrideEffect_t& rOverride, std::string_view a,
                          std::string_view b)
{
    return (AxisMatches_(rOverride.firstId, a) && AxisMatches_(rOverride.secondId, b))
        || (AxisMatches_(rOverride.firstId, b) && AxisMatches_(rOverride.secondId, a));
}

bool WaiverCovers_(std::span<const WaiverPair_t> waivers, std::string_view a, std::string_view b)
{
    return std::any_of(waivers.begin(), waivers.end(), [&](const WaiverPair_t& rWaiver) {
        return (rWaiver.first == a && rWaiver.second == b)
            || (rWaiver.first == b && rWaiver.second == a);
    });
}

const ImprovementConfig_t* FindOccupant_(const Tile& rTile, std::string_view id)
{
    const ImprovementConfig_t* pFound = nullptr;
    rTile.ForEachOccupant([&](const ImprovementConfig_t& rOccupant) {
        if (rOccupant.id != id)
        {
            return false;
        }
        pFound = &rOccupant;
        return true;
    });
    return pFound;
}

// Stock deny when either side names the other. An override applies only when its cell
// differs from that stock answer; a recorded waiver is a standing Allow, so it is consulted
// only once the pair is otherwise denied and never outranks an explicit override.
bool PairDenied_(const ImprovementConfig_t& rCandidate, std::string_view otherId,
                 const ImprovementConfig_t* pOther,
                 std::span<const CoexistenceOverrideEffect_t> overrides,
                 std::span<const WaiverPair_t> waivers)
{
    const bool bStockDeny = ExcludesId_(rCandidate, otherId)
        || (pOther && ExcludesId_(*pOther, rCandidate.id));
    const InteractionCell_t stock = bStockDeny ? InteractionCell_t::Deny : InteractionCell_t::Allow;
    for (const CoexistenceOverrideEffect_t& rOverride : overrides)
    {
        if (!OverrideMatchesPair_(rOverride, rCandidate.id, otherId) || rOverride.cell == stock)
        {
            continue;
        }
        return rOverride.cell == InteractionCell_t::Deny;
    }
    if (bStockDeny && WaiverCovers_(waivers, rCandidate.id, otherId))
    {
        return false;
    }
    return bStockDeny;
}

bool DomainBlocks_(const ImprovementConfig_t& rCandidate, const Tile& rTile)
{
    if (rCandidate.domain == ImprovementDomain_t::Land && !rTile.IsLand())
    {
        return true;
    }
    return rCandidate.domain == ImprovementDomain_t::Sea && !rTile.IsWater();
}

} // namespace

bool OccupantsBlockPlacement(const Tile& rTile, const ImprovementConfig_t& rCandidate,
                             std::span<const std::string> leavingIds,
                             std::span<const CoexistenceOverrideEffect_t> overrides)
{
    if (DomainBlocks_(rCandidate, rTile))
    {
        return true;
    }

    // Waivers already earned on this tile stand alongside the acting former's overrides, so
    // an improvement built under a waiver survives later occupancy sweeps that pass none.
    const std::span<const WaiverPair_t> waivers = rTile.GetCoexistenceWaivers();

    // pKnownOther is the occupant the caller is already holding; the excludes pass has only
    // an id, and resolves it lazily so a candidate that names many absent features does not
    // scan the tile once per name.
    const auto blocked = [&](std::string_view otherId, const ImprovementConfig_t* pKnownOther)
    {
        if (otherId.empty()
            || std::find(leavingIds.begin(), leavingIds.end(), otherId) != leavingIds.end())
        {
            return false;
        }
        if (!rTile.HasFeature(otherId))
        {
            return false;
        }
        const ImprovementConfig_t* pOther =
            pKnownOther ? pKnownOther : FindOccupant_(rTile, otherId);
        return PairDenied_(rCandidate, otherId, pOther, overrides, waivers);
    };

    for (const std::string& rExcludedId : rCandidate.excludes)
    {
        if (blocked(rExcludedId, nullptr))
        {
            return true;
        }
    }
    return rTile.ForEachOccupant([&](const ImprovementConfig_t& rOccupant) {
        return blocked(rOccupant.id, &rOccupant);
    });
}

bool CanBuildImprovement(const Tile& rTile, const ImprovementConfig_t& rCandidate,
                         std::span<const std::string> leavingIds,
                         std::span<const CoexistenceOverrideEffect_t> overrides)
{
    return !OccupantsBlockPlacement(rTile, rCandidate, leavingIds, overrides);
}

std::vector<WaiverPair_t> WaivedPairsFor(const Tile& rTile,
                                         const ImprovementConfig_t& rCandidate,
                                         std::span<const CoexistenceOverrideEffect_t> overrides)
{
    std::vector<WaiverPair_t> waived;
    if (overrides.empty())
    {
        return waived;
    }
    rTile.ForEachOccupant([&](const ImprovementConfig_t& rOther) {
        if (rOther.id != rCandidate.id && PairDenied_(rCandidate, rOther.id, &rOther, {}, {})
            && !PairDenied_(rCandidate, rOther.id, &rOther, overrides, {}))
        {
            waived.emplace_back(rCandidate.id, rOther.id);
        }
        return false;
    });
    return waived;
}

std::vector<std::string> ImprovementsDisplacedBy(
    const Tile& rTile, const ImprovementConfig_t& rIncoming,
    std::span<const CoexistenceOverrideEffect_t> overrides)
{
    const std::span<const WaiverPair_t> waivers = rTile.GetCoexistenceWaivers();
    std::vector<std::string> displaced;
    for (const ImprovementConfig_t* pExisting : rTile.GetImprovements())
    {
        if (!pExisting || pExisting->id == rIncoming.id)
        {
            continue;
        }
        if (PairDenied_(rIncoming, pExisting->id, pExisting, overrides, waivers))
        {
            displaced.push_back(pExisting->id);
        }
    }
    return displaced;
}

} // namespace ac
