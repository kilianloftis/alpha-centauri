#pragma once

#include "game/stages/TurnStages.h"

#include <random>

namespace ac
{

class BaseManager;
class Faction;
class GameState;
class Tile;

// After WorldEvents: each base rolls its eco-damage score; a hit records and announces a bloom
// and applies on_pop_effects in the base radius.
class EcoDamage : public PerFactionTurnStage
{
public:
    explicit EcoDamage(HookContext hookContext);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;

private:
    static void MaybePopFungus_(GameState& rGameState, BaseManager& rBase, std::mt19937& rRng);
    // Uniform among the base's workable tiles that are not bases and have no fungus. Null
    // when none qualifies.
    static Tile* PickPopTile_(GameState& rGameState, const BaseManager& rBase,
                              std::mt19937& rRng);
    static void Pop_(GameState& rGameState, BaseManager& rBase, Tile& rPopTile);
};

} // namespace ac
