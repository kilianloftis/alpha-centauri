add_executable(alpha-centauri
    main.cpp
    game/Engine.cpp
    game/GameState.cpp
    game/Faction.cpp
    game/faction/FactionIdentity.cpp
    game/faction/AIProfile.cpp
    game/faction/Economy.cpp
    game/faction/Military.cpp
    game/faction/Research.cpp
    game/faction/Diplomacy.cpp
    game/faction/Base.cpp
    game/faction/population/BasePopulation.cpp
    game/faction/population/Pop.cpp
    game/faction/population/PopManager.cpp
    game/faction/Specialist.cpp
    game/TurnProcessor.cpp
    lib/EventBus.cpp
    lib/EventBridge.cpp
    game/TurnStageFactory.cpp
    game/TurnStageConfigParser.cpp
    game/HookContext.cpp
    game/stages/TurnStart.cpp
    game/stages/NewYearBegins.cpp
    game/stages/IncomeCollection.cpp
    game/stages/EnergyAllocation.cpp
    game/stages/ResearchAccumulation.cpp
    game/stages/BaseProduction.cpp
    game/stages/Population.cpp
    game/stages/Upkeep.cpp
    game/stages/PlayerActions.cpp
    game/stages/WorldEvents.cpp
    game/stages/VictoryConditionChecks.cpp
    game/stages/TurnEnd.cpp
    game/stages/Save.cpp
    game/stages/CustomTurnStage.cpp
    input/KeyMapping.cpp
    ui/PopulationDisplay.cpp
)

target_include_directories(alpha-centauri PRIVATE
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
target_link_libraries(alpha-centauri PRIVATE nlohmann_json::nlohmann_json)

# Add magic_enum library
FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG v0.9.6
)
FetchContent_MakeAvailable(magic_enum)
target_link_libraries(alpha-centauri PRIVATE magic_enum::magic_enum)

option(USE_SFML "Enable SFML rendering" ON)
if(USE_SFML)
    find_package(SFML 3.0.1 COMPONENTS Graphics Window System REQUIRED)
    target_compile_definitions(alpha-centauri PRIVATE USE_SFML)
    target_sources(alpha-centauri PRIVATE graphics/SFMLGraphics.cpp input/SFMLInput.cpp input/SFMLKeyEventQueue.cpp)
    target_link_libraries(alpha-centauri PRIVATE sfml-graphics sfml-window sfml-system)
else()
    target_sources(alpha-centauri PRIVATE graphics/NullGraphics.cpp input/NullInput.cpp)
endif()
