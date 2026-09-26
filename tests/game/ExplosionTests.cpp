#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TriggeredEffect.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Explosion.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"

#include <catch2/catch_test_macros.hpp>

#include <deque>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

int UnitCount_(const Faction& rFaction)
{
    return static_cast<int>(std::ranges::distance(rFaction.GetUnitManager().Units()));
}

bool StillAlive_(const Faction& rFaction, const Unit& rUnit)
{
    for (const Unit& rLive : rFaction.GetUnitManager().Units())
    {
        if (&rLive == &rUnit)
        {
            return true;
        }
    }
    return false;
}

struct BlastGame_
{
    FactionFixture fixtures;
    std::unique_ptr<GameState> pState;
    Faction* pPlayer = nullptr;
    EffectPool pool;
    UnitComponentConfig_t warhead;
    std::deque<UnitDesign> designs;

    BlastGame_()
    {
        auto pMap = std::make_unique<WorldMap>(9, 9, TestMapRules());
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(3000);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), fixtures.improvements, &fixtures.unitComponents, fixtures.settings,
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules,
            fixtures.dataContext.interactionGrids, k_TestRngSeed);
        pPlayer = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed));
    }

    Tile& At_(int x, int y)
    {
        Tile* pTile = pState->GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        return *pTile;
    }

    BaseManager& MakeBase_(int x, int y)
    {
        BaseManager* pBase = pPlayer->CreateBase(
            pState->AllocateBaseId(), "TestBase", &At_(x, y), fixtures.dataContext,
            pState->GetTileEffects(), pState->GetSecretProjectAvailability());
        REQUIRE(pBase);
        return *pBase;
    }

    Unit& MakeChassisUnit_(int x, int y)
    {
        const UnitComponentConfig_t* pChassis = fixtures.unitComponents.Find("test_chassis");
        REQUIRE(pChassis);
        const std::vector<UnitSlotConfig_t> slots = {
            {.id = "chassis", .displayName = "Chassis", .componentType = "chassis", .required = true},
        };
        const std::unordered_map<std::string, const UnitComponentConfig_t*> assigned = {
            {"chassis", pChassis},
        };
        designs.emplace_back(slots, assigned);
        return pPlayer->GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), designs.back(), pState->GetWorldMap().GetUnitPositions(),
            At_(x, y), /*pHomeBase=*/nullptr, /*pProducedAt=*/nullptr);
    }
};

} // namespace

TEST_CASE("A radius-1 explosion wipes the 3x3 and lowers each tile one level",
          "[map][explosion]")
{
    BlastGame_ game;
    Tile& rOrigin = game.At_(4, 4);
    Tile& rFarm = game.At_(5, 4);
    Tile& rFungus = game.At_(4, 3);
    Tile& rAquifer = game.At_(3, 3);
    Tile& rOutside = game.At_(6, 4);
    const Rockiness_t rockiness = rOrigin.GetRockiness();
    const Moisture_t moisture = rOrigin.GetMoisture();

    game.pState->GetTileEffects().AddOccupantWithEffects(rFarm, std::string(ImprovementIds::k_Farm));
    game.pState->GetTileEffects().AddOccupantWithEffects(rFungus, std::string(ImprovementIds::k_Fungus));
    rAquifer.SetHasAquifer(true);
    BaseManager& rBase = game.MakeBase_(4, 5);
    Unit& rVictim = game.MakeChassisUnit_(3, 4);
    Unit& rOutsider = game.MakeChassisUnit_(6, 4);
    Unit& rDetonator = game.MakeChassisUnit_(4, 4);
    REQUIRE(UnitCount_(*game.pPlayer) == 3);
    REQUIRE(rFarm.HasImprovement(ImprovementIds::k_Farm));
    REQUIRE(rFungus.HasFeature(ImprovementIds::k_Fungus));

    const ExplosionResult_t result =
        ApplyExplosion(rOrigin, game.pState->GetWorldMap(), /*radius=*/1, game.pState->GetRng(),
                       *game.pState, &rDetonator);

    CHECK(result.bChanged);
    CHECK(result.radius == 1);
    CHECK(result.tiles == 9);
    CHECK(StillAlive_(*game.pPlayer, rDetonator));
    CHECK(StillAlive_(*game.pPlayer, rOutsider));
    CHECK(UnitCount_(*game.pPlayer) == 2);
    CHECK(rBase.IsRazed());
    CHECK_FALSE(game.At_(4, 5).HasImprovement(ImprovementIds::k_Base));
    CHECK_FALSE(rFarm.HasImprovement(ImprovementIds::k_Farm));
    CHECK(rFungus.HasFeature(ImprovementIds::k_Fungus));
    CHECK(rAquifer.GetHasAquifer());
    CHECK(rOrigin.GetRockiness() == rockiness);
    CHECK(rOrigin.GetMoisture() == moisture);

    const ElevationRulesConfig_t& rRules = rOrigin.MapRules();
    for (const auto& pTile : game.pState->GetWorldMap().GetTiles())
    {
        const int distance = ChebyshevDistance(rOrigin, *pTile, game.pState->GetWorldMap().GetWidth());
        if (distance <= 1)
        {
            const int drop = 3000 - pTile->GetElevation();
            CHECK(drop >= rRules.levelMinMeters);
            CHECK(drop <= rRules.levelMaxMeters);
        }
        else
        {
            CHECK(pTile->GetElevation() == 3000);
        }
    }
    CHECK(rOutside.GetElevation() == 3000);
    (void)rVictim;
}

TEST_CASE("An explosion at radius 0 and at Planet's floor changes nothing", "[map][explosion]")
{
    BlastGame_ game;
    Tile& rOrigin = game.At_(4, 4);
    Unit& rUnit = game.MakeChassisUnit_(4, 4);

    const ExplosionResult_t none =
        ApplyExplosion(rOrigin, game.pState->GetWorldMap(), /*radius=*/0, game.pState->GetRng(),
                       *game.pState, &rUnit);
    CHECK_FALSE(none.bChanged);
    CHECK(none.tiles == 0);
    CHECK(rOrigin.GetElevation() == 3000);
    CHECK(StillAlive_(*game.pPlayer, rUnit));

    for (auto& pTile : game.pState->GetWorldMap().GetTiles())
    {
        pTile->SetElevation(pTile->MapRules().minElevationMeters);
    }
    const int floor = rOrigin.GetElevation();
    const ExplosionResult_t floored =
        ApplyExplosion(rOrigin, game.pState->GetWorldMap(), /*radius=*/1, game.pState->GetRng(),
                       *game.pState, &rUnit);
    CHECK(floored.tiles == 9);
    CHECK_FALSE(floored.bChanged);
    CHECK(rOrigin.GetElevation() == floor);
    CHECK(StillAlive_(*game.pPlayer, rUnit));
}

TEST_CASE("Detonating a planet buster spends the missile after the blast",
          "[map][explosion][detonate]")
{
    BlastGame_ game;
    game.warhead.id = "planet_buster_test";
    game.warhead.type = "weapon";
    TriggeredEffectConfig_t explosion;
    explosion.effect = ExplosionEffect_t{.radiusStat = StatId_t::ExplosionRadius};
    game.warhead.onDetonateEffects.push_back(std::move(explosion));
    TriggeredEffectConfig_t spend;
    spend.effect = DestroyUnitEffect_t{};
    game.warhead.onDetonateEffects.push_back(std::move(spend));
    game.warhead.effects.push_back(game.pool.StatMod(
        StatId_t::ExplosionRadius, 1.0, ModifierOp_t::Add, EffectScope_t::ThisUnit));

    const UnitComponentConfig_t* pChassis = game.fixtures.unitComponents.Find("test_chassis");
    REQUIRE(pChassis);
    const std::vector<UnitSlotConfig_t> slots = {
        {.id = "weapon", .displayName = "Weapon", .componentType = "weapon", .required = true},
        {.id = "chassis", .displayName = "Chassis", .componentType = "chassis", .required = true},
    };
    const std::unordered_map<std::string, const UnitComponentConfig_t*> assigned = {
        {"weapon", &game.warhead},
        {"chassis", pChassis},
    };
    game.designs.emplace_back(slots, assigned);
    Unit& rMissile = game.pPlayer->GetUnitManager().CreateUnit(
        game.pState->AllocateUnitId(), game.designs.back(),
        game.pState->GetWorldMap().GetUnitPositions(), game.At_(4, 4),
        /*pHomeBase=*/nullptr, /*pProducedAt=*/nullptr);
    Unit& rOutsider = game.MakeChassisUnit_(6, 4);
    game.MakeChassisUnit_(3, 4);

    REQUIRE(rMissile.GetStat(StatId_t::ExplosionRadius) == 1);
    REQUIRE(ApplyDetonation(*game.pState, rMissile));
    CHECK(UnitCount_(*game.pPlayer) == 1);
    CHECK(StillAlive_(*game.pPlayer, rOutsider));
    const int drop = 3000 - game.At_(4, 4).GetElevation();
    CHECK(drop >= game.At_(4, 4).MapRules().levelMinMeters);
    CHECK(drop <= game.At_(4, 4).MapRules().levelMaxMeters);
}

TEST_CASE("Shipping reactors set explosion radius 1, 2, 3, and 4", "[unit][explosion]")
{
    UnitComponentRegistry components;
    components.Load(std::string(AC_CONFIG_DIR) + "/unit_components");

    const auto radiusOf = [&](const char* pId) {
        const UnitComponentConfig_t* pReactor = components.Find(pId);
        REQUIRE(pReactor);
        for (const EffectConfig_t& rEffect : pReactor->effects)
        {
            const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
            if (pMod && pMod->stat == StatId_t::ExplosionRadius)
            {
                return static_cast<int>(pMod->amount);
            }
        }
        return -1;
    };

    CHECK(radiusOf("Fission_Plant") == 1);
    CHECK(radiusOf("Fusion_Lab") == 2);
    CHECK(radiusOf("Quantum_Chambers") == 3);
    CHECK(radiusOf("Singularity_Inductor") == 4);

    const UnitComponentConfig_t* pPayload = components.Find("Planet_Buster");
    REQUIRE(pPayload);
    REQUIRE(pPayload->onDetonateEffects.size() == 2);
    const auto* pExplosion = std::get_if<ExplosionEffect_t>(&pPayload->onDetonateEffects[0].effect);
    REQUIRE(pExplosion);
    CHECK(pExplosion->radiusStat == StatId_t::ExplosionRadius);
    CHECK(std::holds_alternative<DestroyUnitEffect_t>(pPayload->onDetonateEffects[1].effect));
}
