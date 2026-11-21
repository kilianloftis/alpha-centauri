#include <catch2/catch_test_macros.hpp>

#include "game/research/ResearchManager.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/Tile.h"
#include "game/buildings/BuildingManager.h"

#include "GameFixtures.h"
#include "TestHelpers.h"

using namespace ac;

namespace
{

ActiveEffect_t ClampEffect_(actest::EffectPool& rPool, StatId_t stat, double max,
                            std::optional<Condition_t> condition = std::nullopt)
{
    return actest::Active(
        rPool.StatMod(stat, max, ModifierOp_t::MaxClamp, EffectScope_t::FactionGlobal,
                      actest::AnyTileSelector(), std::move(condition)),
        "tile_yield_rules");
}

Condition_t NoNutrientCapExemption_()
{
    return AllOf_t{{
        TargetTileHas_t{"Nutrients", false},
        TargetTileHas_t{"Condenser", false},
    }};
}

} // namespace

TEST_CASE("ResolveTileYield: absent MaxClamp leaves yield uncapped",
          "[resources][restrictions]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);
    tile.SetBaseMoisture(Moisture_t::Wet);
    tile.SetMoisture(Moisture_t::Wet);
    fixture.ctx.AddOccupantWithEffects(tile, "Farm"); // Wet+2 Farm+1 = 3

    const BaseEffects_t noCaps{base};
    const TileYieldView_t yield = fixture.ctx.ResolveTileYield(tile, false, noCaps);
    CHECK(yield.effective.nutrients == 3);
    CHECK(yield.potential.nutrients == 3);
}

TEST_CASE("ResolveTileYield: MaxClamp clamps when the tile has no exemption",
          "[resources][restrictions]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);
    tile.SetBaseMoisture(Moisture_t::Wet);
    tile.SetMoisture(Moisture_t::Wet);
    fixture.ctx.AddOccupantWithEffects(tile, "Farm");

    actest::EffectPool pool;
    BaseEffects_t caps{base, {ClampEffect_(pool, StatId_t::Nutrients, 2)}};
    const TileYieldView_t yield = fixture.ctx.ResolveTileYield(tile, false, caps);
    CHECK(yield.effective.nutrients == 2);
    CHECK(yield.potential.nutrients == 3);
}

TEST_CASE("ResolveTileYield: nutrient bonus lifts MaxClamp and stacks pre-cap",
          "[resources][restrictions]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);
    tile.SetBaseMoisture(Moisture_t::Wet);
    tile.SetMoisture(Moisture_t::Wet);
    fixture.ctx.AddOccupantWithEffects(tile, "Farm");
    fixture.ctx.AddOccupantWithEffects(tile, "Nutrients");

    actest::EffectPool pool;
    BaseEffects_t caps{base,
                       {ClampEffect_(pool, StatId_t::Nutrients, 2, NoNutrientCapExemption_())}};
    const TileYieldView_t yield = fixture.ctx.ResolveTileYield(tile, false, caps);
    // Wet+Farm+Nutrients = 5; bonus lifts the cap so the full total stands.
    CHECK(yield.effective.nutrients == 5);
    CHECK(yield.potential.nutrients == 5);
}

TEST_CASE("ResolveTileYield: Condenser on the tile lifts the nutrient MaxClamp",
          "[resources][restrictions]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 2);
    Tile& tile = fixture.At(8, 8);
    tile.SetBaseMoisture(Moisture_t::Wet);
    tile.SetMoisture(Moisture_t::Wet);
    fixture.ctx.AddOccupantWithEffects(tile, "Farm");
    fixture.ctx.AddOccupantWithEffects(tile, "Condenser");

    actest::EffectPool pool;
    BaseEffects_t caps{base,
                       {ClampEffect_(pool, StatId_t::Nutrients, 2, NoNutrientCapExemption_())}};
    const TileYieldView_t yield = fixture.ctx.ResolveTileYield(tile, false, caps);
    // Condenser MultiplyStaged ×1.5 lifts the MaxClamp 2: Wet+Farm 3 → trunc 4 (not clamped to 2).
    CHECK(yield.effective.nutrients == 4);
    CHECK(yield.potential.nutrients == 4);
}

TEST_CASE("Tile resource restrictions: production caps worked tiles but not flat base bonuses",
          "[resources][restrictions][pipeline]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);

    Tile& farmTile = fixture.At(9, 9);
    farmTile.SetBaseMoisture(Moisture_t::Wet);
    farmTile.SetMoisture(Moisture_t::Wet);
    fixture.ctx.AddOccupantWithEffects(farmTile, "Farm");

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
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);

    Tile& farmTile = fixture.At(9, 9);
    farmTile.SetBaseMoisture(Moisture_t::Wet);
    farmTile.SetMoisture(Moisture_t::Wet);
    fixture.ctx.AddOccupantWithEffects(farmTile, "Farm");

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

TEST_CASE("Tile resource restrictions: nutrient bonus lifts the worked-tile MaxClamp",
          "[resources][restrictions][bonus]")
{
    actest::FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 8);

    Tile& rich = fixture.At(9, 9);
    rich.SetBaseMoisture(Moisture_t::Wet);
    rich.SetMoisture(Moisture_t::Wet); // +2 nutrients
    fixture.ctx.AddOccupantWithEffects(rich, "Farm"); // +1
    fixture.ctx.AddOccupantWithEffects(rich, "Nutrients"); // +2 pre-cap; lifts the clamp

    base.UserAssignBestAvailableWorker(&rich);

    CHECK(base.GetWorkedTileYield(rich).effective.nutrients == 5);
    CHECK(base.GetPreviewTileYield(rich).effective.nutrients == 5);
}
