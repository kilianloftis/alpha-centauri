// Package 4 — composition root, dependency validity, session boundaries.
//
// The invariants here are mostly enforced by signatures (a dependency the composition root
// always supplies is a constructor reference, so "absent" is unrepresentable and there is
// nothing to test at runtime). What remains testable is the seam where that guarantee is
// established — GameDataContext completeness — and the two behaviours the old
// construct-then-wire sequence got wrong: silent no-op visibility and the load-bearing
// AddFaction ordering.

#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/stockpiles/StockpileRegistry.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"
#include "game/units/MoraleCalculator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <memory>

using namespace ac;
using namespace actest;

// The fixtures assemble a narrower context by hand, so nothing else here parses the config
// the game actually ships. Without this, a malformed or missing file under config/ — a new
// registry wired into LoadGameData but never given its json, say — first fails at startup.
TEST_CASE("LoadGameData parses the shipping config", "[composition][gamedata]")
{
    const std::filesystem::path repoRoot =
        std::filesystem::path(AC_TEST_FIXTURES_DIR) / ".." / "..";
    const std::filesystem::path previousDir = std::filesystem::current_path();
    std::filesystem::current_path(repoRoot);

    try
    {
        const GameDataContext data = LoadGameData();
        CHECK_NOTHROW(ThrowIfIncomplete(data));
        // Every stockpile the stock config ships must be selectable as the empty-queue
        // default, or bases silently waste their surplus.
        CHECK(data.stockpileRegistry->FindFallback({}) != nullptr);
    }
    catch (...)
    {
        std::filesystem::current_path(previousDir);
        throw;
    }
    std::filesystem::current_path(previousDir);
}

TEST_CASE("ThrowIfIncomplete names the missing member", "[composition][gamedata]")
{
    // A default-constructed context is the old "service locator" state: every member null.
    // It must not be silently usable — and the diagnostic has to name a field, because
    // "something was null" is not something a modder can act on.
    GameDataContext empty;
    CHECK_THROWS_WITH(ThrowIfIncomplete(empty),
                      Catch::Matchers::ContainsSubstring("buildingRegistry"));
}

TEST_CASE("ThrowIfIncomplete reports the first member still missing", "[composition][gamedata]")
{
    // Filling one member moves the complaint to the next, so the check walks the whole set
    // rather than guarding a single field. (This is the loader's contract. Test fixtures
    // deliberately assemble a narrower context — only what Faction and BaseManager need — and
    // do not run this check; the reference-typed constructors are what stop a fixture from
    // building a half-valid object out of it.)
    GameDataContext data;
    CHECK_THROWS_WITH(ThrowIfIncomplete(data),
                      Catch::Matchers::ContainsSubstring("buildingRegistry"));

    data.buildingRegistry = std::make_unique<BuildingRegistry>();
    CHECK_THROWS_WITH(ThrowIfIncomplete(data),
                      Catch::Matchers::ContainsSubstring("stockpileRegistry"));

    data.stockpileRegistry = std::make_unique<StockpileRegistry>();
    CHECK_THROWS_WITH(ThrowIfIncomplete(data),
                      Catch::Matchers::ContainsSubstring("unitComponentRegistry"));
}

TEST_CASE("A newly constructed faction already has sized fog maps", "[composition][faction]")
{
    // The world map is a constructor dependency, so there is no window between construction
    // and BindWorldMap in which RebuildVisibility silently did nothing. The faction's own
    // base tile is visible without any explicit wiring step.
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();

    CHECK(&faction.GetWorldMap() == &fixture.map);
    // Explored/visible maps are sized from the map, not left at 0x0: an in-bounds query is
    // answerable rather than out of range.
    CHECK_NOTHROW(faction.GetExploredMap().IsExplored(fixture.map.GetWidth() - 1,
                                                      fixture.map.GetHeight() - 1));

    BaseManager& base = fixture.MakeFactionBase(faction, 4, 4);
    CHECK(faction.GetVisibleMap().IsVisible(base.GetTile().GetX(), base.GetTile().GetY()));
}

TEST_CASE("AddFaction scans a faction that already owns bases", "[composition][faction]")
{
    // The ordering bug: AddFaction used to wire observers and rebuild visibility *before*
    // pushing the faction into m_factions, so a faction arriving already populated (load-game,
    // or any runtime creation) was never folded into territory. Now registration happens
    // first and AttachToSession_ ends with a catch-up sweep.
    WorldFixture fixture;
    GameSettings settings;
    GameState state(std::make_unique<WorldMap>(9, 9), fixture.improvements,
                    &fixture.unitComponents, settings, *fixture.dataContext.moraleCalculator,
                    k_TestRngSeed);

    FactionConfig_t definition;
    definition.id = "already_populated";
    auto pFaction = std::make_unique<Faction>(
        state.AllocateFactionId(), true, definition, fixture.dataContext,
        state.GetWorldMap(), settings, k_TestFactionSeed);

    // Found the base BEFORE the faction is known to the session — the case that used to be
    // skipped entirely.
    BaseManager* pBase = pFaction->CreateBase(
        state.AllocateBaseId(), "PreExisting", state.GetWorldMap().GetTile(4, 4),
        fixture.dataContext, state.GetTileEffects(), state.GetSecretProjectAvailability());
    REQUIRE(pBase != nullptr);

    Faction& rAdded = state.AddFaction(std::move(pFaction));

    // Territory covers the pre-existing base's tile, and vision was rebuilt for it.
    CHECK(state.GetWorldMap().GetTerritory().GetOwner(pBase->GetTile())
          == rAdded.GetFactionId());
    CHECK(rAdded.GetVisibleMap().IsVisible(4, 4));
}

TEST_CASE("AddFaction establishes contact in both directions", "[composition][faction]")
{
    // Contact is symmetric, but the catch-up sweep used to run RebuildVisibility on the
    // *newcomer only* — which asks "what can the newcomer see", never "who can already see the
    // newcomer". An incumbent whose vision covers the arriving faction's base stayed unaware of
    // it until some unrelated later event.
    WorldFixture fixture;
    GameSettings settings;
    GameState state(std::make_unique<WorldMap>(9, 9), fixture.improvements,
                    &fixture.unitComponents, settings, *fixture.dataContext.moraleCalculator,
                    k_TestRngSeed);

    // The sighting must be one-way, or the newcomer's own sweep would establish contact and
    // the test would pass either way. A Sensor (vision 2, territory-owned) extends the
    // incumbent's sight to (6,4) from a base at (2,4) that is itself 4 tiles away — outside
    // the newcomer base's own radius of 2, so the newcomer sees no incumbent base or unit.
    FactionConfig_t incumbentDef;
    incumbentDef.id = "incumbent";
    Faction& rIncumbent = state.AddFaction(std::make_unique<Faction>(
        state.AllocateFactionId(), true, incumbentDef, fixture.dataContext,
        state.GetWorldMap(), settings, k_TestFactionSeed));
    rIncumbent.CreateBase(state.AllocateBaseId(), "Incumbent", state.GetWorldMap().GetTile(2, 4),
                          fixture.dataContext, state.GetTileEffects(),
                          state.GetSecretProjectAvailability());
    state.GetTileEffects().AddImprovementWithEffects(*state.GetWorldMap().GetTile(4, 4), "Sensor");
    rIncumbent.RebuildVisibility();

    FactionConfig_t arrivalDef;
    arrivalDef.id = "arrival";
    auto pArrival = std::make_unique<Faction>(
        state.AllocateFactionId(), false, arrivalDef, fixture.dataContext,
        state.GetWorldMap(), settings, k_TestFactionSeed + 1);
    pArrival->CreateBase(state.AllocateBaseId(), "Arrival", state.GetWorldMap().GetTile(6, 4),
                         fixture.dataContext, state.GetTileEffects(),
                         state.GetSecretProjectAvailability());

    // Precondition: the sighting really is one-way.
    REQUIRE(rIncumbent.GetVisibleMap().IsVisible(6, 4));
    REQUIRE_FALSE(pArrival->GetVisibleMap().IsVisible(2, 4));

    Faction& rArrival = state.AddFaction(std::move(pArrival));

    CHECK(state.GetDiplomacyLedger().AreKnown(rIncumbent.GetFactionId(),
                                              rArrival.GetFactionId()));
}

TEST_CASE("Faction random choices are reproducible from the seed", "[composition][faction][rng]")
{
    // Base names and the starting research target used to come from std::random_device inside
    // FactionFlavor / ResearchSelector, so the same session could not be replayed — and any
    // test touching research became order-dependent. Same seed must mean same picks.
    FactionFixture fixture;
    FactionConfig_t definition = fixture.factionDefinition;
    definition.id = "seeded";

    auto make = [&](uint32_t seed) {
        return std::make_unique<Faction>(99, false, definition, fixture.dataContext,
                                         fixture.map, fixture.settings, seed);
    };

    auto pA = make(4242u);
    auto pB = make(4242u);
    CHECK(pA->SuggestBaseName() == pB->SuggestBaseName());
    CHECK(pA->GetResearch().GetResearchTarget() == pB->GetResearch().GetResearchTarget());
}
