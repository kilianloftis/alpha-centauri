#pragma once

#include "TestHelpers.h"

#include "game/buildings/BuildingRegistry.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "lib/effects/TileEffectsContext.h"

#include <memory>
#include <vector>

namespace actest
{

// A small world with the fixture improvement registry loaded and a TileEffectsContext over it.
// Note: TileEffectsContext caches the max improvement radius at construction, so the registry
// must be loaded first — hence the unique_ptr.
struct WorldFixture
{
    ac::WorldMap map;
    ac::ImprovementRegistry improvements;
    std::unique_ptr<ac::TileEffectsContext> ctx;

    explicit WorldFixture(int width = 9, int height = 9)
        : map(width, height)
    {
        improvements.Load(FixturePath("improvements.json"));
        ctx = std::make_unique<ac::TileEffectsContext>(map, improvements);
    }

    ac::Tile& At(int x, int y) { return *map.GetTile(x, y); }
};

// Just the pop-type registry, for tests that only need PopTypeConfig_t entries.
struct PopTypeRegistryOnly
{
    ac::PopTypeRegistry popTypes;

    PopTypeRegistryOnly() { popTypes.Load(FixturePath("pop_types.json")); }
};

// WorldFixture plus the building/pop-type registries and the ability to found real bases.
// Bases are created with the minimum viable dependency set: no research/production/composition
// calculators. Each base starts with 3 default Worker pops (BaseManager's built-in initial size)
// and registers the "Base" improvement on its tile, exactly as in the game.
struct BaseFixture : WorldFixture
{
    ac::BuildingRegistry buildings;
    ac::PopTypeRegistry popTypes;
    ac::GrowthConfig_t growth{};
    ac::EconomyManager economy; // default 40/50/10 energy split
    std::vector<std::unique_ptr<ac::BaseManager>> bases;

    BaseFixture()
    {
        buildings.Load(FixturePath("buildings.json"));
        popTypes.Load(FixturePath("pop_types.json"));
    }

    ac::BaseManager& MakeBase(int x, int y)
    {
        bases.push_back(std::make_unique<ac::BaseManager>(
            At(x, y), &buildings, &popTypes,
            /*popTypeAvailability*/ nullptr, /*compositionCalculator*/ nullptr,
            *ctx,
            /*research*/ nullptr, &economy, /*productionCost*/ nullptr,
            growth, /*secretProjects*/ nullptr));
        return *bases.back();
    }
};

} // namespace actest
