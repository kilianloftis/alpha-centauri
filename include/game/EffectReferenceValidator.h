#pragma once

#include <string>
#include <vector>

namespace ac
{

class BuildingRegistry;
class ImprovementRegistry;
class TechRegistry;
class UnitComponentRegistry;
class NativeUnitRegistry;
class SocialRatingRegistry;
struct EffectConfig_t;
struct GameDataContext;
struct TriggeredEffectConfig_t;

// Checks the cross-config ids in one continuous effects list, throwing with rSourceId and the
// id. Null registries skip only their checks, for partial test contexts.
void ValidateEffectReferences(const std::vector<EffectConfig_t>& rEffects,
                              const std::string& rSourceId,
                              const BuildingRegistry* pBuildings,
                              const ImprovementRegistry* pImprovements,
                              const TechRegistry* pTechs,
                              const UnitComponentRegistry* pUnitComponents = nullptr,
                              const SocialRatingRegistry* pSocialRatings = nullptr,
                              const NativeUnitRegistry* pNativeUnits = nullptr);

// The same for one triggered list: AddBuilding, GrantTech and GrantUnit targets plus conditions.
void ValidateTriggeredEffectReferences(const std::vector<TriggeredEffectConfig_t>& rEffects,
                                       const std::string& rSourceId,
                                       const BuildingRegistry* pBuildings,
                                       const TechRegistry* pTechs,
                                       const UnitComponentRegistry* pUnitComponents = nullptr,
                                       const ImprovementRegistry* pImprovements = nullptr,
                                       const NativeUnitRegistry* pNativeUnits = nullptr);

// Every effects list LoadGameData loaded, continuous and triggered, so a typo'd id fails at
// startup. Throws on any null registry.
void ValidateEffectReferences(const GameDataContext& rData);

} // namespace ac
