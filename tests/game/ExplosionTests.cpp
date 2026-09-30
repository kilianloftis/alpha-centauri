#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TriggeredEffect.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Explosion.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
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
            std::move(pMap), fixtures.dataContext, fixtures.settings, k_TestRngSeed);
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

    Faction& AddFaction_()
    {
        return pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), false, fixtures.factionDefinition, fixtures.dataContext,
            pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed));
    }

    Unit& MakeChassisUnit_(int x, int y)
    {
        return MakeChassisUnitFor_(*pPlayer, x, y);
    }

    Unit& MakeChassisUnitFor_(Faction& rFaction, int x, int y)
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
        return rFaction.GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), designs.back(), pState->GetWorldMap().GetUnitPositions(),
            At_(x, y), /*pHomeBase=*/nullptr, /*pProducedAt=*/nullptr);
    }

    Unit& MakePlanetBuster_(int x, int y)
    {
        warhead.id = "planet_buster_test";
        warhead.type = "weapon";
        warhead.onDetonateEffects.clear();
        TriggeredEffectConfig_t explosion;
        ExplosionEffect_t blast;
        blast.radius = 1;
        explosion.effect = blast;
        warhead.onDetonateEffects.push_back(std::move(explosion));
        TriggeredEffectConfig_t atrocity;
        atrocity.effect = CommitAtrocityEffect_t{AtrocitySeverityId_t::Major};
        warhead.onDetonateEffects.push_back(std::move(atrocity));
        TriggeredEffectConfig_t spend;
        spend.effect = DestroyUnitEffect_t{};
        warhead.onDetonateEffects.push_back(std::move(spend));

        const UnitComponentConfig_t* pChassis = fixtures.unitComponents.Find("test_chassis");
        REQUIRE(pChassis);
        const std::vector<UnitSlotConfig_t> slots = {
            {.id = "weapon", .displayName = "Weapon", .componentType = "weapon", .required = true},
            {.id = "chassis", .displayName = "Chassis", .componentType = "chassis", .required = true},
        };
        const std::unordered_map<std::string, const UnitComponentConfig_t*> assigned = {
            {"weapon", &warhead},
            {"chassis", pChassis},
        };
        designs.emplace_back(slots, assigned);
        return pPlayer->GetUnitManager().CreateUnit(
            pState->AllocateUnitId(), designs.back(),
            pState->GetWorldMap().GetUnitPositions(), At_(x, y),
            /*pHomeBase=*/nullptr, /*pProducedAt=*/nullptr);
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

TEST_CASE("Detonating a Planet Buster names the first foreign base destroyed",
          "[map][explosion][detonate][atrocity]")
{
    BlastGame_ game;
    Faction& rVictim = game.AddFaction_();
    Faction& rBystander = game.AddFaction_();

    BaseManager* pVictimBase = rVictim.CreateBase(
        game.pState->AllocateBaseId(), "VictimBase", &game.At_(4, 4), game.fixtures.dataContext,
        game.pState->GetTileEffects(), game.pState->GetSecretProjectAvailability());
    REQUIRE(pVictimBase);

    Unit& rMissile = game.MakePlanetBuster_(4, 4);
    REQUIRE(ApplyDetonation(*game.pState, rMissile));

    const AtrocityLedger& rLedger = game.pState->GetAtrocityLedger();
    REQUIRE(rLedger.Records().size() == 1);
    CHECK(rLedger.Records().front().perpetrator == game.pPlayer->GetFactionId());
    CHECK(rLedger.Records().front().victim == rVictim.GetFactionId());
    CHECK(rLedger.Records().front().severity == AtrocitySeverityId_t::Major);
    CHECK_FALSE(rLedger.IsSanctioned(game.pPlayer->GetFactionId(), game.pState->GetMissionYear()));
    // Victim is excluded from universal Vendetta; a living AI bystander declares it.
    CHECK(game.pState->GetDiplomacyLedger().GetStatus(rVictim.GetFactionId(),
                                                      game.pPlayer->GetFactionId())
          != DiplomaticStatus_t::Vendetta);
    CHECK(game.pState->GetDiplomacyLedger().GetStatus(rBystander.GetFactionId(),
                                                      game.pPlayer->GetFactionId())
          == DiplomaticStatus_t::Vendetta);
}

TEST_CASE("A Planet Buster next to a foreign base names that owner even off the base tile",
          "[map][explosion][detonate][atrocity]")
{
    BlastGame_ game;
    Faction& rVictim = game.AddFaction_();
    BaseManager* pVictimBase = rVictim.CreateBase(
        game.pState->AllocateBaseId(), "VictimBase", &game.At_(5, 4), game.fixtures.dataContext,
        game.pState->GetTileEffects(), game.pState->GetSecretProjectAvailability());
    REQUIRE(pVictimBase);
    game.pState->RebuildTerritory();
    // Detonation is on a neighboring tile, not the base tile itself. Territory may claim it;
    // the victim still comes from the razed base.
    REQUIRE(game.pState->GetWorldMap().GetTerritory().HasOwner(game.At_(5, 4)));

    Unit& rMissile = game.MakePlanetBuster_(4, 4);
    REQUIRE(ApplyDetonation(*game.pState, rMissile));

    REQUIRE(game.pState->GetAtrocityLedger().Records().size() == 1);
    CHECK(game.pState->GetAtrocityLedger().Records().front().victim == rVictim.GetFactionId());
}

TEST_CASE("A Planet Buster on own land that kills foreign units names the enemy",
          "[map][explosion][detonate][atrocity]")
{
    BlastGame_ game;
    Faction& rEnemy = game.AddFaction_();
    game.pPlayer->CreateBase(
        game.pState->AllocateBaseId(), "Home", &game.At_(4, 4), game.fixtures.dataContext,
        game.pState->GetTileEffects(), game.pState->GetSecretProjectAvailability());
    game.pState->RebuildTerritory();
    REQUIRE(game.pState->GetWorldMap().GetTerritory().GetOwner(game.At_(4, 4))
            == game.pPlayer->GetFactionId());

    game.MakeChassisUnitFor_(rEnemy, 5, 4);

    Unit& rMissile = game.MakePlanetBuster_(4, 4);
    REQUIRE(ApplyDetonation(*game.pState, rMissile));

    REQUIRE(game.pState->GetAtrocityLedger().Records().size() == 1);
    CHECK(game.pState->GetAtrocityLedger().Records().front().victim == rEnemy.GetFactionId());
    CHECK_FALSE(game.pState->GetAtrocityLedger().HasVictimized(
        game.pPlayer->GetFactionId(), game.pPlayer->GetFactionId()));
}

TEST_CASE("A warhead that authors no atrocity records none", "[map][explosion][detonate][atrocity]")
{
    BlastGame_ game;
    game.warhead.id = "clean_payload_test";
    game.warhead.type = "weapon";
    TriggeredEffectConfig_t spend;
    spend.effect = DestroyUnitEffect_t{};
    game.warhead.onDetonateEffects.push_back(std::move(spend));

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

    REQUIRE(ApplyDetonation(*game.pState, rMissile));
    CHECK(game.pState->GetAtrocityLedger().Records().empty());
}

TEST_CASE("A warhead's eco charge lands before DestroyUnit spends it",
          "[map][explosion][detonate][ecology]")
{
    BlastGame_ game;
    game.warhead.id = "tectonic_test";
    game.warhead.type = "weapon";
    TriggeredEffectConfig_t charge;
    charge.effect = AddVirtualMineralsEffect_t{5};
    game.warhead.onDetonateEffects.push_back(std::move(charge));
    TriggeredEffectConfig_t spend;
    spend.effect = DestroyUnitEffect_t{};
    game.warhead.onDetonateEffects.push_back(std::move(spend));

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

    REQUIRE(ApplyDetonation(*game.pState, rMissile));
    CHECK(game.pState->GetEcologyLedger().VirtualMinerals(game.pPlayer->GetFactionId()) == 5);
    CHECK(UnitCount_(*game.pPlayer) == 0);
    CHECK(game.pState->GetAtrocityLedger().Records().empty());
}
