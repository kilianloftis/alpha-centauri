#pragma once

#include "TestHelpers.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/social-engineering/SocialPolicyRegistry.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "lib/effects/TileEffectsContext.h"

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace actest
{

// Just the pop-type registry, for tests that only need PopTypeConfig_t entries.
struct PopTypeRegistryOnly
{
    ac::PopTypeRegistry popTypes;

    PopTypeRegistryOnly() { popTypes.Load(FixturePath("pop_types.json")); }
};

// A small world with the fixture improvement and unit-component registries loaded and a
// TileEffectsContext over them. Note: TileEffectsContext caches the max effect radius at
// construction, so the registries must be loaded first — hence the unique_ptr.
struct WorldFixture
{
    ac::WorldMap map;
    ac::ImprovementRegistry improvements;
    ac::UnitComponentRegistry unitComponents;
    std::unique_ptr<ac::TileEffectsContext> ctx;

    explicit WorldFixture(int width = 9, int height = 9)
        : map(width, height)
    {
        improvements.Load(FixturePath("improvements.json"));
        unitComponents.Load(FixturePath("unit_components.json"));
        ctx = std::make_unique<ac::TileEffectsContext>(map, improvements, &unitComponents);
    }

    ac::Tile& At(int x, int y) { return *map.GetTile(x, y); }
};

// WorldFixture plus the building/pop-type registries and the ability to found real bases.
// Bases are created with the minimum viable dependency set: no research/production/composition
// calculators. Each base starts with 3 default Worker pops (BaseManager's built-in initial size)
// and registers the "Base" improvement on its tile, exactly as in the game.
struct BaseFixture : WorldFixture
{
    ac::GameDataContext dataContext;
    ac::EconomyManager economy; // default 40/50/10 energy split
    std::vector<std::unique_ptr<ac::BaseManager>> bases;

    BaseFixture()
    {
        dataContext.buildingRegistry = std::make_unique<ac::BuildingRegistry>();
        dataContext.buildingRegistry->Load(FixturePath("buildings.json"));
        dataContext.popTypeRegistry = std::make_unique<ac::PopTypeRegistry>();
        dataContext.popTypeRegistry->Load(FixturePath("pop_types.json"));
        dataContext.growthConfig = std::make_unique<ac::GrowthConfig_t>();
    }

    ac::BuildingRegistry& buildings() { return *dataContext.buildingRegistry; }
    const ac::BuildingRegistry& buildings() const { return *dataContext.buildingRegistry; }
    ac::PopTypeRegistry& popTypes() { return *dataContext.popTypeRegistry; }
    const ac::PopTypeRegistry& popTypes() const { return *dataContext.popTypeRegistry; }

    ac::BaseManager& MakeBase(int x, int y)
    {
        bases.push_back(std::make_unique<ac::BaseManager>(
            At(x, y), dataContext, *ctx, /*research*/ nullptr, &economy));
        return *bases.back();
    }
};

// BaseFixture plus social policy/rating registries, real Factions, and unit designs — for
// the universal-routing lanes (FactionUnits, WorldGlobal, two-level ratings, unit auras).
// Bases made through MakeFactionBase are owned by their faction and get the rating registry,
// so per-base social rating expansion is active.
struct FactionFixture : BaseFixture
{
    std::vector<std::unique_ptr<ac::Faction>> factions;
    std::deque<ac::UnitDesign> designs; // deque: Units hold UnitDesign& references

    FactionFixture()
    {
        dataContext.socialPolicyRegistry = std::make_unique<ac::SocialPolicyRegistry>();
        dataContext.socialPolicyRegistry->Load(FixturePath("social_policies.json"));
        dataContext.socialRatingRegistry = std::make_unique<ac::SocialRatingRegistry>();
        dataContext.socialRatingRegistry->Load(FixturePath("social_rating_effects.json"));
    }

    ac::SocialPolicyRegistry& socialPolicies() { return *dataContext.socialPolicyRegistry; }
    const ac::SocialPolicyRegistry& socialPolicies() const { return *dataContext.socialPolicyRegistry; }
    ac::SocialRatingRegistry& socialRatings() { return *dataContext.socialRatingRegistry; }
    const ac::SocialRatingRegistry& socialRatings() const { return *dataContext.socialRatingRegistry; }

    ac::Faction& MakeFaction()
    {
        factions.push_back(std::make_unique<ac::Faction>(
            dataContext.buildingRegistry.get(), /*techRegistry*/ nullptr,
            dataContext.socialPolicyRegistry.get(), dataContext.socialRatingRegistry.get(),
            /*techCost*/ nullptr, /*popTypeAvailability*/ nullptr));
        return *factions.back();
    }

    ac::BaseManager& MakeFactionBase(ac::Faction& rFaction, int x, int y)
    {
        auto pBase = std::make_unique<ac::BaseManager>(
            At(x, y), dataContext, *ctx, nullptr, &economy);
        ac::BaseManager& rBase = *pBase;
        rFaction.AddBase(std::move(pBase));
        return rBase;
    }

    // Builds a design from fixture components (one synthetic slot per component) and
    // creates a live unit at (x, y), registered on the world map for aura/position queries.
    ac::Unit& MakeUnit(ac::Faction& rFaction, int x, int y,
                       const std::vector<std::string>& rComponentIds)
    {
        std::vector<ac::UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const ac::UnitComponentConfig_t*> assigned;
        int slotIndex = 0;
        for (const std::string& rId : rComponentIds)
        {
            const ac::UnitComponentConfig_t* pComponent = unitComponents.Find(rId);
            if (!pComponent)
            {
                throw std::runtime_error("Fixture component not found: " + rId);
            }
            ac::UnitSlotConfig_t slot;
            slot.id = "slot_" + std::to_string(slotIndex++);
            slot.displayName = slot.id;
            slot.componentType = pComponent->type;
            slot.required = true;
            assigned[slot.id] = pComponent;
            slots.push_back(slot);
        }
        designs.emplace_back(slots, assigned);

        ac::Unit& rUnit = rFaction.GetUnitManager().CreateUnit(designs.back(), At(x, y), nullptr);
        map.OnUnitPlaced(rUnit, At(x, y));
        return rUnit;
    }

    void MoveUnit(ac::Unit& rUnit, int x, int y)
    {
        // Order matters: the position index removes the unit from its CURRENT tile,
        // so the map must be told before the unit's tile pointer changes.
        map.OnUnitMoved(rUnit, At(x, y));
        rUnit.SetTile(At(x, y));
    }
};

} // namespace actest
