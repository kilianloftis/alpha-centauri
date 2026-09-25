#pragma once

#include "game/effects/TriggeredEffect.h"
#include "game/map/ImprovementConfigParser.h"

#include <string>
#include <vector>

namespace ac
{

// Where a Former project's energy price comes from.
enum class EnergyCostSource_t
{
    Flat,            // energy_cost exactly as authored
    RaiseLowerQuote, // elevation band plus distance to the nearest owned base
};

// Which Formers may run a project on the tile they are standing on.
enum class FormerDomainRule_t
{
    MatchesTile, // a land Former on land, a sea Former at sea
    Any,         // either Former, whatever the tile is (raising and lowering land)
};

// A Former project that places no improvement: its whole result is its on_complete_effects,
// which run against the Former's tile. The set is open — a new operation is a terrain.json
// entry composed from the triggered effects that already exist, not a C++ change.
struct TerrainOperationConfig_t
{
    std::string id;
    std::string name;
    // Same former-project half a buildable improvement carries, so resolving either into a
    // TerraformProject_t is one assignment rather than a field-by-field copy. Always present:
    // an operation exists only to be run by a former.
    FormerProject_t project;
    EnergyCostSource_t energyCostSource = EnergyCostSource_t::Flat;
    FormerDomainRule_t formerDomain = FormerDomainRule_t::MatchesTile;
    // Required and non-empty: a project that declares nothing to do can never complete.
    // Each entry's `condition` is also what says whether the project may start at all.
    std::vector<TriggeredEffectConfig_t> onCompleteEffects;
};

// Both halves of config/terrain.json. They feed different registries, so parsing the file
// once and handing each half to its owner beats opening it per consumer.
struct TerrainFile_t
{
    std::vector<ImprovementConfig_t> features;
    std::vector<TerrainOperationConfig_t> operations;
};

// Whole file. Requires both halves.
TerrainFile_t ParseTerrainFile(const std::string& configPath);

// Registry<> adapter over the operations half. Prefer ParseTerrainFile when you need both.
class TerrainOperationConfigParser
{
public:
    std::vector<TerrainOperationConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
