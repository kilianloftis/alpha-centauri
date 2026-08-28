#include <catch2/catch_test_macros.hpp>

#include "game/faction/ResearchManager.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/Tile.h"

#include "GameFixtures.h"
#include "TestHelpers.h"

using namespace ac;

namespace
{

ActiveEffect_t ClampEffect_(actest::EffectPool& rPool, StatId_t stat, double max)
{
    return actest::Active(
        rPool.StatMod(stat, max, ModifierOp_t::MaxClamp, EffectScope_t::FactionGlobal,
                      actest::AnyTileSelector()),
        "tile_yield_rules");
}

} // namespace

TEST_CASE("ResolveTileYield: absent MaxClamp leaves yield uncapped",
          "[resources][restrictions]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(1, 1);
    Tile& tile = fixture.At(4, 4);
    tile.SetBaseMoisture(Moisture_t::Wet);
    tile.SetMoisture(Moisture_t::Wet);
    fixture.ctx->AddImprovementWithEffects(tile, "Farm"); // Wet+2 Farm+1 = 3

    const BaseEffects_t noCaps{base};
    const TileYieldView_t yield = fixture.ctx->ResolveTileYield(tile, false, noCaps);
    CHECK(yield.effective.nutrients == 3);
    CHECK(yield.potential.nutrients == 3);
}

TEST_CASE("ResolveTileYield: MaxClamp clamps the pre-bypass lane",
          "[resources][restrictions]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(1, 1);
    Tile& tile = fixture.At(4, 4);
    tile.SetBaseMoisture(Moisture_t::Wet);
    tile.SetMoisture(Moisture_t::Wet);
    fixture.ctx->AddImprovementWithEffects(tile, "Farm");

    actest::EffectPool pool;
    BaseEffects_t caps{base, {ClampEffect_(pool, StatId_t::Nutrients, 2)}};
    const TileYieldView_t yield = fixture.ctx->ResolveTileYield(tile, false, caps);
    CHECK(yield.effective.nutrients == 2);
    CHECK(yield.potential.nutrients == 3);
}

TEST_CASE("ResolveTileYield: bypass_clamp bonuses bypass MaxClamp",
          "[resources][restrictions]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(1, 1);
    Tile& tile = fixture.At(4, 4);
    tile.SetBaseMoisture(Moisture_t::Wet);
    tile.SetMoisture(Moisture_t::Wet);
    fixture.ctx->AddImprovementWithEffects(tile, "Farm");
    fixture.ctx->AddImprovementWithEffects(tile, "Nutrients"); // +2 after clamp

    actest::EffectPool pool;
    BaseEffects_t caps{base, {ClampEffect_(pool, StatId_t::Nutrients, 2)}};
    const TileYieldView_t yield = fixture.ctx->ResolveTileYield(tile, false, caps);
    // min(Wet+Farm=3, 2) + Nutrients 2 = 4
    CHECK(yield.effective.nutrients == 4);
    CHECK(yield.potential.nutrients == 5);
}

TEST_CASE("Tile resource restrictions: production caps worked tiles but not flat base bonuses",
          "[resources][restrictions][pipeline]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Tile& farmTile = fixture.At(5, 4);
    farmTile.SetBaseMoisture(Moisture_t::Wet);
    farmTile.SetMoisture(Moisture_t::Wet);
    fixture.ctx->AddImprovementWithEffects(farmTile, "Farm");

    base.GetBuildingManager().AddBuilding("flat_nutrient"); // +2 flat, uncapped
    base.GetBuildingManager().AddBuilding("farm_booster");  // +1 on Farms (part of tile)
    base.UserAssignBestAvailableWorker(&farmTile);

    // Wet(+2) + Farm(+1) + booster(+1) = 4, capped to 2; flat +2 still applies → 4.
    // Preview is as-if-worked for tile-level yield (same selectors + caps as ResolveTileYield).
    CHECK(base.GetNutrientProduction() == 4);
    CHECK(base.GetWorkedTileYield(farmTile).effective.nutrients == 2);
    CHECK(base.GetPreviewTileYield(farmTile).effective.nutrients == 2);
    CHECK(base.GetWorkedTileYield(farmTile).potential.nutrients == 4);
    CHECK(base.GetPreviewTileYield(farmTile).potential.nutrients == 4);

    // Discovering the lift tech rebuilds the faction pool without the nutrient MaxClamp.
    faction.GetResearch().AddDiscoveredTech("gene_splicing");
    CHECK(base.GetNutrientProduction() == 6);
    CHECK(base.GetWorkedTileYield(farmTile).effective.nutrients == 4);
    CHECK(base.GetPreviewTileYield(farmTile).effective.nutrients == 4); // Wet+Farm+booster
    // The requirement itself: preview is the worked tile-level yield. Fixture pops carry no
    // ThisPop multipliers, so ApplyTileMultipliers is identity and the two must agree exactly.
    CHECK(base.GetPreviewTileYield(farmTile).effective.nutrients
          == base.GetWorkedTileYield(farmTile).effective.nutrients);
    CHECK(base.GetWorkedTileYield(farmTile).effective.nutrients
          == base.GetWorkedTileYield(farmTile).potential.nutrients);
}

TEST_CASE("Preview yield on an unworked tile includes selector modifiers",
          "[resources][restrictions]")
{
    // The placement-UX case: BaseWorkableAreaDisplay shows preview for tiles with no worker,
    // so a building that boosts Farms must be visible before the player assigns anyone.
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Tile& farmTile = fixture.At(5, 4);
    farmTile.SetBaseMoisture(Moisture_t::Wet);
    farmTile.SetMoisture(Moisture_t::Wet);
    fixture.ctx->AddImprovementWithEffects(farmTile, "Farm");

    base.GetBuildingManager().AddBuilding("farm_booster"); // +1 on Farms
    faction.GetResearch().AddDiscoveredTech("gene_splicing"); // lift the nutrient MaxClamp

    // No worker assigned to farmTile — Wet(+2) + Farm(+1) + booster(+1) = 4.
    CHECK(base.GetPreviewTileYield(farmTile).effective.nutrients == 4);
    CHECK(base.GetPreviewTileYield(farmTile).potential.nutrients == 4);

    // Assigning a worker must not change the tile-level number the preview promised.
    base.UserAssignBestAvailableWorker(&farmTile);
    CHECK(base.GetWorkedTileYield(farmTile).effective.nutrients
          == base.GetPreviewTileYield(farmTile).effective.nutrients);
}

TEST_CASE("Tile resource restrictions: resource-bonus improvements apply after MaxClamp",
          "[resources][restrictions][bonus]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);

    Tile& rich = fixture.At(5, 4);
    rich.SetBaseMoisture(Moisture_t::Wet);
    rich.SetMoisture(Moisture_t::Wet); // +2 nutrients
    fixture.ctx->AddImprovementWithEffects(rich, "Farm"); // +1
    fixture.ctx->AddImprovementWithEffects(rich, "Nutrients"); // +2 after clamp

    base.UserAssignBestAvailableWorker(&rich);

    // min(Wet+Farm=3, 2) + Nutrients bonus 2 = 4
    CHECK(base.GetWorkedTileYield(rich).effective.nutrients == 4);
    CHECK(base.GetPreviewTileYield(rich).effective.nutrients == 4);
}
