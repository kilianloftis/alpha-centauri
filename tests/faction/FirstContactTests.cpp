#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/FirstContactResolver.h"
#include "game/faction/UnitVisibility.h"
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
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pA = nullptr;
    Faction* pB = nullptr;

    ContactGame_()
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
            fixtures.dataContext, fixtures.map, fixtures.settings, actest::k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition,
            fixtures.dataContext, fixtures.map, fixtures.settings, actest::k_TestFactionSeed);

        pA = &pState->AddFaction(std::move(pFactionA));
        pB = &pState->AddFaction(std::move(pFactionB));
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

        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return rFaction.GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), fixtures.designs.back(),
            pState->GetWorldMap().GetUnitPositions(), *pTile,
            nullptr);
    }
};

} // namespace

TEST_CASE("Vision of a foreign unit establishes Known", "[diplomacy][first-contact]")
{
    ContactGame_ game;
    CHECK_FALSE(game.pState->GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));

    // Place B's unit next to A's unit so A's vision covers it after create rebuild.
    game.MakeUnit(*game.pA, 4, 4);
    game.MakeUnit(*game.pB, 5, 4);

    CHECK(game.pState->GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}

TEST_CASE("Moving into foreign vision establishes Known", "[diplomacy][first-contact]")
{
    ContactGame_ game;
    Unit& rA = game.MakeUnit(*game.pA, 1, 1);
    game.MakeUnit(*game.pB, 7, 7);
    // Far apart: not known yet if vision radius is small.
    // Force clear known in case create-order already contacted (adjacent vision disks).
    game.pState->GetDiplomacyLedger().SetKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId(), false);

    // Move A next to B; OnUnitMoved rebuilds A's vision and checks B seeing A.
    Tile* pNear = game.pState->GetWorldMap().GetTile(6, 7);
    REQUIRE(pNear);
    game.pState->GetWorldMap().GetUnitPositions().MoveUnit(rA, *pNear);

    CHECK(game.pState->GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}

TEST_CASE("Seeing a foreign base establishes Known", "[diplomacy][first-contact]")
{
    ContactGame_ game;
    game.MakeUnit(*game.pA, 4, 4);
    game.pState->GetDiplomacyLedger().SetKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId(), false);

    game.pB->CreateBase(
        game.pState->AllocateBaseId(), "Enemy",
        game.pState->GetWorldMap().GetTile(5, 4),
        game.fixtures.dataContext,
        game.pState->GetTileEffects(),
        game.pState->GetSecretProjectAvailability());

    CHECK(game.pState->GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}

TEST_CASE("Cloaked unit on visible map still establishes Known",
          "[diplomacy][first-contact][conceal]")
{
    ContactGame_ game;
    game.MakeUnit(*game.pA, 4, 4);
    game.pState->GetDiplomacyLedger().SetKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId(), false);

    // Cloaking hides the unit for combat/UI, but the tile is still in A's vision.
    Unit& rCloaked = game.MakeUnit(*game.pB, 5, 4, {"test_chassis", "Cloaking_Device"});

    REQUIRE(game.pA->GetVisibleMap().IsVisible(rCloaked.GetTile()));
    REQUIRE_FALSE(IsUnitVisibleTo(*game.pA, rCloaked, game.pState->GetTileEffects()));
    CHECK(game.pState->GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}

TEST_CASE("Already known is a no-op", "[diplomacy][first-contact]")
{
    ContactGame_ game;
    game.pState->GetDiplomacyLedger().SetKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId());
    game.MakeUnit(*game.pA, 4, 4);
    game.MakeUnit(*game.pB, 5, 4);
    CHECK(game.pState->GetDiplomacyLedger().AreKnown(
        game.pA->GetFactionId(), game.pB->GetFactionId()));
}
