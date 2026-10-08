#pragma once

#include <string>
#include <vector>

namespace ac
{

// One map-art size column. Stage index in paths is 1-based in declaration order
// (first entry → size1, second → size2, …). Open-ended: add a row and ship the PNGs.
struct BaseSpriteSizeStage_t
{
    int minPopulation = 1;
};

// Population → size-stage table and optional building bumps (SMAC: Children's Creche +1).
// Loaded from base_sprite_sizes.json (not under buildings/, which merges *.json as buildings).
struct BaseSpriteSizesConfig_t
{
    std::vector<BaseSpriteSizeStage_t> sizeStages;
    std::vector<std::string> stageBumpBuildings;
};

} // namespace ac
