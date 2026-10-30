#pragma once

#include "GameFixtures.h"
#include "RecordingGraphics.h"
#include "SpriteRig.h"
#include "TestTempDir.h"

#include "game/CurrentGameState.h"
#include "game/faction/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitSlotRegistry.h"
#include "ui/HotkeyConfig.h"
#include "ui/MapRenderer.h"
#include "ui/ViewFactory.h"
#include "ui/style/UiStyle.h"

#include <filesystem>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

namespace actest
{

// A live session and a ViewFactory over it, so views can be built and rendered the way Engine
// builds them. Views were unreachable from tests until ac-ui (backend-free) let the test target
// link them; this supplies the registries and loaded style they need. The WorldFixture session
// is the game state the views read.
struct ViewFixture : WorldFixture
{
    RecordingGraphics graphics;
    ac::Faction* pPlayer = nullptr;
    std::unique_ptr<ac::HotkeyConfig> pHotkeys;
    std::unique_ptr<SpriteRig> pSprites;
    std::unique_ptr<ac::MapRenderer> pMapRenderer;
    std::unique_ptr<ac::ViewFactory> pFactory;
    // The settings UI saves on every toggle, so each fixture saves to a file of its own rather
    // than the developer's user_settings.json or a file another test process shares.
    std::filesystem::path settingsPath = actest::TestTempDir::Path()
        / ("ac_view_settings_" + std::to_string(std::random_device{}()) + ".json");

    // Loaded once per process — UiStyle is still a singleton (deferred, package 14), so every
    // test loads this same file.
    static void EnsureStyleLoaded()
    {
        static const bool bLoaded = []
        {
            ac::UiStyle::Load(FixturePath("ui/style.json"));
            return true;
        }();
        (void)bLoaded;
    }

    explicit ViewFixture(int mapHeight = k_TestMapHeight)
        : WorldFixture(k_TestMapWidth, mapHeight)
    {
        EnsureStyleLoaded();

        // A missing file keeps the defaults and becomes where Save writes.
        settings.Load(settingsPath.string());

        factionDefinition.id = "test_faction";
        factionDefinition.identity.name = "Test Faction";

        // ViewFactory reads the slot registry off the data context; SessionInputs does not
        // load one.
        dataContext.unitSlotRegistry = std::make_unique<ac::UnitSlotRegistry>();
        dataContext.unitSlotRegistry->Load(FixturePath("unit_slots.json"));

        // Moist tiles so worked tiles actually yield something: a default WorldMap produces
        // zero nutrients everywhere, which makes every yield assertion vacuously true.
        for (const auto& pTile : map.GetTiles())
        {
            pTile->SetMoisture(ac::Moisture_t::Wet);
        }

        pPlayer = &state.AddFaction(std::make_unique<ac::Faction>(
            state.AllocateFactionId(), /*bIsPlayerControlled*/ true, factionDefinition,
            dataContext, map, settings, state.GetUnitIdAllocator(), k_TestFactionSeed));

        // WorldView reads player hotkeys from this path; relative defaults break under ctest's
        // build-dir cwd.
        dataContext.paths.hotkeys = FixturePath("hotkeys.json");
        pHotkeys = std::make_unique<ac::HotkeyConfig>(ac::HotkeyConfig::Load(
            dataContext.paths.hotkeys, *dataContext.improvementRegistry,
            *dataContext.terrainOperationRegistry));

        pSprites = std::make_unique<SpriteRig>(graphics, ac::Style().tileRenderer);
        pMapRenderer = std::make_unique<ac::MapRenderer>(
            pSprites->sprites, state, ac::Style().tileRenderer, ac::Style().mapRenderer);
        pFactory = std::make_unique<ac::ViewFactory>(
            state, dataContext, *pHotkeys, graphics, *pMapRenderer, pSprites->sprites,
            settings);
    }

    ~ViewFixture()
    {
        std::error_code ignored;
        std::filesystem::remove(settingsPath, ignored);
    }

    ac::BaseManager& MakeBase(int x, int y)
    {
        ac::BaseManager* pBase = pPlayer->CreateBase(
            state.AllocateBaseId(), "TestBase", map.GetTile(x, y),
            ctx, std::nullopt);
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
