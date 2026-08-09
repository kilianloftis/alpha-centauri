#pragma once

#include "game/IConstructable.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "lib/Signal.h"
#include <string>

namespace ac
{

// ProductionManager is the API surface for the production component.
// A base can build one item at a time.
// TODO: Extend to unit production once unit definitions with mineral costs exist.
class ProductionManager
{
public:
    // rConfig supplies the retooling rule and the minerals-per-row rate; it outlives every
    // base (GameDataContext owns it).
    explicit ProductionManager(const ProductionConfig_t& rConfig);
    ~ProductionManager();

    // Set the item to produce; nullptr clears it. Setting the item already queued is a no-op
    // and does not re-announce a change.
    //
    // **Retooling.** Switching away from the item the base started this turn on forfeits a
    // share of the minerals already spent (config: retool_penalty_*), but only once more than
    // the threshold has accumulated. Switching *back* to the turn's original item is free;
    // switching on to a third item pays again. Clearing production keeps the stockpile — the
    // player has not committed to anything else yet, and re-queuing pays on the way in.
    //
    // ApplyProduction marks the item in place at that moment as the turn's original, which is
    // the last thing to touch production before PlayerActions hands control to the player.
    //
    // The item is not validated against what this base may actually build — BaseManager owns
    // that question (GetConstructable), and its availability calculator is optional, so the
    // check cannot live here.
    void SetProduction(const IConstructable* pItem);

    // The item currently being produced, or nullptr if none.
    const IConstructable* GetCurrentProduction() const;

    // True if a production item is currently set.
    bool HasProduction() const;

    // Effective mineral cost of the current production item after CostMultiplier effects
    // in rBaseEffects (e.g. Industry social-rating levels). Returns 0 when nothing is queued.
    int GetMineralCost(const BaseEffects_t& rBaseEffects) const;

    // Mineral stockpile owned by this manager.
    int GetMineralStockpile() const;
    void SetMineralStockpile(int amount);

    // Apply minerals produced this turn: add to stockpile, complete if cost is met.
    // rBaseEffects is forwarded to GetMineralCost. Returns the completed item id,
    // or empty string if construction is ongoing.
    std::string ApplyProduction(int minerals, const BaseEffects_t& rBaseEffects);

    // Complete the current production immediately and return its id.
    std::string CompleteProduction();

    // Emitted when a production item is completed, with the completed item id.
    Signal<std::string> OnProductionCompleted;

    // Emitted when the current production item changes (including on clear).
    Signal<> OnProductionChanged;

private:
    // Charging the penalty: what this base was producing when the player got control this turn.
    const IConstructable* m_pTurnOriginalItem = nullptr;
    const ProductionConfig_t& m_rConfig;
    const IConstructable* m_pCurrentItem = nullptr;
    int m_mineralStockpile = 0;

    void ResetProduction_();
    void ApplyRetoolPenalty_(const IConstructable* pNewItem);
};

} // namespace ac
