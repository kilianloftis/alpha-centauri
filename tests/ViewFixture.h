#pragma once

#include "GameFixtures.h"
#include "RecordingGraphics.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"
#include "ui/ViewFactory.h"
#include "ui/style/UiStyle.h"

#include <memory>
#include <stdexcept>
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

        // The settings UI saves on every toggle. Without this, a test that clicks a settings row
        // rewrites the developer's real user_settings.json in the working directory.
        settings.SetSavePath(std::string(AC_TEST_FIXTURES_DIR) + "/../../build/test_settings.json");

        factionDefinition.id = "test_faction";
        factionDefinition.identity.name = "Test Faction";

        // ViewFactory reads these two off the data context; WorldFixture loads the component
        // registry as a standalone member for the rules tests, so it is loaded again here.
        dataContext.unitComponentRegistry = std::make_unique<ac::UnitComponentRegistry>();
        dataContext.unitComponentRegistry->Load(FixturePath("unit_components.json"));
        dataContext.unitSlotRegistry = std::make_unique<ac::UnitSlotRegistry>();
        dataContext.unitSlotRegistry->Load(FixturePath("unit_slots.json"));

        auto pMap = std::make_unique<ac::WorldMap>(9, 9);
        // Moist tiles so worked tiles actually yield something: a default WorldMap produces
        // zero nutrients everywhere, which makes every yield assertion vacuously true.
        for (int y = 0; y < pMap->GetHeight(); ++y)
        {
            for (int x = 0; x < pMap->GetWidth(); ++x)
            {
                pMap->GetTile(x, y)->SetMoisture(ac::Moisture_t::Wet);
            }
        }

        pState = std::make_unique<ac::GameState>(
            std::move(pMap), improvements, &unitComponents, settings,
            morale(), k_TestRngSeed);

        if (bWithPlayerFaction)
        {
            pPlayer = &pState->AddFaction(std::make_unique<ac::Faction>(
                pState->AllocateFactionId(), /*bIsPlayerControlled*/ true, factionDefinition,
                dataContext, pState->GetWorldMap(), settings, k_TestFactionSeed));
        }

        pFactory = std::make_unique<ac::ViewFactory>(*pState, dataContext, graphics, settings);
    }

    ac::BaseManager& MakeBase(int x, int y)
    {
        ac::BaseManager* pBase = pPlayer->CreateBase(
            pState->AllocateBaseId(), "TestBase", pState->GetWorldMap().GetTile(x, y),
            dataContext, pState->GetTileEffects(), pState->GetSecretProjectAvailability());
        if (!pBase)
        {
            throw std::runtime_error("ViewFixture: base creation failed");
        }
        return *pBase;
    }

    static ac::WindowLayout_t FullScreen()
    {
        return ac::WindowLayout_t{0.0f, 0.0f, 1280.0f, 900.0f};
    }
};

} // namespace actest
