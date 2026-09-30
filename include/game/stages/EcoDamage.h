#pragma once

#include "game/TurnStages.h"

#include <random>

namespace ac
{

class BaseManager;
class Faction;
class GameState;
class Tile;

// After WorldEvents: each base rolls its eco-damage score as a fungal-pop percentage. A hit
// records the bloom, announces it, and applies eco_damage.json's on_pop_effects on a tile in
// the base radius.
class EcoDamage : public PerFactionTurnStage
{
public:
    explicit EcoDamage(HookContext hookContext);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;

private:
    // Uniform among the base's workable tiles that are not bases and have no fungus. Null
    // when none qualifies.
    static Tile* PickPopTile_(GameState& rGameState, const BaseManager& rBase,
                              std::mt19937& rRng);
    static void Pop_(GameState& rGameState, BaseManager& rBase, Tile& rPopTile);
};

} // namespace ac
