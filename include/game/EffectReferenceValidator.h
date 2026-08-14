#pragma once

#include <string>
#include <vector>

namespace ac
{

class BuildingRegistry;
class ImprovementRegistry;
class TechRegistry;
class UnitComponentRegistry;
class SocialRatingRegistry;
struct EffectConfig_t;
struct GameDataContext;

// Validates the cross-config id references inside one effects list: GrantBuilding targets,
// GrantTech targets, HasImprovement selector ids, TargetTileHas / AllOf condition feature
// ids (always an improvement id), HasComponent unitFilter component ids, BuildingId
// buildingFilter building ids, and SocialRatingModifier axes. Throws std::runtime_error
// naming rSourceId and the offending id.
//
// Null registry pointers skip only the checks that need that registry — intentional for
// partial unit-test contexts. Production code uses the GameDataContext overload below,
// which requires every target registry. GrantUnit targets are not validated — unit designs
// are runtime data with no config registry.
void ValidateEffectReferences(const std::vector<EffectConfig_t>& rEffects,
                              const std::string& rSourceId,
                              const BuildingRegistry* pBuildings,
                              const ImprovementRegistry* pImprovements,
                              const TechRegistry* pTechs,
                              const UnitComponentRegistry* pUnitComponents = nullptr,
                              const SocialRatingRegistry* pSocialRatings = nullptr);

// Walks every loaded config that declares effects (buildings, techs, improvements, pop types,
// unit components, social policies, social rating tables, factions, council proposals,
// council governor effects, probe actions, tile yield rules, production) and validates each
// list via
// the overload above. Throws if any target registry or walked effect-source unique_ptr that
// LoadGameData always installs is null — never silently no-ops the whole check. Call once
// from LoadGameData after all registries are loaded so a typo'd id fails at startup with
// the source named, instead of loading as a silent no-op effect.
void ValidateEffectReferences(const GameDataContext& rData);

} // namespace ac
