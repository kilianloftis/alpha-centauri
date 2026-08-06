include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(effects-tests
    effects/ModifierMathTests.cpp
    effects/FilterTests.cpp
    effects/ParserTests.cpp
    effects/CollectionTests.cpp
    effects/GrantExpansionTests.cpp
    effects/TileEffectsTests.cpp
    effects/PopEffectsTests.cpp
    effects/BaseIntegrationTests.cpp
    effects/EffectsCacheTests.cpp
    effects/UniversalRoutingTests.cpp
    effects/RatingTests.cpp
    effects/ValidationTests.cpp
    faction/FactionConfigParserTests.cpp
    faction/FactionFlavorTests.cpp
    faction/EconomyManagerTests.cpp
    faction/DiplomacyLedgerTests.cpp
    faction/DiplomacyActionsTests.cpp
    faction/DiplomaticActionExecutorTests.cpp
    faction/FirstContactTests.cpp
    faction/ProductionCostTests.cpp
    faction/GrowthTests.cpp
    faction/BuildingTechGateTests.cpp
    faction/PopCompositionTests.cpp
    faction/ResearchSelectorTests.cpp
    faction/SocialEngineeringManagerTests.cpp
    game/GameCategoryParserTests.cpp
    game/UnitPositionTests.cpp
    game/UnitStatsTests.cpp
    game/CombatRatingTests.cpp
    game/CombatResolverTests.cpp
    game/OrbitalCombatTests.cpp
    game/MoraleCalculatorTests.cpp
    game/ProbeRulesTests.cpp
    game/ProbeActionTests.cpp
    game/UnitNamingTests.cpp
    game/UnitOrderTests.cpp
    game/SupplyCrawlTests.cpp
    game/FoundBaseTests.cpp
    game/BaseConquestTests.cpp
    game/AttackRulesTests.cpp
    game/ImprovementConfigParserTests.cpp
    game/TerraformTests.cpp
    game/TileResourceRestrictionTests.cpp
    game/WorldGeneratorMoistureTests.cpp
    game/WorldGeneratorRockinessTests.cpp
    game/RiverGenerationTests.cpp
    game/LandmarkGenerationTests.cpp
    game/FungusGenerationTests.cpp
    game/TileBonusGenerationTests.cpp
    game/VisibilityTests.cpp
    game/UnitVisibilityTests.cpp
    game/MovementTests.cpp
    game/TransportTests.cpp
    game/MoveCostTests.cpp
    game/PathfindingTests.cpp
    game/TerritoryTests.cpp
    game/WorkerAssignmentTests.cpp
    game/HomeBaseTests.cpp
    game/TurnProcessorTests.cpp
    game/TurnStageConfigTests.cpp
    game/PlayerActionsStageTests.cpp
    game/GameSettingsTests.cpp
    game/RequiredTechValidatorTests.cpp
    game/PlanetaryCouncilTests.cpp
    ui/ModalContractTests.cpp
    lib/SignalTests.cpp
)

target_link_libraries(effects-tests PRIVATE ac-core ac-turn-stages Catch2::Catch2WithMain)

# For TestHelpers.h / GameFixtures.h.
target_include_directories(effects-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Absolute path to the JSON fixture configs the registries load in tests.
target_compile_definitions(effects-tests PRIVATE
    AC_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# CMAKE_RUNTIME_OUTPUT_DIRECTORY points at the repo root for the game binary;
# keep test binaries inside the build tree.
set_target_properties(effects-tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(effects-tests)
