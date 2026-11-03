#pragma once

#include "game/production/ProductionConfigParser.h"
#include "game/GameDataPaths.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/InteractionGridsConfig.h"
#include "game/effects/TileYieldRulesConfig.h"
#include "game/map/MapRulesConfig.h"

#include <memory>
#include <vector>

namespace ac
{

class BuildingRegistry;
struct MapOverlayChannelsConfig_t;
struct BaseSpriteSizesConfig_t;
class StockpileRegistry;
class UnitComponentRegistry;
class UnitSlotRegistry;
class NativeUnitRegistry;
class PopTypeRegistry;
class TechRegistry;
class SocialPolicyRegistry;
class SocialRatingRegistry;
class FactionRegistry;
class DroneCalculator;
struct PopCompositionConfig_t;
class PopCompositionCalculator;
struct GrowthConfig_t;
class LuaRuntime;
class HurryProductionCalculator;
class ScrapRefundCalculator;
struct TechCostConfig_t;
class TechCostCalculator;
class PopTypeAvailabilityCalculator;
class ImprovementRegistry;
class TerrainOperationRegistry;
class WorldGenPresetRegistry;
struct WorldGenDecorationConfig_t;
struct LandmarkConfig_t;
struct MoraleConfig_t;
class MoraleCalculator;
class CommerceCalculator;
class ProbeCalculator;
class DiplomacyCalculator;
class BaseConquestCalculator;
struct ProbeActionsConfig_t;
struct BaseConquestConfig_t;
struct CombatConfig_t;
class CouncilProposalRegistry;
struct CouncilRulesConfig_t;
struct DifficultyConfig_t;
struct AtrocitiesConfig_t;
struct EcoDamageConfig_t;
class EcoDamageCalculator;
struct NativeLifeLevelConfig_t;
struct WorldEventsConfig_t;
struct CommerceConfig_t;
struct DiplomacyConfig_t;

// Definition data loaded once at startup, and the calculators built from it. Holds no save
// state; rules read that through CurrentGameState().
struct GameDataContext
{
    GameDataContext();
    ~GameDataContext();

    // Move-only; out of line because the members are incomplete here.
    GameDataContext(GameDataContext&&) noexcept;
    GameDataContext& operator=(GameDataContext&&) noexcept;
    GameDataContext(const GameDataContext&) = delete;
    GameDataContext& operator=(const GameDataContext&) = delete;

    // --- Registries and config structs (definition data) ---
    std::unique_ptr<BuildingRegistry> buildingRegistry;
    std::unique_ptr<MapOverlayChannelsConfig_t> mapOverlayChannels;
    std::unique_ptr<BaseSpriteSizesConfig_t> baseSpriteSizes;
    std::unique_ptr<StockpileRegistry> stockpileRegistry;
    std::unique_ptr<UnitComponentRegistry> unitComponentRegistry;
    std::unique_ptr<UnitSlotRegistry> unitSlotRegistry;
    std::unique_ptr<NativeUnitRegistry> nativeUnitRegistry;
    std::unique_ptr<TechRegistry> techRegistry;
    std::unique_ptr<SocialPolicyRegistry> socialPolicyRegistry;
    std::unique_ptr<SocialRatingRegistry> socialRatingRegistry;
    std::unique_ptr<FactionRegistry> factionRegistry;
    std::unique_ptr<PopTypeRegistry> popTypeRegistry;
    std::unique_ptr<PopCompositionConfig_t> popCompositionConfig;
    std::unique_ptr<GrowthConfig_t> growthConfig;
    std::unique_ptr<ProductionConfig_t> productionConfig;
    std::unique_ptr<CommerceConfig_t> commerceConfig;
    std::unique_ptr<DiplomacyConfig_t> diplomacyConfig;
    std::unique_ptr<TechCostConfig_t> techCostConfig;
    std::unique_ptr<ImprovementRegistry> improvementRegistry;
    std::unique_ptr<TerrainOperationRegistry> terrainOperationRegistry;
    std::unique_ptr<WorldGenPresetRegistry> worldGenPresetRegistry;
    std::unique_ptr<WorldGenDecorationConfig_t> worldGenDecorationConfig;
    std::vector<LandmarkConfig_t> worldGenLandmarks;
    // Resource-cap and other universal tile-yield effects, and the world yield scalars.
    TileYieldRulesConfig_t tileYieldRules;
    // map_rules.json as parsed; play reads the world's copy (Tile::MapRules()).
    MapRulesConfig_t mapRules;
    // FactionUnits police baselines (away-from-home weight + effectiveness for combat units).
    std::vector<EffectConfig_t> policeRules;
    // Standing WorldGlobal rules (air collateral, bombard floor, tech stagnation).
    std::vector<EffectConfig_t> worldRules;
    // Domain × terrain / attack_unit / ZOC matrices (+ InteractionOverride holes).
    InteractionGridsConfig_t interactionGrids;
    std::unique_ptr<MoraleConfig_t> moraleConfig;
    std::unique_ptr<ProbeActionsConfig_t> probeActionsConfig;
    std::unique_ptr<BaseConquestConfig_t> baseConquestConfig;
    std::unique_ptr<CombatConfig_t> combatConfig;
    std::unique_ptr<CouncilProposalRegistry> councilProposalRegistry;
    std::unique_ptr<CouncilRulesConfig_t> councilRules;
    std::unique_ptr<DifficultyConfig_t> difficultyConfig;
    std::unique_ptr<AtrocitiesConfig_t> atrocitiesConfig;
    std::unique_ptr<NativeLifeLevelConfig_t> nativeLifeLevelConfig;
    std::unique_ptr<WorldEventsConfig_t> worldEventsConfig;
    // eco_damage.json plus the formula eco_damage.lua returns.
    std::unique_ptr<EcoDamageConfig_t> ecoDamageConfig;

    // --- Calculators / services (built from the data above) ---
    std::unique_ptr<LuaRuntime> luaRuntime;
    std::unique_ptr<DroneCalculator> droneCalculator;
    std::unique_ptr<PopCompositionCalculator> popCompositionCalculator;
    std::unique_ptr<TechCostCalculator> techCostCalculator;
    std::unique_ptr<EcoDamageCalculator> ecoDamageCalculator;
    std::unique_ptr<HurryProductionCalculator> hurryProductionCalculator;
    std::unique_ptr<ScrapRefundCalculator> scrapRefundCalculator;
    std::unique_ptr<PopTypeAvailabilityCalculator> popTypeAvailabilityCalculator;
    // Reads no save state, so it lives here; every faction, unit and combat path borrows it.
    std::unique_ptr<MoraleCalculator> moraleCalculator;
    std::unique_ptr<CommerceCalculator> commerceCalculator;
    std::unique_ptr<ProbeCalculator> probeCalculator;
    std::unique_ptr<DiplomacyCalculator> diplomacyCalculator;
    std::unique_ptr<BaseConquestCalculator> baseConquestCalculator;

    // For config files LoadGameData does not parse, so they resolve against the same root.
    GameDataPaths paths;
};

// Runs every parser in dependency order and validates cross-config ids. Returns a complete
// context or throws, so consumers may dereference any member.
GameDataContext LoadGameData(const GameDataPaths& rPaths);

// Throws naming the first null member. Test fixtures that assemble a context run it too.
void ThrowIfIncomplete(const GameDataContext& rData);

} // namespace ac
