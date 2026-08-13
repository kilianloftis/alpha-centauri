#pragma once

#include "game/IConstructable.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "lib/Signal.h"
#include <string>

namespace ac
{

// ProductionManager is the API surface for the production component.
// A base can build one item at a time (building or unit design — both are IConstructable).
class ProductionManager
{
public:
    // rConfig supplies the retooling rule; it outlives every base (GameDataContext owns it).
    explicit ProductionManager(const ProductionConfig_t& rConfig);
    ~ProductionManager();

    // Set the item to produce; nullptr clears it. Setting the item already queued is a no-op
    // and does not re-announce a change.
    //
    // **Retooling.** Switching away from the item the base started this turn on forfeits a
    // share of the minerals already spent (config: retool_penalty_*), but only once more than
    // the threshold has accumulated. Switching *back* to the turn's original item is free;
    // switching on to a third item pays again. No turn original yet (null) — free to queue and
    // switch. Clearing production keeps the stockpile; re-queuing pays only once a turn
    // original exists (BankProduction stamped one).
    //
    // BankProduction marks the item in place at that moment as the turn's original, which is
    // the last thing to touch production before PlayerActions hands control to the player.
    //
    // The item is not validated against what this base may actually build — BaseManager owns
    // that question (GetConstructable), and its availability calculator is optional, so the
    // check cannot live here.
    void SetProduction(const IConstructable* pItem);

    // Replace the queued constructable pointer without retooling. Used when the logical item
    // is unchanged but the backing object moved (base ownership transfer re-homing a unit
    // design onto the new owner's Military), or to clear a queue that can no longer resolve
    // (nullptr — mineral stockpile kept, turn original cleared).
    void RebindProductionItem(const IConstructable* pItem);

    // The item currently being produced, or nullptr if none.
    const IConstructable* GetCurrentProduction() const;

    // True if a production item is currently set.
    bool HasProduction() const;

    // Effective mineral cost of the current production item after CostMultiplier effects
    // in rBaseEffects (e.g. Industry social-rating levels). Returns 0 when nothing is queued.
    // bPrototype applies production.json prototype_surcharge_percent (50% more minerals).
    int GetMineralCost(const BaseEffects_t& rBaseEffects, bool bPrototype = false) const;

    // Mineral stockpile owned by this manager.
    int GetMineralStockpile() const;
    void SetMineralStockpile(int amount);

    // Add this turn's minerals to the stockpile without completing: BaseManager decides
    // whether completion is allowed (abandon confirmation) and calls CompleteProduction.
    //
    // Also stamps m_pTurnOriginalItem from the item then queued (or clears it when empty).
    // Until that stamp exists, SetProduction does not retool — there is no "original" to
    // switch away from (fresh bases, including a founding mineral bank, stay free until
    // the first BankProduction that banks with something queued).
    void BankProduction(int minerals);

    // True when something is queued and the stockpile meets its effective cost.
    bool IsReadyToComplete(const BaseEffects_t& rBaseEffects, bool bPrototype = false) const;

    // Complete the current production immediately and return its id.
    std::string CompleteProduction();

    // Emitted when a production item is completed, with the completed item id.
    Signal<std::string> OnProductionCompleted;

    // Emitted when the current production item changes (including on clear).
    Signal<> OnProductionChanged;

private:
    // Charging the penalty: what this base was producing when the player got control this turn.
    // Null means no turn original yet (new base / BankProduction with nothing queued) —
    // retool does not apply until BankProduction stamps a non-null original.
    const IConstructable* m_pTurnOriginalItem = nullptr;
    const ProductionConfig_t& m_rConfig;
    const IConstructable* m_pCurrentItem = nullptr;
    int m_mineralStockpile = 0;

    void ResetProduction_();
    void ApplyRetoolPenalty_(const IConstructable* pNewItem);
};

} // namespace ac
