# ---------------------------------------------------------------------------
# ac-core: all game/engine logic, free of rendering/input backends, UI, and
# the engine loop. Shared between the game executable and the test suite.
# ---------------------------------------------------------------------------
add_library(ac-core STATIC
    game/GameState.cpp
    game/CurrentGameState.cpp
    game/GameSettings.cpp
    game/GameDataContext.cpp
    game/DifficultyConfig.cpp
    game/DifficultyConfigParser.cpp
    game/EffectReferenceValidator.cpp
    game/RequiredTechValidator.cpp
    game/faction/Faction.cpp
    game/faction/FactionEffectsPool.cpp
    game/faction/FactionIdentity.cpp
    game/faction/AIProfile.cpp
    game/faction/FactionFlavor.cpp
    game/units/Military.cpp
    game/units/UnitManager.cpp
    game/base/RebelRules.cpp
    game/base/RebelMutations.cpp
    game/research/TechRegistry.cpp
    game/research/TechCostCalculator.cpp
    game/research/TechCostConfigParser.cpp
    game/research/ResearchManager.cpp
    game/research/ResearchRules.cpp
    game/research/ResearchSelector.cpp
    game/GameCategory.cpp
    game/diplomacy/DiplomaticStatus.cpp
    game/diplomacy/DiplomacyConfigParser.cpp
    game/diplomacy/DiplomacyLedger.cpp
    game/diplomacy/DiplomaticPermissionRules.cpp
    game/diplomacy/DiplomacyActions.cpp
    game/diplomacy/DiplomaticTransitionMutations.cpp
    game/diplomacy/DiplomaticTransitionRules.cpp
    game/diplomacy/TradeItem.cpp
    game/diplomacy/DiplomaticActionExecutor.cpp
    game/diplomacy/DiplomaticProposalMutations.cpp
    game/diplomacy/DiplomaticProposalRules.cpp
    game/diplomacy/FirstContactResolver.cpp
    game/council/CouncilAiStub.cpp
    game/council/CouncilEffects.cpp
    game/council/CouncilOutcomeApplier.cpp
    game/council/CouncilProposalConfigParser.cpp
    game/council/CouncilRulesConfigParser.cpp
    game/council/PlanetaryCouncil.cpp
    game/atrocities/AtrocityConfig.cpp
    game/atrocities/AtrocityConfigParser.cpp
    game/atrocities/AtrocityMutations.cpp
    game/atrocities/AtrocityLedger.cpp
    game/atrocities/AtrocityRules.cpp
    game/probes/MindControlLedger.cpp
    game/ecology/EcologyLedger.cpp
    game/ecology/EcoDamageConfigParser.cpp
    game/ecology/EcoDamageCalculator.cpp
    game/ecology/BaseEcology.cpp
    game/ecology/EcologyRules.cpp
    game/natives/NativeLifeLevelConfig.cpp
    game/world-events/WorldEventConfig.cpp
    game/world-events/WorldEventTracker.cpp
    game/social-engineering/SocialEngineeringManager.cpp
    game/social-engineering/SocialEngineeringRules.cpp
    game/base/BaseManager.cpp
    game/base/HomeBaseIndex.cpp
    game/buildings/BuildingManager.cpp
    game/production/ProductionManager.cpp
    game/production/ProductionConfigParser.cpp
    game/production/HurryProductionCalculator.cpp
    game/production/ScrapConfigParser.cpp
    game/production/ProductionRules.cpp
    game/production/ProductionMutations.cpp
    game/production/ScrapRefundCalculator.cpp
    game/economy/ResourceManager.cpp
    game/economy/EconomyRules.cpp
    game/economy/EconomyMutations.cpp
    game/economy/WorkerAssignmentManager.cpp
    game/economy/EconomyManager.cpp
    game/economy/CommerceConfigParser.cpp
    game/economy/CommerceCalculator.cpp
    game/economy/CommerceRules.cpp
    game/economy/CommerceCache.cpp
    game/base/BaseEffectsCache.cpp
    game/buildings/BuildingMutations.cpp
    game/base/BaseMoodEffects.cpp
    game/base/BaseRules.cpp
    game/base/BaseRuleset.cpp
    game/production/ProductionCompletion.cpp
    game/production/ProductionPauseGates.cpp
    game/population/AwayFromHomeDrones.cpp
    game/population/CompositionInputs.cpp
    game/population/GarrisonPolice.cpp
    game/population/PopulationManager.cpp
    game/population/PopulationRules.cpp
    game/population/AssimilationTracker.cpp
    game/population/PopContainer.cpp
    game/population/Pop.cpp
    game/population/PopTypeConfigParser.cpp
    game/population/PopCompositionConfigParser.cpp
    game/population/GrowthConfigParser.cpp
    game/population/DroneCalculator.cpp
    game/population/PopCompositionCalculator.cpp
    game/population/MoodLatch.cpp
    game/population/RiotLatch.cpp
    game/population/GrowthRules.cpp
    game/population/GoldenAgeLatch.cpp
    game/population/PopTypeAvailabilityCalculator.cpp
    game/stages/TurnProcessor.cpp
    lib/EventBus.cpp
    lib/LuaRuntime.cpp
    lib/config/ConfigFields.cpp
    game/EventBridge.cpp
    game/PlayerInteractionQueue.cpp
    game/effects/ActiveEffect.cpp
    game/effects/EffectConfigParser.cpp
    game/diplomacy/InfiltrationRules.cpp
    game/effects/TileEffectsContext.cpp
    game/effects/TriggeredEffectDispatch.cpp
    game/effects/TriggeredEffectRules.cpp
    game/effects/TriggeredEffectParser.cpp
    game/effects/TileYieldRulesConfigParser.cpp
    game/effects/InteractionGridsConfigParser.cpp
    game/effects/InteractionResolve.cpp
    game/population/PoliceRulesConfigParser.cpp
    game/effects/WorldRulesConfigParser.cpp
    game/stages/TurnStageFactory.cpp
    game/stages/TurnStageConfigParser.cpp
    game/stages/HookContext.cpp
    game/buildings/BuildingConfigParser.cpp
    game/display/MapOverlayChannelsConfigParser.cpp
    game/display/BaseSpriteSizesConfigParser.cpp
    game/buildings/BuildingUpkeep.cpp
    game/buildings/SecretProjectAvailabilityRules.cpp
    game/production/StockpileConfigParser.cpp
    game/production/StockpileConversion.cpp
    game/buildings/BuildingRules.cpp
    game/buildings/OrbitalCensus.cpp
    game/units/Unit.cpp
    game/units/IDesign.cpp
    game/units/UnitDesign.cpp
    game/natives/NativeDesign.cpp
    game/natives/NativeUnitConfigParser.cpp
    game/natives/NativeUnitRegistry.cpp
    game/natives/EnsureNativeDesign.cpp
    game/natives/NativeRules.cpp
    game/units/UnitDesignAvailability.cpp
    game/units/UnitOrder.cpp
    game/units/UnitOrderExecutor.cpp
    game/units/UnitPlacementRules.cpp
    game/combat/CombatResolver.cpp
    game/combat/CombatRules.cpp
    game/combat/CombatStrength.cpp
    game/combat/StackCollateral.cpp
    game/combat/PlanetPearls.cpp
    game/combat/MoraleConfig.cpp
    game/combat/MoraleConfigParser.cpp
    game/combat/MoraleCalculator.cpp
    game/combat/MoraleRules.cpp
    game/combat/MoraleMutations.cpp
    game/combat/DisengageRules.cpp
    game/movement/EvacuateTerritoryRules.cpp
    game/movement/EvacuateTerritoryMutations.cpp
    game/movement/TerritoryEntryMutations.cpp
    game/combat/InterceptMutations.cpp
    game/combat/InterceptRules.cpp
    game/combat/ScrambleRules.cpp
    game/movement/MovementRules.cpp
    game/movement/TransportRules.cpp
    game/movement/TransportMutations.cpp
    game/movement/FuelRules.cpp
    game/movement/FuelMutations.cpp
    game/combat/AttackRules.cpp
    game/base/FoundBaseRules.cpp
    game/base/BaseMutations.cpp
    game/movement/AirdropRules.cpp
    game/terraform/TerraformRules.cpp
    game/terraform/TerraformMutations.cpp
    game/units/AdHocDesign.cpp
    game/base/BaseConquestConfigParser.cpp
    game/base/BaseConquestRules.cpp
    game/base/BaseConquestMutations.cpp
    game/probes/ProbeActionConfigParser.cpp
    game/probes/ProbeActionMutations.cpp
    game/probes/ProbeActionExecutor.cpp
    game/probes/ProbeRules.cpp
    game/probes/ProbeTarget.cpp
    game/movement/StepEvaluator.cpp
    game/movement/Pathfinder.cpp
    game/movement/MoveCostRules.cpp
    game/units/UnitComponentConfigParser.cpp
    game/units/UnitSlotConfigParser.cpp
    game/research/TechConfigParser.cpp
    game/faction/FactionConfigParser.cpp
    game/social-engineering/SocialPolicyConfigParser.cpp
    game/social-engineering/SocialRatingConfigParser.cpp
    game/social-engineering/SocialRatingResolver.cpp
    game/map/Tile.cpp
    game/display/OccupantArtParser.cpp
    game/map/UnitPositionIndex.cpp
    game/map/WorkedTileIndex.cpp
    game/map/TileFlagMap.cpp
    game/map/TerritoryMap.cpp
    game/map/TerritoryRules.cpp
    game/visibility/FactionVisibleMap.cpp
    game/visibility/FactionTileMemory.cpp
    game/visibility/VisibilityMutations.cpp
    game/visibility/VisibilityRules.cpp
    game/map/ImprovementConfigParser.cpp
    game/map/OccupantCoexistence.cpp
    game/map/TerrainConfigParser.cpp
    game/effects/CoexistenceResolve.cpp
    game/map/ElevationRulesConfigParser.cpp
    game/terraform/ElevationChange.cpp
    game/terraform/SurfaceOccupancy.cpp
    game/map/TerrainFeatureValidation.cpp
    game/terraform/TerraformSpread.cpp
    game/worldgen/WorldGenPresetConfigParser.cpp
    game/worldgen/WorldGenDecorationConfigParser.cpp
    game/worldgen/LandmarkConfigParser.cpp
    game/worldgen/LandmarkGeneration.cpp
    game/worldgen/FungusGeneration.cpp
    game/ecology/FungalBloom.cpp
    game/combat/ExplosionMutations.cpp
    game/combat/ExplosionRules.cpp
    game/worldgen/TileBonusGeneration.cpp
    game/worldgen/MapGenerationConfig.cpp
    game/worldgen/MoistureGeneration.cpp
    game/map/RiverGeneration.cpp
    game/worldgen/FbmNoise.cpp
    game/map/WorldMap.cpp
    game/worldgen/WorldGenerator.cpp
    game/stages/CustomTurnStage.cpp
    lib/Rational.cpp
    input/BufferedInput.cpp
    input/PlatformEventQueue.cpp
)

# Exhaustive-switch enforcement. Several enums (TerrainFeature_t, EffectScope_t, ...) are
# deliberately handled by unqualified switches so that adding an enumerator breaks the build
# at every site that must make a decision about it, rather than silently taking a default.
# Promoted to an error because a mere warning is lost in build output.
if(NOT MSVC)
    target_compile_options(ac-core PRIVATE -Wswitch -Werror=switch)
endif()

# Built-in turn stages self-register via file-scope TurnStageRegistrar<T> instances.
# Those symbols are never named from outside their TUs, so a STATIC archive would
# drop them at link time and every stage would fall through to Custom*TurnStage.
# An OBJECT library is always pulled into the final link, so the registrars run.
add_library(ac-turn-stages OBJECT
    game/stages/YieldingPerFactionTurnStage.cpp
    game/stages/TurnStart.cpp
    game/stages/ResourceCollection.cpp
    game/stages/BaseGrowth.cpp
    game/stages/UnitSupport.cpp
    game/stages/IncomeCollection.cpp
    game/stages/ResearchAccumulation.cpp
    game/stages/BaseProduction.cpp
    game/stages/PostActionsProduction.cpp
    game/stages/Population.cpp
    game/stages/Upkeep.cpp
    game/stages/PlayerActions.cpp
    game/stages/Mood.cpp
    game/stages/WorldEvents.cpp
    game/stages/EcoDamage.cpp
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
# ac-ui: every view, panel and input controller. Depends only on the abstract
# Graphics/Input interfaces, never on a rendering backend, so the test suite can link
# it and drive views against a recording Graphics.
# ---------------------------------------------------------------------------
add_library(ac-ui STATIC
    ui/SpriteLibrary.cpp
    ui/TileRenderer.cpp
    ui/TileShapeGeometry.cpp
    ui/MapRenderer.cpp
    ui/CoastOverlay.cpp
    ui/TileAutotile.cpp
    ui/WaterShading.cpp
    ui/TileRelief.cpp
    ui/UIManager.cpp
    ui/ViewFactory.cpp
    ui/InteractionPresenter.cpp
    ui/style/UiStyle.cpp
    ui/style/DrawPanelChrome.cpp
    ui/world/WorldView.cpp
    ui/ListSelectorPopup.cpp
    ui/NoticePopup.cpp
    ui/world/CameraInputController.cpp
    ui/world/UnitOrderInputController.cpp
    ui/HotkeyConfig.cpp
    ui/world/CombatPresentation.cpp
    ui/world/CombatView.cpp
    ui/world/MapViewport.cpp
    ui/world/MapAppearance.cpp
    ui/world/WorldDisplay.cpp
    ui/world/FactionBaseArt.cpp
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
    ui/council/CouncilVoteView.cpp
    ui/council/CouncilFactionVotesPanel.cpp
    ui/council/CouncilProposalInfoPanel.cpp
    ui/council/CouncilVoteButton.cpp
    ui/council/CouncilVoteWeightCache.cpp
    ui/satellite/SatelliteView.cpp
    ui/satellite/SatelliteLabeledButton.cpp
    ui/satellite/SatelliteSummaryPanel.cpp
    ui/satellite/SatelliteButtonListPanel.cpp
    ui/satellite/OrbitalAttackerPopup.cpp
    ui/PlaceholderPanel.cpp
    ui/base/BaseView.cpp
    ui/base/BaseDisplaySnapshot.cpp
    ui/base/BaseNameDisplay.cpp
    ui/base/BaseWorkableAreaDisplay.cpp
    ui/base/GrowthDisplay.cpp
    ui/base/CommerceDisplay.cpp
    ui/base/ProductionDisplay.cpp
    ui/base/BuildQueueDisplay.cpp
    ui/base/HurryProductionPopup.cpp
    ui/base/PopulationDisplay.cpp
    ui/base/SupportDisplay.cpp
    ui/base/BuildingsDisplay.cpp
    ui/research/ResearchView.cpp
    ui/research/CurrentResearchPanel.cpp
    ui/settings/SettingsView.cpp
    ui/settings/SettingsPanel.cpp
    ui/social-engineering/SocialEngineeringView.cpp
    ui/social-engineering/SocialEngineeringDisplay.cpp
    ui/social-engineering/SocialEngineeringBottomPanel.cpp
    ui/unit-designer/SlotColumnPanel.cpp
    ui/unit-designer/DesignStatsDisplay.cpp
    ui/unit-designer/DesignListPanel.cpp
    ui/unit-designer/UnitStatusPanel.cpp
    ui/unit-designer/UnitDesignerView.cpp
)

target_link_libraries(ac-ui PUBLIC ac-core)

# ---------------------------------------------------------------------------
# alpha-centauri: the game executable — engine loop and the selected
# rendering/input backend on top of ac-core and ac-ui.
# ---------------------------------------------------------------------------
add_executable(alpha-centauri
    main.cpp
    game/Engine.cpp
)

target_link_libraries(alpha-centauri PRIVATE ac-core ac-ui ac-turn-stages)

option(USE_SFML "Enable SFML rendering" ON)
if(USE_SFML)
    find_package(SFML 3.0.1 COMPONENTS Graphics Window System REQUIRED)
    target_compile_definitions(alpha-centauri PRIVATE USE_SFML)
    # Only the windowing backend differs per configuration; Input is one implementation over the
    # shared PlatformEventQueue and lives in ac-core.
    target_sources(alpha-centauri PRIVATE
        graphics/SFMLGraphics.cpp
        input/SfmlKeyMapping.cpp)
    target_link_libraries(alpha-centauri PRIVATE SFML::Graphics SFML::Window SFML::System)
    if(UNIX AND NOT APPLE)
        find_package(X11 REQUIRED)
        target_link_libraries(alpha-centauri PRIVATE X11::X11)
    endif()
else()
    target_sources(alpha-centauri PRIVATE graphics/NullGraphics.cpp)
endif()
