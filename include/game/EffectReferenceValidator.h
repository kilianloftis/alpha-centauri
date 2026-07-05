#pragma once

#include <string>
#include <vector>

namespace ac
{

class BuildingRegistry;
class ImprovementRegistry;
class TechRegistry;
struct EffectConfig_t;
struct GameDataContext;

// Validates the cross-config id references inside one effects list: GrantBuilding targets,
// GrantTech targets, HasImprovement selector ids, and TargetTileHas condition feature ids
// (an improvement id or a terrain feature id from AllTerrainFeatureIds). Throws
// std::runtime_error naming rSourceId and the offending id. A null registry skips the
// checks that need it. GrantUnit targets are not validated — unit designs are runtime
// data with no config registry.
void ValidateEffectReferences(const std::vector<EffectConfig_t>& rEffects,
                              const std::string& rSourceId,
                              const BuildingRegistry* pBuildings,
                              const ImprovementRegistry* pImprovements,
                              const TechRegistry* pTechs);

// Walks every loaded config that declares effects (buildings, improvements, pop types,
// unit components, social policies, social rating tables) and validates each list via the
// overload above. Call once after all registries are loaded so a typo'd id fails at
// startup with the source named, instead of loading as a silent no-op effect.
void ValidateEffectReferences(const GameDataContext& rData);

} // namespace ac
