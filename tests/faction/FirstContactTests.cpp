#include "GameFixtures.h"

#include "game/CurrentGameState.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/FirstContactResolver.h"
#include "game/visibility/VisibilityRules.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

struct ContactGame_
{
    FactionFixture fixtures;
    Faction* pA = nullptr;
    Faction* pB = nullptr;

    ContactGame_()
    {
        FillLand(fixtures.map);

        auto pFactionA = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), true, fixtures.factionDefinition,
            fixtures.dataContext, fixtures.state.GetWorldMap(), fixtures.settings,
            actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), false, fixtures.factionDefinition,
            fixtures.dataContext, fixtures.state.GetWorldMap(), fixtures.settings,
            actest::k_TestFactionSeed);

        pA = &fixtures.state.AddFaction(std::move(pFactionA));
        pB = &fixtures.state.AddFaction(std::move(pFactionB));
    }

    Unit& MakeUnit(Faction& rFaction, int x, int y,
                   const std::vector<std::string>& rComponentIds = {"test_chassis"})
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

        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return rFaction.GetUnitManager().CreateUnit(
            fixtures.state.AllocateUnitId(), fixtures.designs.back(),
            fixtures.state.GetWorldMap().GetUnitPositions(), *pTile,
            nullptr);
    }
};

} // namespace

TEST_CASE("Vision of a foreign unit establishes Known", "[diplomacy][first-contact]")
{
    ContactGame_ game;
    CHECK_FALSE(game.fixtures.state.GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));

    // Place B's unit next to A's unit so A's vision covers it after create rebuild.
    game.MakeUnit(*game.pA, 8, 8);
    game.MakeUnit(*game.pB, 9, 9);

    CHECK(game.fixtures.state.GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}

TEST_CASE("Moving into foreign vision establishes Known", "[diplomacy][first-contact]")
{
    ContactGame_ game;
    Unit& rA = game.MakeUnit(*game.pA, 8, 2);
    game.MakeUnit(*game.pB, 8, 14);
    // Far apart: not known yet if vision radius is small.
    // Force clear known in case create-order already contacted (adjacent vision disks).
    game.fixtures.state.GetDiplomacyLedger().SetKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId(), false);

    // Move A next to B; OnUnitMoved rebuilds A's vision and checks B seeing A.
    Tile* pNear = game.fixtures.state.GetWorldMap().GetTile(7, 13);
    REQUIRE(pNear);
    game.fixtures.state.GetWorldMap().GetUnitPositions().MoveUnit(rA, *pNear);

    CHECK(game.fixtures.state.GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}

TEST_CASE("Seeing a foreign base establishes Known", "[diplomacy][first-contact]")
{
    ContactGame_ game;
    game.MakeUnit(*game.pA, 8, 8);
    game.fixtures.state.GetDiplomacyLedger().SetKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId(), false);

    game.pB->CreateBase(
        game.fixtures.state.AllocateBaseId(), "Enemy",
        game.fixtures.state.GetWorldMap().GetTile(9, 9),
        game.fixtures.state.GetTileEffects());

    CHECK(game.fixtures.state.GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}

TEST_CASE("Cloaked unit on visible map still establishes Known",
          "[diplomacy][first-contact][conceal]")
{
    ContactGame_ game;
    game.MakeUnit(*game.pA, 8, 8);
    game.fixtures.state.GetDiplomacyLedger().SetKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId(), false);

    // Cloaking hides the unit for combat/UI, but the tile is still in A's vision.
    Unit& rCloaked = game.MakeUnit(*game.pB, 9, 9, {"test_chassis", "Cloaking_Device"});

    REQUIRE(game.pA->GetVisibleMap().IsVisible(rCloaked.GetTile()));
    REQUIRE_FALSE(IsUnitVisibleTo(*game.pA, rCloaked));
    CHECK(game.fixtures.state.GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}

TEST_CASE("Already known is a no-op", "[diplomacy][first-contact]")
{
    ContactGame_ game;
    game.fixtures.state.GetDiplomacyLedger().SetKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId());
    game.MakeUnit(*game.pA, 8, 8);
    game.MakeUnit(*game.pB, 9, 9);
    CHECK(game.fixtures.state.GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}

TEST_CASE("Seeing native life establishes no contact", "[diplomacy][first-contact]")
{
    ContactGame_ game;
    Faction& rPlanet = AddNativeLifeFaction(game.fixtures);

    // Each side arrives in the other's vision once.
    game.MakeUnit(*game.pA, 8, 8);
    game.MakeUnit(rPlanet, 9, 9);
    game.MakeUnit(rPlanet, 8, 2);
    game.MakeUnit(*game.pA, 9, 3);

    CHECK_FALSE(game.fixtures.state.GetDiplomacyLedger().AreKnown(game.pA->GetFactionId(),
                                                          rPlanet.GetFactionId()));
}

TEST_CASE("Seeing a covert unit establishes no contact", "[diplomacy][first-contact][covert]")
{
    ContactGame_ game;
    // A's base sees two tiles out; B's units see one, so they never see A back.
    game.pA->CreateBase(
        game.fixtures.state.AllocateBaseId(), "Watch",
        game.fixtures.state.GetWorldMap().GetTile(8, 8),
        game.fixtures.state.GetTileEffects());

    Unit& rCovert = game.MakeUnit(*game.pB, 10, 10, {"test_chassis", "covert"});

    REQUIRE(game.pA->GetVisibleMap().IsVisible(rCovert.GetTile()));
    CHECK_FALSE(game.fixtures.state.GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));

    game.MakeUnit(*game.pB, 10, 10);

    CHECK(game.fixtures.state.GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}
