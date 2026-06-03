add_executable(alpha-centauri
    main.cpp
    game/Engine.cpp
    input/KeyMapping.cpp
)

target_include_directories(alpha-centauri PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

option(USE_SFML "Enable SFML rendering" OFF)
if(USE_SFML)
    find_package(SFML 3.0.2 COMPONENTS Graphics Window System REQUIRED)
    target_compile_definitions(alpha-centauri PRIVATE USE_SFML)
    target_sources(alpha-centauri PRIVATE graphics/SFMLGraphics.cpp input/SFMLInput.cpp input/SFMLKeyEventQueue.cpp)
    target_link_libraries(alpha-centauri PRIVATE SFML::Graphics SFML::Window SFML::System)
else()
    target_sources(alpha-centauri PRIVATE graphics/NullGraphics.cpp input/NullInput.cpp)
endif()
