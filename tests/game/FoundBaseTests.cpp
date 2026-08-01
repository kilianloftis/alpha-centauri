#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/FoundBaseRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

void RebuildTerritory_(FactionFixture& rFixture)
{
    std::vector<const BaseManager*> bases;
    for (const auto& pFaction : rFixture.factions)
    {
        for (const BaseManager& rBase : pFaction->Bases())
        {
            bases.push_back(&rBase);
        }
    }
    rFixture.map.GetTerritory().Rebuild(rFixture.map, bases);
}

std::vector<const BaseManager*> AllBases_(FactionFixture& rFixture)
{
    std::vector<const BaseManager*> bases;
    for (const auto& pFaction : rFixture.factions)
    {
        for (const BaseManager& rBase : pFaction->Bases())
        {
            bases.push_back(&rBase);
        }
    }
    return bases;
}

size_t CountUnits_(Faction& rFaction)
{
    return static_cast<size_t>(std::ranges::distance(rFaction.GetUnitManager().Units()));
}

struct FoundBaseGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    Faction* pAi = nullptr;

    FoundBaseGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator);

        auto pFactionA = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition,
            fixtures.dataContext);
        auto pFactionB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition,
            fixtures.dataContext);

        pPlayer = &pState->AddFaction(std::move(pFactionA));
        pAi = &pState->AddFaction(std::move(pFactionB));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            pState->AllocateBaseId(), "TestBase", pTile, fixtures.dataContext,
            pState->GetTileEffects(), pState->GetSecretProjectAvailability());
        REQUIRE(pBase);
        return *pBase;
    }

    Unit& MakeUnit(Faction& rFaction, int x, int y,
                   const std::vector<std::string>& rComponentIds,
                   BaseManager* pHomeBase = nullptr)
    {
        std::vector<UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
        int slotIndex = 0;
        for (const std::string& rId : rComponentIds)
        {
            const UnitComponentConfig_t* pComponent = fixtures.unitComponents.Find(rId);
            REQUIRE(pComponent);
            UnitSlotConfig_t slot;
            slot.id = "slot_" + std::to_string(slotIndex++);
            slot.displayName = slot.id;
            slot.componentType = pComponent->type;
            slot.required = true;
            assigned[slot.id] = pComponent;
            slots.push_back(slot);
        }
        fixtures.designs.emplace_back(slots, assigned);

        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return rFaction.GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), fixtures.designs.back(),
            pState->GetWorldMap().GetUnitPositions(), *pTile,
            pHomeBase);
    }
};

} // namespace

TEST_CASE("Founding is illegal within 2 tiles of an existing base", "[unit][found-base]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    fixture.MakeFactionBase(faction, 4, 4);
    RebuildTerritory_(fixture);

    const auto bases = AllBases_(fixture);
    CHECK_FALSE(CanFoundBaseAt(fixture.At(4, 4), faction.GetFactionId(), fixture.map, bases)); // 0
    CHECK_FALSE(CanFoundBaseAt(fixture.At(5, 4), faction.GetFactionId(), fixture.map, bases)); // 1
    CHECK_FALSE(CanFoundBaseAt(fixture.At(6, 4), faction.GetFactionId(), fixture.map, bases)); // 2
    CHECK(CanFoundBaseAt(fixture.At(7, 4), faction.GetFactionId(), fixture.map, bases));       // 3
}

TEST_CASE("Founding is illegal in another faction's territory", "[unit][found-base]")
{
    FactionFixture fixture;
    Faction& owner = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();
    fixture.MakeFactionBase(owner, 4, 4);
    RebuildTerritory_(fixture);

    REQUIRE(fixture.map.GetTerritory().GetOwner(5, 4) == owner.GetFactionId());

    // Far enough from the base for spacing, but still in owner's disk.
    Tile& inOwnerTerritory = fixture.At(4, 0);
    REQUIRE(ChebyshevDistance(fixture.At(4, 4), inOwnerTerritory, fixture.map.GetWidth()) >= 3);
    REQUIRE(fixture.map.GetTerritory().GetOwner(inOwnerTerritory) == owner.GetFactionId());

    CHECK_FALSE(CanFoundBaseAt(inOwnerTerritory, other.GetFactionId(), fixture.map, AllBases_(fixture)));
    CHECK(CanFoundBaseAt(inOwnerTerritory, owner.GetFactionId(), fixture.map, AllBases_(fixture)));
}

TEST_CASE("TryFoundBase creates a base; SingleUse expends the colony pod", "[unit][found-base]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);
    Unit& pod = game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis", "test_colony_pod"}, &home);
    REQUIRE(pod.GetFlag(RuleFlagId_t::FoundBase));
    REQUIRE(pod.GetFlag(RuleFlagId_t::SingleUse));
    REQUIRE(game.pPlayer->GetBaseCount() == 1);
    REQUIRE(CountUnits_(*game.pPlayer) == 1);

    BaseManager* pNew = game.pState->GetUnitOrderExecutor().TryFoundBase(
        pod, *game.pState, game.fixtures.dataContext);
    REQUIRE(pNew);
    CHECK(pNew->GetTile().GetX() == 7);
    CHECK(pNew->GetTile().GetY() == 4);
    CHECK(game.pPlayer->GetBaseCount() == 2);
    CHECK(CountUnits_(*game.pPlayer) == 0);
    CHECK(game.pState->FindBaseAt(7, 4) == pNew);
}

TEST_CASE("TryFoundBase without SingleUse leaves the unit alive", "[unit][found-base]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);
    Unit& pod = game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis", "test_found_base_only"}, &home);
    REQUIRE(pod.GetFlag(RuleFlagId_t::FoundBase));
    REQUIRE_FALSE(pod.GetFlag(RuleFlagId_t::SingleUse));

    BaseManager* pNew = game.pState->GetUnitOrderExecutor().TryFoundBase(
        pod, *game.pState, game.fixtures.dataContext);
    REQUIRE(pNew);
    CHECK(game.pPlayer->GetBaseCount() == 2);
    CHECK(CountUnits_(*game.pPlayer) == 1);
}

TEST_CASE("TryFoundBase fails without FoundBase or on an illegal tile", "[unit][found-base]")
{
    FoundBaseGame_ game;
    BaseManager& home = game.MakeBase(*game.pPlayer, 4, 4);

    Unit& scout = game.MakeUnit(*game.pPlayer, 7, 4, {"test_chassis"}, &home);
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryFoundBase(
        scout, *game.pState, game.fixtures.dataContext));
    CHECK(game.pPlayer->GetBaseCount() == 1);
    CHECK(CountUnits_(*game.pPlayer) == 1);

    Unit& tooClose = game.MakeUnit(*game.pPlayer, 5, 4, {"test_chassis", "test_colony_pod"}, &home);
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryFoundBase(
        tooClose, *game.pState, game.fixtures.dataContext));
    CHECK(game.pPlayer->GetBaseCount() == 1);
    CHECK(CountUnits_(*game.pPlayer) == 2);

    game.MakeBase(*game.pAi, 0, 0);
    Tile* pAiBaseTile = game.pState->GetWorldMap().GetTile(0, 0);
    Tile* pForeign = game.pState->GetWorldMap().GetTile(0, 3);
    REQUIRE(pAiBaseTile);
    REQUIRE(pForeign);
    REQUIRE(ChebyshevDistance(*pAiBaseTile, *pForeign, game.pState->GetWorldMap().GetWidth()) >= 3);
    REQUIRE(game.pState->GetWorldMap().GetTerritory().GetOwner(*pForeign) == game.pAi->GetFactionId());

    Unit& inForeign = game.MakeUnit(*game.pPlayer, 0, 3, {"test_chassis", "test_colony_pod"}, &home);
    CHECK_FALSE(game.pState->GetUnitOrderExecutor().TryFoundBase(
        inForeign, *game.pState, game.fixtures.dataContext));
    CHECK(game.pPlayer->GetBaseCount() == 1);
}
