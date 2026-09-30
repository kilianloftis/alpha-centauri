// The EcologicalDamage multiplier stack: Planet rating, native life abundance and Perihelion
// each contribute a MultiplyGeometric factor. The fixture difficulty levels author none.

#include "GameFixtures.h"

#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/world-events/WorldEventTracker.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ac;
using namespace actest;
using Catch::Approx;

namespace
{

double EcoScale_(const BaseManager& rBase)
{
    return ResolveBaseStat(rBase.GetBaseEffects(), StatId_t::EcologicalDamage,
                           SeedFor(StatId_t::EcologicalDamage));
}

void SelectNativeLife_(FactionFixture& rFixtures, const char* pLevelId)
{
    GameRulesConfig_t rules = rFixtures.settings.GetGameRules();
    rules.nativeLifeLevelId = pLevelId;
    rFixtures.settings.SetGameRules(rules);
}

void RaisePlanet_(BaseManager& rBase, int levels)
{
    for (int i = 0; i < levels; ++i)
    {
        rBase.GetBuildingManager().AddBuilding("test_planet_plus1");
    }
}

} // namespace

TEST_CASE("A faction at PLANET 0 carries the neutral row's x3", "[ecology][scale]")
{
    FactionFixture fixtures;
    SelectNativeLife_(fixtures, "rare");
    Faction& rFaction = fixtures.MakeFaction();
    BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 4, 4);
    CHECK(EcoScale_(rBase) == Approx(3.0));
}

TEST_CASE("Higher PLANET lowers the multiplier to a floor of x1", "[ecology][scale]")
{
    const auto scaleAt = [](int planet) {
        FactionFixture fixtures;
        SelectNativeLife_(fixtures, "rare");
        Faction& rFaction = fixtures.MakeFaction();
        BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 4, 4);
        RaisePlanet_(rBase, planet);
        return EcoScale_(rBase);
    };
    CHECK(scaleAt(1) == Approx(2.0));
    CHECK(scaleAt(2) == Approx(1.0));
    CHECK(scaleAt(3) == Approx(1.0));
    // Past the table's top row the +3 row applies.
    CHECK(scaleAt(4) == Approx(1.0));
}

TEST_CASE("Native life abundance scales the multiplier x1 / x2 / x3", "[ecology][scale]")
{
    FactionFixture fixtures;
    Faction& rFaction = fixtures.MakeFaction();
    BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 4, 4);

    SelectNativeLife_(fixtures, "rare");
    const double rare = EcoScale_(rBase);
    SelectNativeLife_(fixtures, "normal");
    CHECK(EcoScale_(rBase) == Approx(2.0 * rare));
    SelectNativeLife_(fixtures, "abundant");
    CHECK(EcoScale_(rBase) == Approx(3.0 * rare));
    // Empty defers to the config's default level.
    SelectNativeLife_(fixtures, "");
    CHECK(EcoScale_(rBase) == Approx(2.0 * rare));
}

TEST_CASE("An active Perihelion doubles the multiplier until it ends", "[ecology][scale][world-events]")
{
    FactionFixture fixtures;
    Faction& rFaction = fixtures.MakeFaction();
    BaseManager& rBase = fixtures.MakeFactionBase(rFaction, 4, 4);
    fixtures.pBindState->CreateWorldEvents();
    WorldEventTracker& rEvents = *fixtures.pBindState->GetWorldEvents();

    const double before = EcoScale_(rBase);
    rEvents.Advance(0);
    CHECK(EcoScale_(rBase) == Approx(2.0 * before));
    rEvents.Advance(20);
    CHECK(EcoScale_(rBase) == Approx(before));
}
