#pragma once

#include "game/map/WorldMap.h"
#include <random>

namespace ac
{

struct WorldGenConfig
{
    int width = 10;
    int height = 10;
    int minElevation = -4000;
    int maxElevation = 4000;
    float riverChance = 0.05f;  // 5% chance per tile
    unsigned int seed = 0;     // 0 = random seed
};

class WorldGenerator
{
public:
    WorldGenerator();
    ~WorldGenerator();

    // Generate a new world map with the given configuration
    std::unique_ptr<WorldMap> Generate(const WorldGenConfig& config);

private:
    std::mt19937 m_rng;

    void GenerateElevation_(WorldMap& world, const WorldGenConfig& config);
    void GenerateMoisture_(WorldMap& world);
    void GenerateRockiness_(WorldMap& world);
    void GenerateRivers_(WorldMap& world, const WorldGenConfig& config);

    int RandomInt_(int min, int max);
    float RandomFloat_();
};

} // namespace ac
