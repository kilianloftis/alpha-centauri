#include "GameFixtures.h"

#include "game/CurrentGameState.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/PlayerInteraction.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/research/ResearchManager.h"
#include "game/research/ResearchRules.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/production/ProductionApplyResult.h"
#include "game/production/ProductionManager.h"
#include "game/map/WorldMap.h"
#include "game/natives/EnsureNativeDesign.h"
#include "game/natives/NativeDesign.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitSlotConfig.h"

#include <catch2/catch_test_macros.hpp>

#include <deque>
#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <variant>

using namespace ac;
using namespace actest;

namespace
{

struct LinkGame_
{
    FactionFixture fixtures;
    std::deque<UnitDesign> designs;
    Faction* pFaction = nullptr;

    LinkGame_()
    {
        fixtures.dataContext.nativeUnitRegistry = std::make_unique<NativeUnitRegistry>();
        fixtures.dataContext.nativeUnitRegistry->Load(FixturePath("native_units_full.json"));

        FillLand(fixtures.map);
        pFaction = &fixtures.state.AddFaction(std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            fixtures.state.GetWorldMap(), fixtures.settings, actest::k_TestFactionSeed));
    }

    BaseManager& MakeBase(Faction& rFaction, int x, int y)
    {
        Tile* pTile = fixtures.state.GetWorldMap().GetTile(x, y);
        REQUIRE(pTile);
        BaseManager* pBase = rFaction.CreateBase(
            fixtures.state.AllocateBaseId(), "TestBase", pTile,
            fixtures.state.GetTileEffects());
        REQUIRE(pBase);
        return *pBase;
    }

    Unit& MakeArtifact(Faction& rFaction, const Tile& rTile, BaseManager* pHome)
    {
        const NativeDesign* pDesign = EnsureNativeDesign(
            rFaction, *fixtures.dataContext.nativeUnitRegistry, "Alien_Artifact");
        REQUIRE(pDesign);
        return rFaction.GetUnitManager().CreateUnit(
            fixtures.state.AllocateUnitId(), *pDesign, fixtures.state.GetWorldMap().GetUnitPositions(), rTile,
            pHome, /*pProducedAt=*/nullptr);
    }

    Unit& MakeScout(Faction& rFaction, const Tile& rTile)
    {
        const UnitComponentConfig_t* pChassis = fixtures.unitComponents.Find("test_chassis");
        REQUIRE(pChassis);
        UnitSlotConfig_t slot;
        slot.id = "slot_0";
        slot.displayName = "slot_0";
        slot.componentType = pChassis->type;
        slot.required = true;
        std::unordered_map<std::string, const UnitComponentConfig_t*> assigned{{slot.id, pChassis}};
        designs.emplace_back(std::vector<UnitSlotConfig_t>{slot}, assigned);
        return rFaction.GetUnitManager().CreateUnit(
            fixtures.state.AllocateUnitId(), designs.back(), fixtures.state.GetWorldMap().GetUnitPositions(),
            rTile, nullptr, /*pProducedAt=*/nullptr);
    }
};

int UnitCount_(const Faction& rFaction)
{
    return static_cast<int>(std::ranges::distance(rFaction.GetUnitManager().Units()));
}

void DiscoverEverything_(ResearchManager& rResearch)
{
    for (;;)
    {
        const std::vector<const TechConfig_t*> available = AvailableTechs(rResearch);
        if (available.empty())
        {
            break;
        }
        rResearch.AddDiscoveredTech(available.front()->id);
    }
}

} // namespace

TEST_CASE("SubjectDesign matches the unit's design id and fails closed without a unit",
          "[effects][condition][native]")
{
    LinkGame_ game;
    BaseManager& base = game.MakeBase(*game.pFaction, 8, 8);
    Unit& artifact = game.MakeArtifact(*game.pFaction, base.GetTile(), &base);
    Unit& scout = game.MakeScout(*game.pFaction, *game.fixtures.state.GetWorldMap().GetTile(8, 4));

    EffectContext_t artifactCtx;
    artifactCtx.pUnit = &artifact;
    CHECK(ConditionSatisfied(SubjectDesign_t{"Alien_Artifact"}, artifactCtx));
    CHECK_FALSE(ConditionSatisfied(SubjectDesign_t{"Mind_Worm"}, artifactCtx));

    EffectContext_t scoutCtx;
    scoutCtx.pUnit = &scout;
    CHECK_FALSE(ConditionSatisfied(SubjectDesign_t{"Alien_Artifact"}, scoutCtx));
    CHECK_FALSE(ConditionSatisfied(SubjectDesign_t{"Alien_Artifact"}, EffectContext_t{}));
}

TEST_CASE("BaseHasBuilding matches a facility on the context base",
          "[effects][condition][native]")
{
    LinkGame_ game;
    BaseManager& base = game.MakeBase(*game.pFaction, 8, 8);
    EffectContext_t ctx;
    ctx.pBase = &base;
    CHECK_FALSE(ConditionSatisfied(BaseHasBuilding_t{"Network_Node"}, ctx));
    CHECK_FALSE(ConditionSatisfied(BaseHasBuilding_t{"Network_Node"}, EffectContext_t{}));

    base.GetBuildingManager().AddBuilding("Network_Node");
    CHECK(ConditionSatisfied(BaseHasBuilding_t{"Network_Node"}, ctx));
}

TEST_CASE("Holding an Alien Artifact at a Network Node prompts; Link grants a tech and removes it",
          "[native][artifact][hold]")
{
    LinkGame_ game;
    BaseManager& base = game.MakeBase(*game.pFaction, 8, 8);
    base.GetBuildingManager().AddBuilding("Network_Node");
    Unit& artifact = game.MakeArtifact(*game.pFaction, base.GetTile(), &base);
    artifact.SetOrder(HoldOrder_t{});

    const std::size_t techsBefore = game.pFaction->GetResearch().GetDiscoveredTechs().size();
    game.fixtures.state.ConsiderHoldLink(artifact);

    REQUIRE(game.fixtures.state.GetPlayerInteractions().Size() == 1);
    CHECK(std::holds_alternative<ArtifactLinkInteraction_t>(
        game.fixtures.state.GetPlayerInteractions().Front()->payload));
    CHECK(UnitCount_(*game.pFaction) == 1);
    CHECK(game.pFaction->GetResearch().GetDiscoveredTechs().size() == techsBefore);

    ApplyHoldLink(game.fixtures.state, artifact);
    CHECK(UnitCount_(*game.pFaction) == 0);
    CHECK(game.pFaction->GetResearch().GetDiscoveredTechs().size() > techsBefore);
}

TEST_CASE("Declining the artifact link leaves the artifact Holding",
          "[native][artifact][hold]")
{
    LinkGame_ game;
    BaseManager& base = game.MakeBase(*game.pFaction, 8, 8);
    base.GetBuildingManager().AddBuilding("Network_Node");
    Unit& artifact = game.MakeArtifact(*game.pFaction, base.GetTile(), &base);
    artifact.SetOrder(HoldOrder_t{});

    const std::size_t techsBefore = game.pFaction->GetResearch().GetDiscoveredTechs().size();
    game.fixtures.state.ConsiderHoldLink(artifact);
    REQUIRE(game.fixtures.state.GetPlayerInteractions().Size() == 1);

    CHECK(UnitCount_(*game.pFaction) == 1);
    REQUIRE(artifact.GetOrder().has_value());
    CHECK(std::holds_alternative<HoldOrder_t>(*artifact.GetOrder()));
    CHECK(game.pFaction->GetResearch().GetDiscoveredTechs().size() == techsBefore);

    game.fixtures.state.ConsiderHoldLink(artifact);
    CHECK(game.fixtures.state.GetPlayerInteractions().Size() == 2);
}

TEST_CASE("Hold does not offer a link without a Network Node or without an Alien Artifact",
          "[native][artifact][hold]")
{
    LinkGame_ game;
    BaseManager& bare = game.MakeBase(*game.pFaction, 8, 8);
    Unit& artifact = game.MakeArtifact(*game.pFaction, bare.GetTile(), &bare);
    artifact.SetOrder(HoldOrder_t{});
    game.fixtures.state.ConsiderHoldLink(artifact);
    CHECK(game.fixtures.state.GetPlayerInteractions().Empty());

    BaseManager& nodeBase = game.MakeBase(*game.pFaction, 8, 12);
    nodeBase.GetBuildingManager().AddBuilding("Network_Node");
    Unit& scout = game.MakeScout(*game.pFaction, nodeBase.GetTile());
    scout.SetOrder(HoldOrder_t{});
    game.fixtures.state.ConsiderHoldLink(scout);
    CHECK(game.fixtures.state.GetPlayerInteractions().Empty());

    Unit& healed = game.MakeArtifact(*game.pFaction, nodeBase.GetTile(), &nodeBase);
    healed.SetOrder(HoldUntilHealedOrder_t{});
    game.fixtures.state.ConsiderHoldLink(healed);
    CHECK(game.fixtures.state.GetPlayerInteractions().Empty());
}

TEST_CASE("An artifact Holding at a foreign base is not offered a link",
          "[native][artifact][hold]")
{
    LinkGame_ game;
    FactionConfig_t aiDef = game.fixtures.factionDefinition;
    aiDef.id = "ai";
    Faction& ai = game.fixtures.state.AddFaction(std::make_unique<Faction>(
        game.fixtures.state.AllocateFactionId(), false, aiDef, game.fixtures.dataContext,
        game.fixtures.state.GetWorldMap(), game.fixtures.settings, actest::k_TestFactionSeed + 1));
    BaseManager& aiBase = game.MakeBase(ai, 8, 6);
    aiBase.GetBuildingManager().AddBuilding("Network_Node");
    Unit& artifact = game.MakeArtifact(*game.pFaction, aiBase.GetTile(), nullptr);
    artifact.SetOrder(HoldOrder_t{});
    game.fixtures.state.ConsiderHoldLink(artifact);
    CHECK(game.fixtures.state.GetPlayerInteractions().Empty());
}

TEST_CASE("Completing a Network Node under a Holding artifact prompts to link",
          "[native][artifact][hold]")
{
    LinkGame_ game;
    BaseManager& base = game.MakeBase(*game.pFaction, 8, 8);
    Unit& artifact = game.MakeArtifact(*game.pFaction, base.GetTile(), &base);
    artifact.SetOrder(HoldOrder_t{});
    game.fixtures.state.ConsiderHoldLink(artifact);
    CHECK(game.fixtures.state.GetPlayerInteractions().Empty());

    const BuildingConfig_t* pNode = game.fixtures.buildings().Find("Network_Node");
    REQUIRE(pNode);
    base.GetProduction().SetProduction(pNode, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());
    REQUIRE(base.TryCompleteReadyProduction().kind == ProductionApplyKind_t::Completed);

    REQUIRE(game.fixtures.state.GetPlayerInteractions().Size() == 1);
    CHECK(std::holds_alternative<ArtifactLinkInteraction_t>(
        game.fixtures.state.GetPlayerInteractions().Front()->payload));
    CHECK(UnitCount_(*game.pFaction) == 1);
}

TEST_CASE("Linking an artifact spends it even when the research pool is empty",
          "[native][artifact][hold]")
{
    LinkGame_ game;
    BaseManager& base = game.MakeBase(*game.pFaction, 8, 8);
    base.GetBuildingManager().AddBuilding("Network_Node");
    Unit& artifact = game.MakeArtifact(*game.pFaction, base.GetTile(), &base);
    artifact.SetOrder(HoldOrder_t{});
    DiscoverEverything_(game.pFaction->GetResearch());
    const std::size_t techsBefore = game.pFaction->GetResearch().GetDiscoveredTechs().size();

    ApplyHoldLink(game.fixtures.state, artifact);
    CHECK(UnitCount_(*game.pFaction) == 0);
    CHECK(game.pFaction->GetResearch().GetDiscoveredTechs().size() == techsBefore);
}

TEST_CASE("An AI artifact Holding at a Network Node is linked without a prompt",
          "[native][artifact][hold]")
{
    LinkGame_ game;
    FactionConfig_t aiDef = game.fixtures.factionDefinition;
    aiDef.id = "ai";
    Faction& ai = game.fixtures.state.AddFaction(std::make_unique<Faction>(
        game.fixtures.state.AllocateFactionId(), false, aiDef, game.fixtures.dataContext,
        game.fixtures.state.GetWorldMap(), game.fixtures.settings, actest::k_TestFactionSeed + 1));
    BaseManager& base = game.MakeBase(ai, 8, 10);
    base.GetBuildingManager().AddBuilding("Network_Node");
    Unit& artifact = game.MakeArtifact(ai, base.GetTile(), &base);
    artifact.SetOrder(HoldOrder_t{});
    const std::size_t techsBefore = ai.GetResearch().GetDiscoveredTechs().size();
    const std::size_t queueBefore = game.fixtures.state.GetPlayerInteractions().Size();

    game.fixtures.state.ConsiderHoldLink(artifact);
    CHECK(game.fixtures.state.GetPlayerInteractions().Size() == queueBefore);
    CHECK(UnitCount_(ai) == 0);
    CHECK(ai.GetResearch().GetDiscoveredTechs().size() > techsBefore);
}

TEST_CASE("A GrantTech hold entry prompts and leaves the unit alive",
          "[native][artifact][hold]")
{
    UnitComponentConfig_t part;
    part.id = "hold_link";
    part.name = "Hold Link";
    part.type = "ability";
    TriggeredEffectConfig_t grant;
    grant.effect = GrantTechEffect_t{};
    grant.condition = BaseHasBuilding_t{"Network_Node"};
    part.onHoldEffects.push_back(std::move(grant));

    LinkGame_ game;
    const UnitComponentConfig_t* pChassis = game.fixtures.unitComponents.Find("test_chassis");
    REQUIRE(pChassis);
    UnitSlotConfig_t chassisSlot;
    chassisSlot.id = "chassis";
    chassisSlot.displayName = "chassis";
    chassisSlot.componentType = pChassis->type;
    chassisSlot.required = true;
    UnitSlotConfig_t abilitySlot;
    abilitySlot.id = "ability";
    abilitySlot.displayName = "ability";
    abilitySlot.componentType = "ability";
    abilitySlot.required = true;
    const std::unordered_map<std::string, const UnitComponentConfig_t*> assigned{
        {"chassis", pChassis},
        {"ability", &part},
    };
    game.designs.emplace_back(std::vector<UnitSlotConfig_t>{chassisSlot, abilitySlot}, assigned);

    BaseManager& base = game.MakeBase(*game.pFaction, 8, 8);
    base.GetBuildingManager().AddBuilding("Network_Node");
    Unit& holder = game.pFaction->GetUnitManager().CreateUnit(
        game.fixtures.state.AllocateUnitId(), game.designs.back(),
        game.fixtures.state.GetWorldMap().GetUnitPositions(), base.GetTile(), &base,
        /*pProducedAt=*/nullptr);
    holder.SetOrder(HoldOrder_t{});
    game.fixtures.state.ConsiderHoldLink(holder);
    REQUIRE(game.fixtures.state.GetPlayerInteractions().Size() == 1);
    CHECK(std::holds_alternative<ArtifactLinkInteraction_t>(
        game.fixtures.state.GetPlayerInteractions().Front()->payload));

    const std::size_t techsBefore = game.pFaction->GetResearch().GetDiscoveredTechs().size();
    ApplyHoldLink(game.fixtures.state, holder);
    CHECK(UnitCount_(*game.pFaction) == 1);
    CHECK(game.pFaction->GetResearch().GetDiscoveredTechs().size() > techsBefore);
}
