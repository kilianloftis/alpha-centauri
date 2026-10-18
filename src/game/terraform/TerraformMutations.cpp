#include "game/terraform/TerraformMutations.h"

#include "game/effects/CoexistenceResolve.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/Tile.h"
#include "game/units/IUnitOrderWorld.h"
#include "game/units/Unit.h"

#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

// Nothing is removed until the placement is known to succeed: a project that has already
// been paid for must not leave the tile stripped and empty when terrain shifted mid-order.
bool PlaceImprovement_(Tile& rTile, const ImprovementConfig_t& rPlaced,
                       TileEffectsContext& rTileEffects,
                       std::span<const CoexistenceOverrideEffect_t> overrides)
{
    if (rTile.HasImprovement(rPlaced.id))
    {
        return true;
    }
    if (!ImprovementApplies(rTile, rPlaced, overrides))
    {
        return false;
    }

    for (const auto& [rImprovementId, rFeatureId] : WaivedPairsFor(rTile, rPlaced, overrides))
    {
        rTile.AddCoexistenceWaiver(rImprovementId, rFeatureId);
    }
    for (const std::string& rId : ImprovementsDisplacedBy(rTile, rPlaced, overrides))
    {
        rTileEffects.RemoveOccupantWithEffects(rTile, rId);
    }
    rTileEffects.AddOccupantWithEffects(rTile, rPlaced.id);
    return rTile.HasImprovement(rPlaced.id);
}

} // namespace

bool ApplyTerraformResult(Tile& rTile, const TerraformProject_t& rProject,
                          TileEffectsContext& rTileEffects, Unit& rFormer, std::mt19937& rRng,
                          IUnitOrderWorld* pWorld)
{
    bool bDidAnything = false;
    if (rProject.pPlaces)
    {
        const std::vector<CoexistenceOverrideEffect_t> overrides =
            ActiveCoexistenceOverrides(rFormer);
        bDidAnything = PlaceImprovement_(rTile, *rProject.pPlaces, rTileEffects, overrides);
    }
    if (!rProject.onCompleteEffects.empty())
    {
        // The effects mutate world state, which a movement-only harness has no session for.
        if (!pWorld)
        {
            throw std::runtime_error("Terraform project '" + rProject.id
                                     + "' declares on_complete_effects but there is no session "
                                       "to apply them");
        }
        const bool bEffectsApplied =
            pWorld->ApplyTerraformCompleteEffects(rTile, rProject, rFormer, rRng);
        bDidAnything = bEffectsApplied || bDidAnything;
    }
    return bDidAnything;
}

} // namespace ac
