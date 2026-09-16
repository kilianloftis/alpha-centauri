#include "GameFixtures.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/CommerceCalculator.h"
#include "game/faction/CommerceManager.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/CompositionInputs.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <memory>

using namespace ac;
using namespace actest;

namespace
{

struct CommerceGame_
{
    FactionFixture fixtures;
    GameSettings settings;
    std::unique_ptr<GameState> pState;
    Faction* pA = nullptr;
    Faction* pB = nullptr;

    CommerceGame_()
    {
        // techs.json plus two commerce_rating entries the denominator tests need. Loaded
        // here rather than added to the shared fixture, which several tests count.
        fixtures.dataContext.techRegistry->Load(FixturePath("techs_commerce.json"));

        auto pMap = std::make_unique<WorldMap>(9, 9);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules,
            fixtures.dataContext.interactionGrids, k_TestRngSeed);

        auto pFactionA = std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, k_TestFactionSeed);
        auto pFactionB = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, k_TestFactionSeed);

        pA = &pState->AddFaction(std::move(pFactionA));
        pB = &pState->AddFaction(std::move(pFactionB));
    }

    Faction& AddFaction()
    {
        auto pFaction = std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), settings, k_TestFactionSeed);
        return pState->AddFaction(std::move(pFaction));
    }

    void SetStatus(const Faction& rOther, DiplomaticStatus_t status)
    {
        pState->GetDiplomacyLedger().SetStatus(pA->GetFactionId(), rOther.GetFactionId(), status);
    }

    BaseManager& MakeHqBase(Faction& rFaction, int x, int y)
    {
        BaseManager& rBase = fixtures.MakeFactionBase(rFaction, x, y);
        rBase.GetBuildingManager().AddBuilding("Headquarters");
        rBase.GetBuildingManager().AddBuilding("world_beacon");
        return rBase;
    }
};

int ExpectedPairRaw_(int energyA, int energyB)
{
    return static_cast<int>(std::ceil(static_cast<double>(energyA + energyB) * 0.125));
}

} // namespace

TEST_CASE("ComputeForBase reports our and their energy for a paired partner", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const auto lines = CommerceCalculator{}.ComputeForBase(a1, *game.pState);
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].pPartner == game.pB);
    CHECK(lines[0].status == DiplomaticStatus_t::Pact);

    const int expectedOurs =
        ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    const int expectedTheirs =
        ExpectedPairRaw_(b1.GetEnergyProduction(), a1.GetEnergyProduction());
    CHECK(lines[0].ourEnergy == expectedOurs);
    CHECK(lines[0].theirEnergy == expectedTheirs);
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == expectedOurs);
}

TEST_CASE("Faction GetCommerce matches calculator for the same pair", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    game.MakeHqBase(*game.pB, 6, 2);

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const auto viaManager = game.pA->GetCommerce().ComputeForBase(a1);
    const auto viaCalculator = CommerceCalculator{}.ComputeForBase(a1, *game.pState);
    REQUIRE(viaManager.size() == 1);
    REQUIRE(viaCalculator.size() == 1);
    CHECK(viaManager[0].ourEnergy == viaCalculator[0].ourEnergy);
    CHECK(viaManager[0].theirEnergy == viaCalculator[0].theirEnergy);
}

TEST_CASE("ComputeForBase is empty for surplus bases and non-commerce treaties", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& a2 = game.MakeHqBase(*game.pA, 2, 6);
    game.MakeHqBase(*game.pB, 6, 2);
    a1.GetBuildingManager().AddBuilding("energy_tap");
    REQUIRE(a1.GetEnergyProduction() > a2.GetEnergyProduction());

    DiplomacyLedger& rDiplomacy = game.pState->GetDiplomacyLedger();
    rDiplomacy.SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);
    CHECK(CommerceCalculator{}.ComputeForBase(a2, *game.pState).empty());

    rDiplomacy.SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Truce);
    CHECK(CommerceCalculator{}.ComputeForBase(a1, *game.pState).empty());
}

TEST_CASE("Commerce pairs by pre-commerce energy; surplus bases ignored", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& a2 = game.MakeHqBase(*game.pA, 2, 6);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);

    a1.GetBuildingManager().AddBuilding("energy_tap");
    REQUIRE(a1.GetEnergyProduction() > a2.GetEnergyProduction());

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const auto commerce = CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState);
    REQUIRE(commerce.count(a1.GetBaseId()) == 1);
    CHECK(commerce.count(a2.GetBaseId()) == 0);

    const int expected = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    CHECK(commerce.at(a1.GetBaseId()) == expected);
}

TEST_CASE("Friendship applies treaty_multiplier; Pact does not", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);

    const int pairRaw = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    REQUIRE(pairRaw >= 2);

    DiplomacyLedger& rDiplomacy = game.pState->GetDiplomacyLedger();
    rDiplomacy.SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(),
                         DiplomaticStatus_t::Friendship);
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == static_cast<int>(std::floor(pairRaw * 0.5)));

    rDiplomacy.SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == pairRaw);
}

TEST_CASE("No commerce for Truce, None, or Vendetta", "[commerce]")
{
    CommerceGame_ game;
    game.MakeHqBase(*game.pA, 2, 2);
    game.MakeHqBase(*game.pB, 6, 2);

    DiplomacyLedger& rDiplomacy = game.pState->GetDiplomacyLedger();
    CommerceCalculator calc;

    rDiplomacy.SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Truce);
    CHECK(calc.ComputeForFaction(*game.pA, *game.pState).empty());

    rDiplomacy.SetStatus(game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::None);
    CHECK(calc.ComputeForFaction(*game.pA, *game.pState).empty());

    rDiplomacy.SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Vendetta);
    CHECK(calc.ComputeForFaction(*game.pA, *game.pState).empty());
}

TEST_CASE("CommerceRate doubles pair value when present", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);
    a1.GetBuildingManager().AddBuilding("commerce_rate_doubler");

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const int pairRaw = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == pairRaw * 2);
}

TEST_CASE("commerce_rating and economic techs feed the tech ratio; total ignores rating",
          "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);
    a1.GetBuildingManager().AddBuilding("commerce_rating_shrine"); // +2 CommerceRating

    game.pA->GetResearch().AddDiscoveredTech("industrial_automation"); // +1 economic tech
    game.pB->GetResearch().AddDiscoveredTech("planetary_economics");   // +1 economic tech (total)

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    // commerceTech = Resolve(CommerceRating) = shrine +2 + tech +1 = 3
    // totalCommerceTech = tech-only Adds across factions = 2
    // value = pairRaw * (3+1)/(2+1) = pairRaw * 4/3
    const int pairRaw = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == (pairRaw * 4) / 3);
}

TEST_CASE("CommerceEnergyBonus adds at the end of each pair", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);
    a1.GetBuildingManager().AddBuilding("commerce_energy_bonus_shrine");

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const int pairRaw = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction());
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == pairRaw + 1);
}

TEST_CASE("Commerce feeds ResourceManager raw energy before the econ split", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);

    const int commerce =
        CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId());
    REQUIRE(commerce > 0);

    const int econWithCommerce = a1.GetEconProduction();
    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::None);
    const int econWithout = a1.GetEconProduction();
    CHECK(econWithCommerce > econWithout);

    game.pState->GetDiplomacyLedger().SetStatus(
        game.pA->GetFactionId(), game.pB->GetFactionId(), DiplomaticStatus_t::Pact);
    const int treasuryBefore = game.pA->GetEconomy().GetEnergy();
    game.pA->ProduceBaseResources();
    game.pA->CollectIncome();
    CHECK(game.pA->GetEconomy().GetEnergy() == treasuryBefore + econWithCommerce);
    (void)b1;
}

TEST_CASE("Commerce is reciprocal: the partner earns it too, and the lines agree", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);
    // Asymmetric energy, so a line that silently used one side's figure for both would show.
    a1.GetBuildingManager().AddBuilding("energy_tap");

    game.SetStatus(*game.pB, DiplomaticStatus_t::Pact);

    const auto ours = CommerceCalculator{}.ComputeForBase(a1, *game.pState);
    const auto theirs = CommerceCalculator{}.ComputeForBase(b1, *game.pState);
    REQUIRE(ours.size() == 1);
    REQUIRE(theirs.size() == 1);

    CHECK(ours[0].ourEnergy > 0);
    CHECK(theirs[0].ourEnergy > 0);
    // Each side's view of the same pair must agree on who earns what.
    CHECK(ours[0].theirEnergy == theirs[0].ourEnergy);
    CHECK(theirs[0].theirEnergy == ours[0].ourEnergy);

    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pB, *game.pState).at(b1.GetBaseId())
          == theirs[0].ourEnergy);
}

TEST_CASE("A base accumulates one commerce line per eligible partner", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);
    Faction& rC = game.AddFaction();
    BaseManager& c1 = game.MakeHqBase(rC, 6, 6);

    game.SetStatus(*game.pB, DiplomaticStatus_t::Pact);
    game.SetStatus(rC, DiplomaticStatus_t::Pact);

    const auto lines = CommerceCalculator{}.ComputeForBase(a1, *game.pState);
    REQUIRE(lines.size() == 2);

    const int expected = ExpectedPairRaw_(a1.GetEnergyProduction(), b1.GetEnergyProduction())
                       + ExpectedPairRaw_(a1.GetEnergyProduction(), c1.GetEnergyProduction());
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == expected);

    // Dropping one treaty drops exactly that partner's line.
    game.SetStatus(rC, DiplomaticStatus_t::Truce);
    CHECK(CommerceCalculator{}.ComputeForBase(a1, *game.pState).size() == 1);
}

TEST_CASE("Commerce reaches labs and psych, not just econ", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    game.MakeHqBase(*game.pB, 6, 2);

    // An even three-way split, so every category visibly carries commerce.
    game.pA->GetEconomy().SetEnergyAllocation(EnergyAllocation_t{34, 33, 33});

    const int econBefore = a1.GetEconProduction();
    const int labsBefore = a1.GetLabsProduction();
    const int psychBefore = a1.GetPsychProduction();

    game.SetStatus(*game.pB, DiplomaticStatus_t::Pact);
    REQUIRE(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId()) > 0);

    CHECK(a1.GetEconProduction() > econBefore);
    CHECK(a1.GetLabsProduction() > labsBefore);
    CHECK(a1.GetPsychProduction() > psychBefore);
}

TEST_CASE("A negative commerce_rating tech cannot zero the tech denominator", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);

    // Planet-wide tech total of -1 would make the raw denominator (total + 1) zero.
    game.pA->GetResearch().AddDiscoveredTech("negative_commerce_tech");

    game.SetStatus(*game.pB, DiplomaticStatus_t::Pact);

    int value = 0;
    REQUIRE_NOTHROW(value = CommerceCalculator{}.ComputeForBase(a1, *game.pState).at(0).ourEnergy);
    // Clamped to 1: pairRaw * (rating + 1) / 1, and the base's own rating is -1 → 0.
    CHECK(value == 0);
    (void)b1;
}

TEST_CASE("A gated commerce_rating tech stays out of the planet-wide denominator", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    BaseManager& b1 = game.MakeHqBase(*game.pB, 6, 2);

    game.SetStatus(*game.pB, DiplomaticStatus_t::Pact);
    const int baseline = CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState)
                             .at(a1.GetBaseId());

    // +5 commerce_rating, but only for Command_Center — it resolves to nothing for anyone,
    // so counting it would lower every faction's commerce.
    game.pB->GetResearch().AddDiscoveredTech("gated_commerce_tech");
    CHECK(CommerceCalculator{}.ComputeForFaction(*game.pA, *game.pState).at(a1.GetBaseId())
          == baseline);
    (void)b1;
}

TEST_CASE("A base detached from its faction reports no commerce instead of throwing",
          "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    game.MakeHqBase(*game.pA, 2, 6);
    game.MakeHqBase(*game.pB, 6, 2);

    game.SetStatus(*game.pB, DiplomaticStatus_t::Pact);
    REQUIRE_FALSE(CommerceCalculator{}.ComputeForBase(a1, *game.pState).empty());

    // ReleaseBase leaves a live BaseManager that is no longer in Faction::Bases() — the same
    // momentarily-ownerless state CreateBaseFromSnapshot passes through.
    const BaseId_t detachedId = a1.GetBaseId();
    std::unique_ptr<BaseManager> pDetached = game.pA->ReleaseBase(detachedId);
    REQUIRE(pDetached != nullptr);

    CHECK(CommerceCalculator{}.ComputeForBase(*pDetached, *game.pState).empty());
    CHECK(game.pA->GetCommerce().ComputeForBase(*pDetached).empty());
    CHECK(pDetached->GetResources().GetCommercePartners().empty());
}

TEST_CASE("CommerceManager recomputes when a treaty changes", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    game.MakeHqBase(*game.pB, 6, 2);
    const CommerceManager& rCommerce = game.pA->GetCommerce();

    CHECK(rCommerce.GetCommerceEnergy(a1) == 0);

    game.SetStatus(*game.pB, DiplomaticStatus_t::Pact);
    const int pact = rCommerce.GetCommerceEnergy(a1);
    CHECK(pact > 0);

    game.SetStatus(*game.pB, DiplomaticStatus_t::Friendship);
    CHECK(rCommerce.GetCommerceEnergy(a1) < pact);

    game.SetStatus(*game.pB, DiplomaticStatus_t::Vendetta);
    CHECK(rCommerce.GetCommerceEnergy(a1) == 0);
}

TEST_CASE("A partner-side change reaches the composition input key", "[commerce]")
{
    CommerceGame_ game;
    BaseManager& a1 = game.MakeHqBase(*game.pA, 2, 2);
    game.MakeHqBase(*game.pB, 6, 2);
    game.pA->GetEconomy().SetEnergyAllocation(EnergyAllocation_t{34, 33, 33});

    const CompositionInputKey_t before = ReadCompositionInputKey(a1);

    game.SetStatus(*game.pB, DiplomaticStatus_t::Pact);

    // The key is read through GetPsychProduction, which resolves commerce live, so a treaty
    // signed on the far side of the planet shows up here with no notification path.
    const CompositionInputKey_t after = ReadCompositionInputKey(a1);
    CHECK(after.psychAvailable > before.psychAvailable);
    CHECK_FALSE(before == after);
}
