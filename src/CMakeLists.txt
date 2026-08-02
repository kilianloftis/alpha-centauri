# ---------------------------------------------------------------------------
# ac-core: all game/engine logic, free of rendering/input backends, UI, and
# the engine loop. Shared between the game executable and the test suite.
# ---------------------------------------------------------------------------
add_library(ac-core STATIC
    game/GameState.cpp
    game/GameSettings.cpp
    game/GameDataContext.cpp
    game/EffectReferenceValidator.cpp
    game/RequiredTechValidator.cpp
    game/Faction.cpp
    game/faction/FactionEffectsPool.cpp
    game/faction/FactionIdentity.cpp
    game/faction/AIProfile.cpp
    game/faction/FactionFlavor.cpp
    game/faction/Military.cpp
    game/faction/UnitManager.cpp
    game/research/TechRegistry.cpp
    game/research/TechCostCalculator.cpp
    game/research/TechCostConfigParser.cpp
    game/faction/ResearchManager.cpp
    game/faction/ResearchSelector.cpp
    game/GameCategory.cpp
    game/faction/DiplomacyLedger.cpp
    game/faction/DiplomacyActions.cpp
    game/faction/TradeItem.cpp
    game/faction/DiplomaticActionExecutor.cpp
    game/faction/FirstContactResolver.cpp
    game/council/CouncilAiStub.cpp
    game/council/CouncilEffects.cpp
    game/council/CouncilOutcomeApplier.cpp
    game/council/CouncilProposalConfigParser.cpp
    game/council/CouncilRulesConfigParser.cpp
    game/council/PlanetaryCouncil.cpp
    game/faction/SocialEngineeringManager.cpp
    game/faction/base/BaseManager.cpp
    game/faction/base/HomeBaseIndex.cpp
    game/faction/base/buildings/BuildingManager.cpp
    game/faction/base/production/ProductionManager.cpp
    game/faction/base/production/ProductionCostCalculator.cpp
    game/faction/base/resources/ResourceManager.cpp
    game/faction/base/resources/WorkerAssignmentManager.cpp
    game/faction/EconomyManager.cpp
    game/faction/base/population/PopulationManager.cpp
    game/faction/base/population/PopContainer.cpp
    game/population/pop-types/Pop.cpp
    game/population/pop-types/PopTypeConfigParser.cpp
    game/population/pop-types/PopCompositionConfigParser.cpp
    game/population/pop-types/GrowthConfigParser.cpp
    game/population/calculators/PopCompositionCalculator.cpp
    game/population/calculators/RiotCalculator.cpp
    game/population/calculators/GrowthCalculator.cpp
    game/population/calculators/GoldenAgeCalculator.cpp
    game/population/calculators/PopTypeAvailabilityCalculator.cpp
    game/faction/Specialist.cpp
    game/TurnProcessor.cpp
    lib/EventBus.cpp
    lib/LuaRuntime.cpp
    lib/config/ConfigFields.cpp
    game/EventBridge.cpp
    game/effects/ActiveEffect.cpp
    game/effects/BonusEffectParser.cpp
    game/effects/InfiltrationRules.cpp
    game/effects/TileEffectsContext.cpp
    game/effects/TileYieldRulesConfigParser.cpp
    game/TurnStageFactory.cpp
    game/TurnStageConfigParser.cpp
    game/HookContext.cpp
    game/buildings/BuildingConfigParser.cpp
    game/buildings/SecretProjectAvailabilityCalculator.cpp
    game/orbital/OrbitalAttack.cpp
    game/orbital/OrbitalCensus.cpp
    game/units/Unit.cpp
    game/units/UnitDesign.cpp
    game/units/UnitOrder.cpp
    game/units/UnitOrderExecutor.cpp
    game/units/CombatResolver.cpp
    game/units/MoraleConfig.cpp
    game/units/MoraleConfigParser.cpp
    game/units/MoraleCalculator.cpp
    game/units/DisengageRules.cpp
    game/units/InterceptRules.cpp
    game/units/MovementRules.cpp
    game/units/FoundBaseRules.cpp
    game/units/TerraformRules.cpp
    game/units/ProbeActionConfigParser.cpp
    game/units/ProbeActionEffects.cpp
    game/units/ProbeActionExecutor.cpp
    game/units/ProbeRules.cpp
    game/units/ProbeTarget.cpp
    game/units/StepEvaluator.cpp
    game/units/Pathfinder.cpp
    game/units/MoveCostCalculator.cpp
    game/units/UnitComponentConfigParser.cpp
    game/units/UnitSlotConfigParser.cpp
    game/research/TechConfigParser.cpp
    game/faction/FactionConfigParser.cpp
    game/social-engineering/SocialPolicyConfigParser.cpp
    game/social-engineering/SocialRatingConfigParser.cpp
    game/social-engineering/SocialRatingResolver.cpp
    game/map/Tile.cpp
    game/map/TileLayerResolver.cpp
    game/map/UnitPositionIndex.cpp
    game/map/WorkedTileIndex.cpp
    game/map/TileFlagMap.cpp
    game/map/TerritoryMap.cpp
    game/faction/FactionVisibleMap.cpp
    game/faction/VisibilityRules.cpp
    game/faction/UnitVisibility.cpp
    game/map/ImprovementConfigParser.cpp
    game/map/TerraformSpread.cpp
    game/map/WorldGenPresetConfigParser.cpp
    game/map/WorldGenDecorationConfigParser.cpp
    game/map/LandmarkConfigParser.cpp
    game/map/LandmarkGeneration.cpp
    game/map/FungusGeneration.cpp
    game/map/TileBonusGeneration.cpp
    game/map/MapGenerationConfig.cpp
    game/map/RiverGeneration.cpp
    game/map/FbmNoise.cpp
    game/map/WorldMap.cpp
    game/map/WorldGenerator.cpp
    game/stages/CustomTurnStage.cpp
    lib/Rational.cpp
)

# Built-in turn stages self-register via file-scope TurnStageRegistrar<T> instances.
# Those symbols are never named from outside their TUs, so a STATIC archive would
# drop them at link time and every stage would fall through to Custom*TurnStage.
# An OBJECT library is always pulled into the final link, so the registrars run.
add_library(ac-turn-stages OBJECT
    game/stages/TurnStart.cpp
    game/stages/ResourceCollection.cpp
    game/stages/IncomeCollection.cpp
    game/stages/ResearchAccumulation.cpp
    game/stages/BaseProduction.cpp
    game/stages/Population.cpp
    game/stages/Upkeep.cpp
    game/stages/PlayerActions.cpp
    game/stages/WorldEvents.cpp
    game/stages/VictoryConditionChecks.cpp
    game/stages/TurnEnd.cpp
    game/stages/Save.cpp
)

target_include_directories(ac-turn-stages PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(ac-core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Add nlohmann/json library
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)
target_link_libraries(ac-core PUBLIC nlohmann_json::nlohmann_json)
# Stages compile against the same public headers/deps as ac-core (HookContext, etc.).
target_link_libraries(ac-turn-stages PUBLIC ac-core)

# Add magic_enum library
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.6
)
FetchContent_MakeAvailable(magic_enum)
target_link_libraries(ac-core PUBLIC magic_enum::magic_enum)

# Add Lua 5.4
FetchContent_Declare(
    lua
    URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
    URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_MakeAvailable(lua)
    set(LUA_SRC ${lua_SOURCE_DIR}/src)
    add_compile_definitions(LUA_USE_LINUX)
    file(GLOB LUA_SOURCES
        ${LUA_SRC}/lapi.c ${LUA_SRC}/lcode.c ${LUA_SRC}/lctype.c
        ${LUA_SRC}/ldebug.c ${LUA_SRC}/ldo.c ${LUA_SRC}/ldump.c
        ${LUA_SRC}/lfunc.c ${LUA_SRC}/lgc.c ${LUA_SRC}/linit.c
        ${LUA_SRC}/llex.c ${LUA_SRC}/lmem.c ${LUA_SRC}/lobject.c
        ${LUA_SRC}/lopcodes.c ${LUA_SRC}/lparser.c ${LUA_SRC}/lstate.c
        ${LUA_SRC}/lstring.c ${LUA_SRC}/ltable.c ${LUA_SRC}/ltm.c
        ${LUA_SRC}/lundump.c ${LUA_SRC}/lvm.c ${LUA_SRC}/lzio.c
        ${LUA_SRC}/lauxlib.c ${LUA_SRC}/lbaselib.c ${LUA_SRC}/lcorolib.c
        ${LUA_SRC}/ldblib.c ${LUA_SRC}/liolib.c ${LUA_SRC}/lmathlib.c
        ${LUA_SRC}/loadlib.c ${LUA_SRC}/loslib.c ${LUA_SRC}/lstrlib.c
        ${LUA_SRC}/ltablib.c ${LUA_SRC}/lutf8lib.c
    )
    add_library(lua54 STATIC ${LUA_SOURCES})
    target_include_directories(lua54 PUBLIC ${LUA_SRC})
    set_target_properties(lua54 PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_link_libraries(ac-core PUBLIC lua54)

# Add sol2 (header-only)
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(sol2)
target_link_libraries(ac-core PUBLIC sol2)

# FastNoiseLite (header-only; upstream has no CMake package target)
FetchContent_Declare(
    fastnoiselite
    GIT_REPOSITORY https://github.com/Auburn/FastNoiseLite.git
    GIT_TAG v1.1.1
)
FetchContent_MakeAvailable(fastnoiselite)
add_library(fastnoiselite INTERFACE)
target_include_directories(fastnoiselite INTERFACE ${fastnoiselite_SOURCE_DIR}/Cpp)
target_link_libraries(ac-core PUBLIC fastnoiselite)

# ---------------------------------------------------------------------------
# alpha-centauri: the game executable — engine loop, UI, and the selected
# rendering/input backend on top of ac-core.
# ---------------------------------------------------------------------------
add_executable(alpha-centauri
    main.cpp
    game/Engine.cpp
    input/KeyMapping.cpp
    input/KeyEventQueue.cpp
    input/MouseEventQueue.cpp
    ui/TileHitTester.cpp
    ui/TileRenderer.cpp
    ui/UIManager.cpp
    ui/ViewFactory.cpp
    ui/style/UiStyle.cpp
    ui/world/WorldView.cpp
    ui/world/SupplyCrawlPopup.cpp
    ui/world/ProbeActionPopup.cpp
    ui/world/CameraInputController.cpp
    ui/world/UnitOrderInputController.cpp
    ui/world/TerraformInputController.cpp
    ui/world/CombatPresentation.cpp
    ui/world/CombatView.cpp
    ui/world/MapViewport.cpp
    ui/world/WorldDisplay.cpp
    ui/world/UnitMarkerRenderer.cpp
    ui/world/InfoPanelElement.cpp
    ui/world/LocationPanel.cpp
    ui/world/SelectedUnitPanel.cpp
    ui/world/UnitStackPanel.cpp
    ui/world/EndTurnButton.cpp
    ui/world/MinimapDisplay.cpp
    ui/world/CommlinksButton.cpp
    ui/commlinks/CommlinksView.cpp
    ui/commlinks/CommlinksPanel.cpp
    ui/commlinks/CouncilButton.cpp
    ui/commlinks/CouncilCooldownPopup.cpp
    ui/commlinks/CouncilProposalsPopup.cpp
    ui/council/CouncilVoteView.cpp
    ui/council/CouncilFactionVotesPanel.cpp
    ui/council/CouncilProposalInfoPanel.cpp
    ui/council/CouncilVoteButton.cpp
    ui/council/CouncilBallotPopup.cpp
    ui/satellite/SatelliteView.cpp
    ui/satellite/SatelliteLabeledButton.cpp
    ui/satellite/SatelliteSummaryPanel.cpp
    ui/satellite/SatelliteButtonListPanel.cpp
    ui/satellite/OrbitalAttackerPopup.cpp
    ui/satellite/OrbitalAttackOutcomePopup.cpp
    ui/PlaceholderPanel.cpp
    ui/base/BaseView.cpp
    ui/base/BaseNameDisplay.cpp
    ui/base/BaseWorkableAreaDisplay.cpp
    ui/base/GrowthDisplay.cpp
    ui/base/ProductionDisplay.cpp
    ui/base/ProductionSelectorPopup.cpp
    ui/base/PopulationDisplay.cpp
    ui/base/PopTypeSelectorPopup.cpp
    ui/base/SupportDisplay.cpp
    ui/research/ResearchView.cpp
    ui/research/CurrentResearchPanel.cpp
    ui/settings/SettingsView.cpp
    ui/settings/SettingsPanel.cpp
    ui/social-engineering/SocialEngineeringView.cpp
    ui/social-engineering/SocialEngineeringDisplay.cpp
    ui/social-engineering/SocialEngineeringBottomPanel.cpp
    ui/unit-designer/SlotColumnPanel.cpp
    ui/unit-designer/ComponentSlotDisplay.cpp
    ui/unit-designer/ComponentSelectorPopup.cpp
    ui/unit-designer/DesignStatsDisplay.cpp
    ui/unit-designer/DesignListPanel.cpp
    ui/unit-designer/UnitStatusPanel.cpp
    ui/unit-designer/UnitDesignerView.cpp
)

target_link_libraries(alpha-centauri PRIVATE ac-core ac-turn-stages)

option(USE_SFML "Enable SFML rendering" ON)
if(USE_SFML)
    find_package(SFML 3.0.1 COMPONENTS Graphics Window System REQUIRED)
    target_compile_definitions(alpha-centauri PRIVATE USE_SFML)
    target_sources(alpha-centauri PRIVATE graphics/SFMLGraphics.cpp input/SFMLInput.cpp)
    target_link_libraries(alpha-centauri PRIVATE SFML::Graphics SFML::Window SFML::System)
    if(UNIX AND NOT APPLE)
        find_package(X11 REQUIRED)
        target_link_libraries(alpha-centauri PRIVATE X11::X11)
    endif()
else()
    target_sources(alpha-centauri PRIVATE graphics/NullGraphics.cpp input/NullInput.cpp)
endif()
