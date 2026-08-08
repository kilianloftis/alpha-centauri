#pragma once

#include "GameFixtures.h"
#include "RecordingGraphics.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"
#include "ui/ViewFactory.h"
#include "ui/style/UiStyle.h"

#include <memory>
#include <string>

namespace actest
{

// A live session and a ViewFactory over it, so views can be built and rendered the way Engine
// builds them. Views were unreachable from tests until ac-ui (backend-free) let the test target
// link them; this supplies the GameState, registries and loaded style they need.
//
// WorldFixture's own map goes unused here: GameState owns the map the views read.
struct ViewFixture : WorldFixture
{
    RecordingGraphics graphics;
    ac::FactionConfig_t factionDefinition;
    std::unique_ptr<ac::GameState> pState;
    ac::Faction* pPlayer = nullptr;
    std::unique_ptr<ac::ViewFactory> pFactory;

    // The shipped style, not a hand-built one: views index into it constantly and a test copy
    // would drift. Loaded once per process — UiStyle is still a singleton (deferred, package 14).
    static void EnsureStyleLoaded()
    {
        static const bool bLoaded = []
        {
            ac::UiStyle::Load(std::string(AC_CONFIG_DIR) + "/ui/style.json");
            return true;
        }();
        (void)bLoaded;
    }

    explicit ViewFixture(bool bWithPlayerFaction = true)
    {
        EnsureStyleLoaded();

        factionDefinition.id = "test_faction";
        factionDefinition.identity.name = "Test Faction";

        // ViewFactory reads these two off the data context; WorldFixture loads the component
        // registry as a standalone member for the rules tests, so it is loaded again here.
        dataContext.unitComponentRegistry = std::make_unique<ac::UnitComponentRegistry>();
        dataContext.unitComponentRegistry->Load(FixturePath("unit_components.json"));
        dataContext.unitSlotRegistry = std::make_unique<ac::UnitSlotRegistry>();
        dataContext.unitSlotRegistry->Load(FixturePath("unit_slots.json"));

        pState = std::make_unique<ac::GameState>(
            std::make_unique<ac::WorldMap>(9, 9), improvements, &unitComponents, settings,
            morale(), k_TestRngSeed);

        if (bWithPlayerFaction)
        {
            pPlayer = &pState->AddFaction(std::make_unique<ac::Faction>(
                pState->AllocateFactionId(), /*bIsPlayerControlled*/ true, factionDefinition,
                dataContext, pState->GetWorldMap(), settings, k_TestFactionSeed));
        }

        pFactory = std::make_unique<ac::ViewFactory>(*pState, dataContext, graphics, settings);
    }

    static ac::WindowLayout_t FullScreen()
    {
        return ac::WindowLayout_t{0.0f, 0.0f, 1280.0f, 900.0f};
    }
};

} // namespace actest
