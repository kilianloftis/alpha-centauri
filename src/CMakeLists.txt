add_executable(alpha-centauri
    main.cpp
    game/Engine.cpp
    game/GameState.cpp
    game/GameDataContext.cpp
    game/Faction.cpp
    game/faction/FactionIdentity.cpp
    game/faction/AIProfile.cpp
    game/faction/Military.cpp
    game/research/Tech.cpp
    game/research/TechRegistry.cpp
    game/research/TechCostCalculator.cpp
    game/faction/ResearchManager.cpp
    game/faction/Diplomacy.cpp
    game/faction/base/BaseManager.cpp
    game/faction/base/buildings/BuildingManager.cpp
    game/faction/base/resources/ResourceManager.cpp
    game/faction/base/resources/WorkerAssignmentManager.cpp
    game/faction/base/resources/BaseEconomyManager.cpp
    game/faction/base/population/PopulationManager.cpp
    game/faction/base/population/PopContainer.cpp
    game/faction/base/population/pop-types/Pop.cpp
    game/faction/base/population/pop-types/PopTypeConfigParser.cpp
    game/faction/base/population/pop-types/PopTypeRegistry.cpp
    game/faction/base/population/pop-types/PopCompositionConfigParser.cpp
    game/faction/base/population/pop-types/GrowthConfigParser.cpp
    game/faction/base/population/calculators/PopCompositionCalculator.cpp
    game/faction/base/population/calculators/RiotCalculator.cpp
    game/faction/base/population/calculators/GrowthCalculator.cpp
    game/faction/base/population/calculators/GoldenAgeCalculator.cpp
    game/faction/Specialist.cpp
    game/TurnProcessor.cpp
    lib/EventBus.cpp
    lib/EventBridge.cpp
    lib/LuaRuntime.cpp
    game/TurnStageFactory.cpp
    game/TurnStageConfigParser.cpp
    game/HookContext.cpp
    game/buildings/Building.cpp
    game/buildings/BuildingConfigParser.cpp
    game/buildings/BuildingRegistry.cpp
    game/research/TechConfigParser.cpp
    game/map/Tile.cpp
    game/map/TileBonusConfigParser.cpp
    game/map/TileBonusRegistry.cpp
    game/map/WorldMap.cpp
    game/map/WorldGenerator.cpp
    game/stages/TurnStart.cpp
    game/stages/NewYearBegins.cpp
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
    game/stages/CustomTurnStage.cpp
    input/KeyMapping.cpp
    input/KeyEventQueue.cpp
    input/MouseEventQueue.cpp
    ui/base/BaseView.cpp
    ui/base/BaseDisplay.cpp
    ui/base/PopulationDisplay.cpp
    ui/base/BaseWorkableAreaDisplay.cpp
    ui/TileHitTester.cpp
    ui/UIManager.cpp
    ui/world/WorldDisplay.cpp
    ui/world/WorldMapElement.cpp
    ui/world/WorldView.cpp
    ui/world/InfoPanelElement.cpp
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
target_link_libraries(alpha-centauri PRIVATE lua54)

# Add sol2 (header-only)
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(sol2)
target_link_libraries(alpha-centauri PRIVATE sol2)

option(USE_SFML "Enable SFML rendering" ON)
if(USE_SFML)
    find_package(SFML 3.0.1 COMPONENTS Graphics Window System REQUIRED)
    target_compile_definitions(alpha-centauri PRIVATE USE_SFML)
    target_sources(alpha-centauri PRIVATE graphics/SFMLGraphics.cpp input/SFMLInput.cpp)
    target_link_libraries(alpha-centauri PRIVATE SFML::Graphics SFML::Window SFML::System)
else()
    target_sources(alpha-centauri PRIVATE graphics/NullGraphics.cpp input/NullInput.cpp)
endif()
