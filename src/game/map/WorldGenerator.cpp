#include "game/map/WorldGenerator.h"
#include <chrono>

namespace ac
{

WorldGenerator::WorldGenerator()
{
}

WorldGenerator::~WorldGenerator()
{
}

std::unique_ptr<WorldMap> WorldGenerator::Generate(const WorldGenConfig_t& config)
{
    // Initialize RNG
    unsigned int seed = config.seed == 0 
        ? static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())
        : config.seed;
    m_rng.seed(seed);

    // Create empty world
    auto pWorld = std::make_unique<WorldMap>(config.width, config.height);

    // Generate terrain characteristics
    GenerateElevation_(*pWorld, config);
    GenerateMoisture_(*pWorld);
    GenerateRockiness_(*pWorld);
    GenerateRivers_(*pWorld, config);

    return pWorld;
}

void WorldGenerator::GenerateElevation_(WorldMap& world, const WorldGenConfig_t& config)
{
    for (int y = 0; y < world.GetHeight(); ++y)
    {
        for (int x = 0; x < world.GetWidth(); ++x)
        {
            Tile* pTile = world.GetTile(x, y);
            if (pTile)
            {
                int elevation = RandomInt_(config.minElevation, config.maxElevation);
                pTile->SetElevation(elevation);
            }
        }
    }
}

void WorldGenerator::GenerateMoisture_(WorldMap& world)
{
    std::uniform_int_distribution<int> dist(0, 2);
    
    for (int y = 0; y < world.GetHeight(); ++y)
    {
        for (int x = 0; x < world.GetWidth(); ++x)
        {
            Tile* pTile = world.GetTile(x, y);
            if (pTile)
            {
                int value = dist(m_rng);
                pTile->SetBaseMoisture(static_cast<Moisture_t>(value));
                pTile->SetMoisture(static_cast<Moisture_t>(value));
            }
        }
    }
}

void WorldGenerator::GenerateRockiness_(WorldMap& world)
{
    std::uniform_int_distribution<int> dist(0, 2);
    
    for (int y = 0; y < world.GetHeight(); ++y)
    {
        for (int x = 0; x < world.GetWidth(); ++x)
        {
            Tile* pTile = world.GetTile(x, y);
            if (pTile)
            {
                int value = dist(m_rng);
                pTile->SetRockiness(static_cast<Rockiness_t>(value));
            }
        }
    }
}

void WorldGenerator::GenerateRivers_(WorldMap& world, const WorldGenConfig_t& config)
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    for (int y = 0; y < world.GetHeight(); ++y)
    {
        for (int x = 0; x < world.GetWidth(); ++x)
        {
            Tile* pTile = world.GetTile(x, y);
            if (pTile)
            {
                if (dist(m_rng) < config.riverChance)
                {
                    pTile->SetHasRiver(true);
                }
            }
        }
    }
}

int WorldGenerator::RandomInt_(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_rng);
}

float WorldGenerator::RandomFloat_()
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(m_rng);
}

} // namespace ac
