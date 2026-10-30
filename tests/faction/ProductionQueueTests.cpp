#include "GameFixtures.h"

#include "game/base/BaseManager.h"
#include "game/buildings/BuildingConfig.h"
#include "game/faction/Faction.h"
#include "game/production/IConstructable.h"
#include "game/production/ProductionApplyResult.h"
#include "game/production/ProductionManager.h"
#include "game/production/ProductionRules.h"
#include "game/stages/BaseProduction.h"
#include "game/stages/HookContext.h"
#include "game/units/Military.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/buildings/BuildingManager.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

struct QueueGame_
{
    FactionFixture fixtures;
    Faction* pFaction = nullptr;

    QueueGame_()
    {
        FillLand(fixtures.map);
        pFaction = &fixtures.state.AddFaction(std::make_unique<Faction>(
            fixtures.state.AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
            fixtures.state.GetWorldMap(), fixtures.settings, fixtures.state.GetUnitIdAllocator(), k_TestFactionSeed));
    }

    BaseManager& MakeBase()
    {
        BaseManager* pBase = pFaction->CreateBase(
            fixtures.state.AllocateBaseId(), "TestBase", fixtures.state.GetWorldMap().GetTile(8, 8),
            fixtures.state.GetTileEffects(), 3);
        REQUIRE(pBase != nullptr);
        return *pBase;
    }

    const UnitDesign& AddDesign()
    {
        std::vector<UnitSlotConfig_t> slots;
        std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
        const std::vector<std::string> ids{"test_chassis", "test_weapon", "test_armor"};
        int slotIndex = 0;
        for (const std::string& rId : ids)
        {
            const UnitComponentConfig_t* pComponent = fixtures.unitComponents.Find(rId);
            REQUIRE(pComponent != nullptr);
            UnitSlotConfig_t slot;
            slot.id = "slot_" + std::to_string(slotIndex++);
            slot.displayName = slot.id;
            slot.componentType = pComponent->type;
            slot.required = true;
            assigned[slot.id] = pComponent;
            slots.push_back(slot);
        }
        auto pDesign = std::make_unique<UnitDesign>(slots, assigned);
        const UnitDesign& rDesign = *pDesign;
        REQUIRE(pFaction->GetMilitary().AddDesign(std::move(pDesign)));
        return rDesign;
    }
};

const BuildingConfig_t& FacilityA_(QueueGame_& rGame)
{
    return rGame.fixtures.buildings().Get("test_facility_a");
}

const BuildingConfig_t& FacilityB_(QueueGame_& rGame)
{
    return rGame.fixtures.buildings().Get("test_facility_b");
}

const BuildingConfig_t& Center_(QueueGame_& rGame)
{
    return rGame.fixtures.buildings().Get("Command_Center");
}

} // namespace

TEST_CASE("Replace and Insert edit the queue without retool", "[production][queue]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rA = FacilityA_(game);
    const BuildingConfig_t& rB = FacilityB_(game);
    const BuildingConfig_t& rCenter = Center_(game);
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    rProduction.SetProduction(&rA, rEffects);
    rProduction.Insert(0, rB);
    rProduction.BankProduction(40);
    REQUIRE(rProduction.GetQueue().size() == 1);

    rProduction.Replace(0, rCenter);
    CHECK(rProduction.GetCurrentProduction() == &rA);
    REQUIRE(rProduction.GetQueue().size() == 1);
    CHECK(rProduction.GetQueue()[0] == &rCenter);
    CHECK(rProduction.GetMineralStockpile() == 40);

    rProduction.Insert(1, rA);
    CHECK(rProduction.GetQueue().size() == 2);
    CHECK(rProduction.GetQueue()[1] == &rA);
    CHECK(rProduction.GetMineralStockpile() == 40);
}

TEST_CASE("Insert at slot 0 does not change current production or retool", "[production][queue]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rA = FacilityA_(game);
    const BuildingConfig_t& rB = FacilityB_(game);
    const BuildingConfig_t& rCenter = Center_(game);
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    rProduction.SetProduction(&rA, rEffects);
    rProduction.Insert(0, rB);
    rProduction.BankProduction(40);

    rProduction.Insert(0, rCenter);
    CHECK(rProduction.GetCurrentProduction() == &rA);
    REQUIRE(rProduction.GetQueue().size() == 2);
    CHECK(rProduction.GetQueue()[0] == &rCenter);
    CHECK(rProduction.GetQueue()[1] == &rB);
    CHECK(rProduction.GetMineralStockpile() == 40);

    rProduction.Delete(1);
    REQUIRE(rProduction.GetQueue().size() == 1);
    CHECK(rProduction.GetQueue()[0] == &rCenter);
    CHECK(rProduction.GetMineralStockpile() == 40);
}

TEST_CASE("SetProduction changes current production and leaves the queue",
          "[production][queue]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rA = FacilityA_(game);
    const BuildingConfig_t& rB = FacilityB_(game);
    const BuildingConfig_t& rCenter = Center_(game);
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    rProduction.SetProduction(&rA, rEffects);
    rProduction.Insert(0, rB);
    rProduction.SetProduction(&rCenter, rEffects);

    CHECK(rProduction.GetCurrentProduction() == &rCenter);
    REQUIRE(rProduction.GetQueue().size() == 1);
    CHECK(rProduction.GetQueue()[0] == &rB);
}

TEST_CASE("A refused queue edit throws", "[production][queue]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rA = FacilityA_(game);
    ProductionManager& rProduction = base.GetProduction();

    CHECK_THROWS_AS(rProduction.Replace(3, rA), std::invalid_argument);
    CHECK_THROWS_AS(rProduction.Insert(-1, rA), std::invalid_argument);
    CHECK_THROWS_AS(rProduction.Delete(1), std::invalid_argument);
}

TEST_CASE("A once-only building occupies one queue slot and Insert moves it",
          "[production][queue]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rA = FacilityA_(game);
    const BuildingConfig_t& rB = FacilityB_(game);
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    rProduction.SetProduction(&rA, rEffects);
    rProduction.Insert(0, rB);
    rProduction.Insert(1, rA);
    rProduction.Insert(2, rB);

    REQUIRE(rProduction.GetQueue().size() == 2);
    CHECK(rProduction.GetCurrentProduction() == &rA);
    CHECK(rProduction.GetQueue()[0] == &rA);
    CHECK(rProduction.GetQueue()[1] == &rB);
    const std::vector<const IConstructable*> available = ConstructableItems(base);
    CHECK(std::ranges::contains(available, &rB));
}

TEST_CASE("SetProduction of a queued once-only building pulls it off the queue",
          "[production][queue]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rA = FacilityA_(game);
    const BuildingConfig_t& rB = FacilityB_(game);
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    rProduction.SetProduction(&rA, rEffects);
    rProduction.Insert(0, rB);
    rProduction.SetProduction(&rB, rEffects);

    CHECK(rProduction.GetCurrentProduction() == &rB);
    CHECK(rProduction.GetQueue().empty());
}

TEST_CASE("Units and allowMultiple buildings can be queued twice", "[production][queue]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rA = FacilityA_(game);
    const UnitDesign& rDesign = game.AddDesign();
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    rProduction.SetProduction(&rA, rEffects);
    rProduction.Insert(0, rA);
    rProduction.Insert(1, rDesign);
    rProduction.Insert(2, rDesign);

    REQUIRE(rProduction.GetQueue().size() == 3);
    CHECK(rProduction.GetQueue()[0] == &rA);
    CHECK(rProduction.GetQueue()[1] == &rDesign);
    CHECK(rProduction.GetQueue()[2] == &rDesign);
}

TEST_CASE("Completion leftover minerals sit on the first item that survives validate",
          "[production][queue]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rA = FacilityA_(game);
    const BuildingConfig_t& rB = FacilityB_(game);
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    rProduction.SetProduction(&rA, rEffects);
    rProduction.Insert(0, rB);
    rProduction.SetMineralStockpile(base.GetMineralCost() + 8);

    REQUIRE(rProduction.TryCompleteReadyAtTurnStart().kind == ProductionApplyKind_t::Completed);
    CHECK_FALSE(rProduction.HasProduction());
    REQUIRE(rProduction.GetQueue().size() == 1);
    CHECK(rProduction.GetQueue()[0] == &rB);
    rProduction.ValidateQueue();
    CHECK(rProduction.GetCurrentProduction() == &rB);
    CHECK(rProduction.GetQueue().empty());
    CHECK(rProduction.GetMineralStockpile() == 8);
}

TEST_CASE("ValidateQueue drops a completed once-only building still listed",
          "[production][queue]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rB = FacilityB_(game);
    const BuildingConfig_t& rA = FacilityA_(game);
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    rProduction.SetProduction(&rB, rEffects);
    rProduction.Insert(0, rA);
    rProduction.SetMineralStockpile(base.GetMineralCost());

    base.ApplyProduction();
    REQUIRE(rProduction.TryCompleteReadyAtTurnStart().kind == ProductionApplyKind_t::Completed);
    CHECK(base.GetBuildingManager().HasBuilding(rB.id));
    rProduction.ValidateQueue();
    CHECK(rProduction.GetCurrentProduction() == &rA);
    CHECK_FALSE(std::ranges::contains(rProduction.GetQueue(), &rB));
}

TEST_CASE("ValidateQueue keeps one copy of a once-only building", "[production][queue]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rB = FacilityB_(game);
    const BuildingConfig_t& rA = FacilityA_(game);
    ProductionManager& rProduction = base.GetProduction();

    rProduction.RebindQueue({&rB, &rB, &rA});
    rProduction.ValidateQueue();
    REQUIRE(rProduction.GetQueue().size() == 2);
    CHECK(rProduction.GetQueue()[0] == &rB);
    CHECK(rProduction.GetQueue()[1] == &rA);
}

TEST_CASE("BaseProduction strips an invalid later slot and keeps minerals",
          "[production][queue][BaseProduction]")
{
    QueueGame_ game;
    BaseManager& base = game.MakeBase();
    const BuildingConfig_t& rHurry = game.fixtures.buildings().Get("test_hurry_facility");
    const BuildingConfig_t& rCenter = Center_(game);
    ProductionManager& rProduction = base.GetProduction();
    const BaseEffects_t& rEffects = base.GetBaseEffects();

    rProduction.SetProduction(&rHurry, rEffects);
    rProduction.Insert(0, rCenter);
    base.GetBuildingManager().AddBuilding("Command_Center");
    rProduction.SetMineralStockpile(5);

    BaseProduction stage(HookContext{});
    CHECK(stage.Execute(game.fixtures.state, *game.pFaction) == StageResult_t::Continue);
    CHECK(rProduction.GetQueue().empty());
    CHECK(rProduction.GetCurrentProduction() == &rHurry);
    CHECK(rProduction.GetMineralStockpile() >= 5);
}
