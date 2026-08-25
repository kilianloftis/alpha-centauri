// InteractionPresenter gates mid-turn prompts against GameSettings: disabled arms still
// CompleteFront (with a domain default when needed) so the turn never stalls.

#include "ViewFixture.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/PlayerInteraction.h"
#include "game/PauseOnEventsConfig.h"
#include "game/PlayerInteractionQueue.h"
#include "game/faction/Military.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionApplyResult.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"
#include "input/Input.h"
#include "input/PlatformEventQueue.h"
#include "ui/InteractionPresenter.h"
#include "ui/UIManager.h"
#include "ui/world/WorldView.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

using namespace ac;
using actest::ViewFixture;

namespace
{

struct PresenterHarness_
{
    ViewFixture fixture;
    PlatformEventQueue queue;
    std::unique_ptr<Input> pInput = CreateInput(queue);
    UIManager ui{fixture.graphics, *pInput};
    WorldView* pWorldView = nullptr;
    int advanceCount = 0;
    std::unique_ptr<InteractionPresenter> pPresenter;

    PresenterHarness_()
    {
        auto pWorld = fixture.pFactory->CreateWorldView(
            ViewFixture::FullScreen(),
            [] {},
            [] {},
            [](BaseManager&) {},
            [](auto&&...) {},
            [] {});
        pWorldView = pWorld.get();
        ui.SetWorldView(std::move(pWorld));

        pPresenter = std::make_unique<InteractionPresenter>(
            *fixture.pState,
            ui,
            *fixture.pFactory,
            *pWorldView,
            [this]() { ++advanceCount; });
    }

    void Enqueue(PlayerInteraction_t payload)
    {
        QueuedInteraction_t item;
        item.payload = std::move(payload);
        item.audience = fixture.pPlayer->GetFactionId();
        fixture.pState->GetPlayerInteractions().Enqueue(std::move(item));
    }
};

const UnitDesign& AddPodDesign_(ViewFixture& rFixture)
{
    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    int slotIndex = 0;
    for (const std::string& rId : {"test_chassis", "test_colony_pod", "test_armor"})
    {
        const UnitComponentConfig_t* pComponent = rFixture.unitComponents.Find(rId);
        REQUIRE(pComponent);
        UnitSlotConfig_t slot;
        slot.id = "slot_" + std::to_string(slotIndex++);
        slot.displayName = slot.id;
        slot.componentType = pComponent->type;
        slots.push_back(slot);
        assigned[slot.id] = pComponent;
    }
    auto pDesign = std::make_unique<UnitDesign>(slots, assigned);
    const UnitDesign& rDesign = *pDesign;
    REQUIRE(rFixture.pPlayer->GetMilitary().AddDesign(std::move(pDesign)));
    return rDesign;
}

} // namespace

TEST_CASE("Disabled facility-built notice completes without a modal and advances",
          "[ui][InteractionPresenter][PlayerInteraction]")
{
    PresenterHarness_ harness;
    PauseOnEventsConfig_t config = harness.fixture.settings.GetPauseOnEvents();
    config.newFacilityBuilt = false;
    harness.fixture.settings.SetPauseOnEvents(config);

    harness.Enqueue(NoticeInteraction_t{
        PauseOnEventId_t::NewFacilityBuilt, "Title", "Body", std::nullopt});
    REQUIRE(harness.fixture.pState->GetPlayerInteractions().Size() == 1);

    harness.pPresenter->Update();

    CHECK(harness.fixture.pState->GetPlayerInteractions().Empty());
    CHECK(harness.advanceCount == 1);
    CHECK_FALSE(harness.pWorldView->HasModalElement());
}

TEST_CASE("Disabled combat-unit-built skips the completion prompt",
          "[ui][InteractionPresenter][PlayerInteraction]")
{
    PresenterHarness_ harness;
    PauseOnEventsConfig_t config = harness.fixture.settings.GetPauseOnEvents();
    config.combatUnitBuilt = false;
    harness.fixture.settings.SetPauseOnEvents(config);

    BaseManager& rBase = harness.fixture.MakeBase(4, 4);
    harness.Enqueue(ProductionIdleInteraction_t{
        harness.fixture.pPlayer->GetFactionId(),
        rBase.GetBaseId(),
        true,
        "item",
        "Laser Infantry",
        PauseOnEventId_t::CombatUnitBuilt,
    });

    harness.pPresenter->Update();

    CHECK(harness.fixture.pState->GetPlayerInteractions().Empty());
    CHECK(harness.advanceCount == 1);
    CHECK_FALSE(harness.pWorldView->HasModalElement());
}

TEST_CASE("Disabled build-orders-out-of-date skips the idle prompt",
          "[ui][InteractionPresenter][PlayerInteraction]")
{
    PresenterHarness_ harness;
    PauseOnEventsConfig_t config = harness.fixture.settings.GetPauseOnEvents();
    config.buildOrdersOutOfDate = false;
    harness.fixture.settings.SetPauseOnEvents(config);

    BaseManager& rBase = harness.fixture.MakeBase(4, 4);
    harness.Enqueue(ProductionIdleInteraction_t{
        harness.fixture.pPlayer->GetFactionId(),
        rBase.GetBaseId(),
        false,
        {},
        {},
        std::nullopt,
    });

    harness.pPresenter->Update();

    CHECK(harness.fixture.pState->GetPlayerInteractions().Empty());
    CHECK(harness.advanceCount == 1);
    CHECK_FALSE(harness.pWorldView->HasModalElement());
}

TEST_CASE("Production abandon still presents when pause-on-event flags are off",
          "[ui][InteractionPresenter][PlayerInteraction][abandon]")
{
    PresenterHarness_ harness;
    PauseOnEventsConfig_t config{};
    config.newFacilityBuilt = false;
    config.nonCombatUnitBuilt = false;
    config.combatUnitBuilt = false;
    config.prototypeBuilt = false;
    config.droneRiots = false;
    config.endOfDroneRiots = false;
    config.goldenAgeStarts = false;
    config.endOfGoldenAge = false;
    config.nutrientLow = false;
    config.buildOrdersOutOfDate = false;
    config.populationLimitReached = false;
    config.delayInTranscendence = false;
    harness.fixture.settings.SetPauseOnEvents(config);

    BaseManager& rBase = harness.fixture.MakeBase(4, 4);
    while (rBase.GetPopulation().GetSize() > 1)
    {
        rBase.GetPopulation().RemovePop();
    }

    const UnitDesign& rPod = AddPodDesign_(harness.fixture);
    rBase.GetProduction().SetProduction(&rPod, rBase.GetBaseEffects());
    rBase.GetProduction().SetMineralStockpile(rBase.GetMineralCost());
    REQUIRE(rBase.ApplyProduction().kind == ProductionApplyKind_t::AwaitingAbandonConfirm);
    REQUIRE(rBase.HasPendingProductionAbandonConfirm());

    harness.Enqueue(ProductionAbandonInteraction_t{
        harness.fixture.pPlayer->GetFactionId(),
        rBase.GetBaseId(),
    });

    harness.pPresenter->Update();

    CHECK(rBase.HasPendingProductionAbandonConfirm());
    CHECK(harness.advanceCount == 0);
    CHECK(harness.pWorldView->HasModalElement());
    CHECK(harness.fixture.pState->GetPlayerInteractions().Size() == 1);
}
