#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/TriggeredEffect.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/UnitManager.h"
#include "game/units/NativeUnitConfig.h"
#include "game/map/FungalBloom.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/NativeUnitRegistry.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitDomain.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;
using Catch::Matchers::ContainsSubstring;

namespace
{

int FungusCount_(const WorldMap& rMap)
{
    int count = 0;
    for (const auto& pTile : rMap.GetTiles())
    {
        if (pTile->HasImprovement("Fungus"))
        {
            ++count;
        }
    }
    return count;
}

int UnitCount_(const Faction& rFaction)
{
    int count = 0;
    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        (void)rUnit;
        ++count;
    }
    return count;
}

// lifeMin/lifeMax override the map-rules roll. bShippingImprovements loads config/improvements.json
// so Former excludes are the shipping lists. bPlanet adds a native-life owner for spawns.
struct BloomSession_
{
    FactionFixture fixtures;
    ImprovementRegistry shippingImprovements;
    ElevationRulesConfig_t rules;
    std::unique_ptr<GameState> pState;
    Faction* pPlanet = nullptr;
    EffectPool pool;
    UnitComponentConfig_t warhead;
    std::deque<UnitDesign> designs;

    BloomSession_(int lifeMin, int lifeMax, bool bShippingImprovements, bool bPlanet)
    {
        rules = TestMapRules();

        fixtures.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
        fixtures.dataContext.nativeUnitRegistry->Load(std::string(AC_CONFIG_DIR)
                                                      + "/native_units.json");
        NativeLifeConfig_t life;
        life.fungalBloomNativeLifeformsMin = lifeMin;
        life.fungalBloomNativeLifeformsMax = lifeMax;
        fixtures.dataContext.nativeUnitRegistry->SetFungalBloomLifeforms(life);

        const ImprovementRegistry* pRegistry = &fixtures.improvements;
        if (bShippingImprovements)
        {
            shippingImprovements.Load(std::string(AC_CONFIG_DIR) + "/improvements.json");
            pRegistry = &shippingImprovements;
        }

        auto pMap = std::make_unique<WorldMap>(9, 9, rules);
        for (auto& pTile : pMap->GetTiles())
        {
            pTile->SetElevation(100);
        }
        pState = std::make_unique<GameState>(
            std::move(pMap), *pRegistry, &fixtures.unitComponents, fixtures.settings,
            *fixtures.dataContext.moraleCalculator, fixtures.dataContext.tileYieldRules,
            fixtures.dataContext.interactionGrids, k_TestRngSeed);

        if (!bPlanet)
        {
            return;
        }
        fixtures.extraDefinitions.push_back(
            std::make_unique<FactionConfig_t>(fixtures.factionDefinition));
        FactionConfig_t& rDef = *fixtures.extraDefinitions.back();
        rDef.id = "planet";
        rDef.identity.species = FactionSpecies_t::NativeLife;
        rDef.identity.participatesInCouncil = false;
        pPlanet = &pState->AddFaction(std::make_unique<Faction>(
            pState->AllocateFactionId(), /*bIsPlayerControlled=*/false, rDef, fixtures.dataContext,
            pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed));
    }

    Tile& At_(int x, int y) { return *pState->GetWorldMap().GetTile(x, y); }

    FungalBloomResult_t Bloom_(int x, int y, int count)
    {
        return ApplyFungalBloom(At_(x, y), pState->GetWorldMap(), count, pState->GetRng(),
                                *pState, *fixtures.dataContext.nativeUnitRegistry);
    }
};

} // namespace

TEST_CASE("A fungal bloom of 3 turns the origin and two neighbors to fungus", "[map][fungus]")
{
    BloomSession_ session(/*lifeMin=*/0, /*lifeMax=*/0, /*bShippingImprovements=*/false,
                          /*bPlanet=*/false);
    const FungalBloomResult_t result = session.Bloom_(4, 4, 3);

    CHECK(result.tilesFungused == 3);
    CHECK(result.lifeforms == 0);
    CHECK(FungusCount_(session.pState->GetWorldMap()) == 3);
    CHECK(session.At_(4, 4).HasImprovement("Fungus"));
    for (const auto& pTile : session.pState->GetWorldMap().GetTiles())
    {
        if (pTile->HasImprovement("Fungus"))
        {
            CHECK(ChebyshevDistance(session.At_(4, 4), *pTile,
                                    session.pState->GetWorldMap().GetWidth())
                  <= 1);
        }
    }
}

TEST_CASE("A fungal bloom skips neighbors that already have fungus", "[map][fungus]")
{
    BloomSession_ session(0, 0, false, false);
    Tile& rOrigin = session.At_(4, 4);
    ForEachTileInChebyshevRadius(rOrigin, session.pState->GetWorldMap(), 1, false,
        [&](Tile* pTile, int distance)
        {
            if (distance == 1 && !(pTile->GetX() == 5 && pTile->GetY() == 4))
            {
                pTile->AddImprovement(
                    session.pState->GetTileEffects().GetImprovements().Get("Fungus"));
            }
        });

    const FungalBloomResult_t result = session.Bloom_(4, 4, 2);
    CHECK(result.tilesFungused == 2);
    CHECK(rOrigin.HasImprovement("Fungus"));
    CHECK(session.At_(5, 4).HasImprovement("Fungus"));
}

TEST_CASE("A base tile is not a fungal bloom target", "[map][fungus]")
{
    BloomSession_ session(0, 0, false, false);
    session.pState->GetTileEffects().AddImprovementWithEffects(session.At_(5, 4),
                                                              std::string(ImprovementIds::k_Base));

    const FungalBloomResult_t around = session.Bloom_(4, 4, 9);
    CHECK(around.tilesFungused == 8);
    CHECK_FALSE(session.At_(5, 4).HasImprovement("Fungus"));
    CHECK(session.At_(4, 4).HasImprovement("Fungus"));

    BloomSession_ onBase(0, 0, false, false);
    onBase.pState->GetTileEffects().AddImprovementWithEffects(onBase.At_(4, 4),
                                                             std::string(ImprovementIds::k_Base));
    const FungalBloomResult_t fromBase = onBase.Bloom_(4, 4, 3);
    CHECK(fromBase.tilesFungused == 3);
    CHECK_FALSE(onBase.At_(4, 4).HasImprovement("Fungus"));
    CHECK(FungusCount_(onBase.pState->GetWorldMap()) == 3);
}

TEST_CASE("Fungus removes Former improvements that exclude it", "[map][fungus]")
{
    BloomSession_ session(0, 0, /*bShippingImprovements=*/true, false);
    Tile& rTile = session.At_(4, 4);
    session.pState->GetTileEffects().AddImprovementWithEffects(rTile, "Farm");
    session.pState->GetTileEffects().AddImprovementWithEffects(rTile, "Road");
    session.pState->GetTileEffects().AddImprovementWithEffects(rTile, "Nutrients");

    const FungalBloomResult_t result = session.Bloom_(4, 4, 1);
    CHECK(result.tilesFungused == 1);
    CHECK(rTile.HasImprovement("Fungus"));
    CHECK_FALSE(rTile.HasImprovement("Farm"));
    CHECK_FALSE(rTile.HasImprovement("Road"));
    CHECK(rTile.HasImprovement("Nutrients"));

    const ImprovementConfig_t& rFarm = session.shippingImprovements.Get("Farm");
    CHECK_FALSE(CanBuildImprovement(rTile, rFarm));
}

TEST_CASE("A fungal bloom spawns one native lifeform on a new fungal tile", "[map][fungus]")
{
    BloomSession_ land(1, 1, false, true);
    const FungalBloomResult_t landResult = land.Bloom_(4, 4, 1);
    CHECK(landResult.tilesFungused == 1);
    CHECK(landResult.lifeforms == 1);
    CHECK(UnitCount_(*land.pPlanet) == 1);
    for (const Unit& rUnit : land.pPlanet->GetUnitManager().Units())
    {
        CHECK(rUnit.GetTile().HasImprovement("Fungus"));
        CHECK(rUnit.GetDomain() != UnitDomain_t::Sea);
        CHECK(rUnit.GetDesign().GetId() != "Isle_of_the_Deep");
    }

    BloomSession_ sea(1, 1, false, true);
    sea.At_(4, 4).SetElevation(-100);
    REQUIRE(sea.At_(4, 4).IsWater());
    const FungalBloomResult_t seaResult = sea.Bloom_(4, 4, 1);
    CHECK(seaResult.lifeforms == 1);
    for (const Unit& rUnit : sea.pPlanet->GetUnitManager().Units())
    {
        CHECK(rUnit.GetTile().IsWater());
        CHECK(rUnit.GetDomain() != UnitDomain_t::Land);
        CHECK(rUnit.GetDesign().GetId() != "Mind_Worm");
    }
}

TEST_CASE("A fungal bloom spawns the configured lifeform count", "[map][fungus]")
{
    BloomSession_ session(2, 2, false, true);
    const FungalBloomResult_t result = session.Bloom_(4, 4, 3);
    CHECK(result.tilesFungused == 3);
    CHECK(result.lifeforms == 2);
    CHECK(UnitCount_(*session.pPlanet) == 2);
}

TEST_CASE("A fungal bloom with no native-life faction throws when it would spawn",
          "[map][fungus]")
{
    BloomSession_ session(1, 1, false, false);
    CHECK_THROWS_AS(session.Bloom_(4, 4, 1), std::logic_error);
}

TEST_CASE("Detonating a fungal payload funguses five tiles and spends the missile",
          "[map][fungus][detonate]")
{
    BloomSession_ session(1, 1, false, true);
    Faction& rPlayer = session.pState->AddFaction(std::make_unique<Faction>(
        session.pState->AllocateFactionId(), true, session.fixtures.factionDefinition,
        session.fixtures.dataContext, session.pState->GetWorldMap(), session.fixtures.settings,
        k_TestFactionSeed));

    session.warhead.id = "fungal_warhead";
    session.warhead.type = "weapon";
    TriggeredEffectConfig_t bloom;
    bloom.effect = FungalBloomEffect_t{.tilesStat = StatId_t::FungalBloomTiles};
    session.warhead.onDetonateEffects.push_back(std::move(bloom));
    TriggeredEffectConfig_t spend;
    spend.effect = DestroyUnitEffect_t{};
    session.warhead.onDetonateEffects.push_back(std::move(spend));
    session.warhead.effects.push_back(session.pool.StatMod(
        StatId_t::FungalBloomTiles, 5.0, ModifierOp_t::Add, EffectScope_t::ThisUnit));

    const UnitComponentConfig_t* pChassis = session.fixtures.unitComponents.Find("test_chassis");
    REQUIRE(pChassis);
    const std::vector<UnitSlotConfig_t> slots = {
        {.id = "weapon", .displayName = "Weapon", .componentType = "weapon", .required = true},
        {.id = "chassis", .displayName = "Chassis", .componentType = "chassis", .required = true},
    };
    const std::unordered_map<std::string, const UnitComponentConfig_t*> assigned = {
        {"weapon", &session.warhead},
        {"chassis", pChassis},
    };
    session.designs.emplace_back(slots, assigned);
    Unit& rMissile = rPlayer.GetUnitManager().CreateUnit(
        session.pState->AllocateUnitId(), session.designs.back(),
        session.pState->GetWorldMap().GetUnitPositions(), session.At_(4, 4),
        /*pHomeBase=*/nullptr, /*pProducedAt=*/nullptr);

    REQUIRE(rMissile.GetStat(StatId_t::FungalBloomTiles) == 5);
    REQUIRE(ApplyDetonation(*session.pState, rMissile));
    CHECK(FungusCount_(session.pState->GetWorldMap()) == 5);
    CHECK(UnitCount_(rPlayer) == 0);
    CHECK(UnitCount_(*session.pPlanet) == 1);
}

TEST_CASE("Shipping reactors set fungal bloom tiles 3, 5, 7, and 9", "[unit][fungus]")
{
    UnitComponentRegistry components;
    components.Load(std::string(AC_CONFIG_DIR) + "/unit_components");

    const auto tilesOf = [&](const char* pId) {
        const UnitComponentConfig_t* pReactor = components.Find(pId);
        REQUIRE(pReactor);
        for (const EffectConfig_t& rEffect : pReactor->effects)
        {
            const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
            if (pMod && pMod->stat == StatId_t::FungalBloomTiles)
            {
                return static_cast<int>(pMod->amount);
            }
        }
        return -1;
    };

    CHECK(tilesOf("Fission_Plant") == 3);
    CHECK(tilesOf("Fusion_Lab") == 5);
    CHECK(tilesOf("Quantum_Chambers") == 7);
    CHECK(tilesOf("Singularity_Inductor") == 9);

    const UnitComponentConfig_t* pPayload = components.Find("Fungal_Payload");
    REQUIRE(pPayload);
    REQUIRE(pPayload->onDetonateEffects.size() == 2);
    const auto* pBloom = std::get_if<FungalBloomEffect_t>(&pPayload->onDetonateEffects[0].effect);
    REQUIRE(pBloom);
    CHECK(pBloom->tilesStat == StatId_t::FungalBloomTiles);
}

TEST_CASE("Native units reject an inverted fungal lifeform range", "[map][fungus]")
{
    NativeUnitRegistry shipping;
    shipping.Load(std::string(AC_CONFIG_DIR) + "/native_units.json");
    CHECK(shipping.FungalBloomLifeforms().fungalBloomNativeLifeformsMin == 1);
    CHECK(shipping.FungalBloomLifeforms().fungalBloomNativeLifeformsMax == 1);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "ac_bad_fungal_bloom_natives.json";
    {
        std::ofstream file(path);
        file << R"({
            "fungal_bloom_native_lifeforms_min": 2,
            "fungal_bloom_native_lifeforms_max": 0,
            "units": []
        })";
    }
    CHECK_THROWS_WITH(NativeUnitRegistry{}.Load(path.string()),
                      ContainsSubstring("fungal_bloom_native_lifeforms_max"));
    std::filesystem::remove(path);
}
