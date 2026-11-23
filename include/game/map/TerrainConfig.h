#pragma once

#include "game/effects/TriggeredEffect.h"
#include "game/map/ImprovementConfigParser.h"

#include <optional>
#include <string>
#include <vector>

namespace ac
{

// Which Formers may run a project on the tile they are standing on.
enum class FormerDomainRule_t
{
    MatchesTile, // a land Former on land, a sea Former at sea
    Any,         // either Former, whatever the tile is (raising and lowering land)
};

// A Former project from terraform_projects.json. May place an occupant, run on_complete
// effects, or both.
struct TerraformProjectConfig_t
{
    std::string id;
    std::string name;
    FormerProject_t project;
    FormerDomainRule_t formerDomain = FormerDomainRule_t::MatchesTile;
    // Tile surface the project may run on. Empty means either land or sea.
    std::optional<ImprovementDomain_t> domain;
    // Occupant id placed on complete (Farm, Road, …). Empty when the project only mutates
    // terrain via on_complete_effects.
    std::optional<std::string> places;
    // Occupant ids that refuse start while present (PlantFungus blocked_by Ocean). Not
    // coexistence — world gen and AddTerrainFeature do not consult this.
    std::vector<std::string> blockedBy;
    // Each entry's `condition` is also what says whether the project may start when there
    // is no places target (or alongside places when effects would also fire).
    std::vector<TriggeredEffectConfig_t> onCompleteEffects;
};

// terrain.json features half. Projects live in terraform_projects.json.
struct TerrainFile_t
{
    std::vector<ImprovementConfig_t> features;
};

TerrainFile_t ParseTerrainFile(const std::string& configPath);

// Registry<> adapter over terraform_projects.json (a JSON array of projects).
class TerraformProjectConfigParser
{
public:
    static std::vector<TerraformProjectConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
