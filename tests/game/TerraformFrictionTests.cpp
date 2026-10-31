#include "DiplomacyFixture.h"

#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticStandingMutations.h"
#include "game/base/BaseManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/MapUtils.h"
#include "game/map/TerrainOperationRegistry.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/map/Tile.h"
#include "game/terraform/TerraformRules.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <random>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

int StopDistance_(DiplomacyFixture& rGame)
{
    return rGame.fixtures.state.GetGameData().diplomacyConfig->terraformRainfallStopDistance;
}

int Distance_(DiplomacyFixture& rGame, const Tile& rA, const Tile& rB)
{
    return TabletopDiagonalDistance(rA, rB, rGame.Map().GetWidth());
}

// A change whose tile is now `after`, having been `before`.
MoistureChange_t Change_(Tile& rTile, Moisture_t before, Moisture_t after)
{
    rTile.SetBaseMoisture(after);
    return MoistureChange_t{&rTile, before};
}

// A tile nearest B's base (11, 11), far from A's (5, 5).
Tile& NearB_(DiplomacyFixture& rGame)
{
    Tile& rTile = rGame.At(14, 12);
    REQUIRE(rGame.fixtures.state.FindNearestBase(rTile) == rGame.pBaseB);
    REQUIRE(Distance_(rGame, rTile, rGame.pBaseA->GetTile()) > StopDistance_(rGame));
    return rTile;
}

} // namespace

TEST_CASE("A player's raise or lower land that dries an AI's land aggrieves that AI",
          "[terraform][diplomacy][standing]")
{
    DiplomacyFixture game;
    const std::vector<MoistureChange_t> changes{
        Change_(NearB_(game), Moisture_t::Moist, Moisture_t::Arid)};

    CHECK(RainfallGrievant(*game.pA, changes) == game.pB->GetFactionId());

    ApplyRainfallGrievance(game.fixtures.state, *game.pA, changes);
    CHECK(game.Ledger().GetFriction(game.pB->GetFactionId(), game.pA->GetFactionId())
          == game.fixtures.state.GetGameData().diplomacyConfig->terraformRainfallFriction);
}

TEST_CASE("Only a drier tile names a grievant", "[terraform][diplomacy][standing]")
{
    DiplomacyFixture game;
    const std::vector<MoistureChange_t> changes{
        Change_(NearB_(game), Moisture_t::Moist, Moisture_t::Wet)};

    CHECK_FALSE(RainfallGrievant(*game.pA, changes));
}

TEST_CASE("The last drier AI tile wins, but a player owner sticks",
          "[terraform][diplomacy][standing]")
{
    DiplomacyFixture game;
    MakeSessionBase(game.fixtures, *game.pC, 26, 4);
    Tile& rNearC = game.At(28, 4);
    REQUIRE(game.fixtures.state.FindNearestBase(rNearC)->GetFactionId()
            == game.pC->GetFactionId());
    REQUIRE(Distance_(game, rNearC, game.pBaseA->GetTile()) > StopDistance_(game));

    SECTION("AI then AI")
    {
        const std::vector<MoistureChange_t> changes{
            Change_(rNearC, Moisture_t::Wet, Moisture_t::Moist),
            Change_(NearB_(game), Moisture_t::Moist, Moisture_t::Arid)};
        CHECK(RainfallGrievant(*game.pA, changes) == game.pB->GetFactionId());
    }
    SECTION("player then AI")
    {
        Tile& rNearA = game.At(5, 9);
        REQUIRE(game.fixtures.state.FindNearestBase(rNearA) == game.pBaseA);
        REQUIRE(Distance_(game, rNearA, game.pBaseA->GetTile()) > StopDistance_(game));
        const std::vector<MoistureChange_t> changes{
            Change_(rNearA, Moisture_t::Moist, Moisture_t::Arid),
            Change_(NearB_(game), Moisture_t::Moist, Moisture_t::Arid)};
        CHECK_FALSE(RainfallGrievant(*game.pA, changes));
    }
}

TEST_CASE("A change near the terraformer's base ends the scan", "[terraform][diplomacy][standing]")
{
    DiplomacyFixture game;
    Tile& rNearHome = game.At(6, 6);
    REQUIRE(Distance_(game, rNearHome, game.pBaseA->GetTile()) <= StopDistance_(game));
    const std::vector<MoistureChange_t> changes{
        Change_(rNearHome, Moisture_t::Moist, Moisture_t::Wet),
        Change_(NearB_(game), Moisture_t::Moist, Moisture_t::Arid)};

    CHECK_FALSE(RainfallGrievant(*game.pA, changes));
}

TEST_CASE("No rainfall grievance between Pact partners, at Vendetta, or from an AI terraformer",
          "[terraform][diplomacy][standing]")
{
    DiplomacyFixture game;
    const std::vector<MoistureChange_t> changes{
        Change_(NearB_(game), Moisture_t::Moist, Moisture_t::Arid)};
    const FactionId_t a = game.pA->GetFactionId();
    const FactionId_t b = game.pB->GetFactionId();

    game.Ledger().SetStatus(a, b, DiplomaticStatus_t::Treaty);
    CHECK(RainfallGrievant(*game.pA, changes) == b);
    game.Ledger().SetStatus(a, b, DiplomaticStatus_t::Pact);
    CHECK_FALSE(RainfallGrievant(*game.pA, changes));
    game.Ledger().SetStatus(a, b, DiplomaticStatus_t::Vendetta);
    CHECK_FALSE(RainfallGrievant(*game.pA, changes));

    game.Ledger().SetStatus(a, b, DiplomaticStatus_t::Neutral);
    CHECK_FALSE(RainfallGrievant(*game.pC, changes));
}

TEST_CASE("A player Former lowering a ridge that dries an AI's land adds friction",
          "[terraform][diplomacy][standing][moisture]")
{
    DiplomacyFixture game;
    for (const std::unique_ptr<Tile>& pTile : game.Map().GetTiles())
    {
        pTile->SetElevation(500);
    }
    // The ridge's windward rain belt runs west over B's land; lowering it dries that belt.
    Tile& rRidge = game.At(18, 8);
    rRidge.SetElevation(2000);
    game.fixtures.state.GetTileEffects().RecomputeClimate();
    REQUIRE(game.At(14, 8).GetBaseMoisture() == Moisture_t::Wet);
    REQUIRE(game.fixtures.state.FindNearestBase(game.At(14, 8)) == game.pBaseB);

    Unit& rFormer = game.MakeUnit(*game.pA, 18, 8);
    const std::optional<TerraformProject_t> project = FindTerraformProject(
        "LowerLand", game.fixtures.state.GetTileEffects().GetImprovements(),
        *game.fixtures.state.GetGameData().terrainOperationRegistry);
    REQUIRE(project);
    std::mt19937 rng(1);
    REQUIRE(game.fixtures.state.ApplyTerraformCompleteEffects(rRidge, *project, rFormer, rng));

    CHECK(game.At(14, 8).GetBaseMoisture() < Moisture_t::Wet);
    CHECK(game.Ledger().GetFriction(game.pB->GetFactionId(), game.pA->GetFactionId())
          == game.fixtures.state.GetGameData().diplomacyConfig->terraformRainfallFriction);
}
