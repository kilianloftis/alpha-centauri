#pragma once

#include "game/IConstructable.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "lib/Signal.h"
#include <functional>
#include <string>

namespace ac
{

// ProductionManager is the API surface for the production component.
// A base can build one item at a time (building or unit design — both are IConstructable).
class ProductionManager
{
public:
    // rConfig supplies the retooling rule; it outlives every base (GameDataContext owns it).
    //
    // defaultItemProvider answers "what should this base build when nothing is chosen?" —
    // the first available stockpile, or nullptr when a mod ships none. It is consulted
    // whenever the queue would otherwise become empty (construction, completion, clearing),
    // which is what makes "a base is never idle while a stockpile is available" an invariant
    // of this class rather than a fix-up its owner applies after the fact. Selecting the
    // default never charges the retool penalty: the player did not choose it.
    ProductionManager(const ProductionConfig_t& rConfig,
                      std::function<const IConstructable*()> defaultItemProvider);
    ~ProductionManager();

    // Set the item to produce; nullptr resets to the default item (see the constructor), not
    // to an empty queue — a base only goes idle when no default exists. Setting the item
    // already queued is a no-op and does not re-announce a change. rBaseEffects is required
    // and scales the retool forfeit via RetoolPenaltyScale (empty → seed 1.0). Callers with
    // a live base should pass BaseManager::GetBaseEffects() so Skunkworks and similar apply.
    void SetProduction(const IConstructable* pItem, const BaseEffects_t& rBaseEffects);

    // Replace the queued constructable pointer without retooling. Used when the logical item
    // is unchanged but the backing object moved (base ownership transfer re-homing a unit
    // design onto the new owner's Military), or to drop a queue that can no longer resolve
    // (nullptr — falls back to the default item, mineral stockpile kept, turn original
    // cleared). Compares the outgoing pointer without dereferencing it, so the caller may
    // pass nullptr for an item that has already been destroyed.
    void RebindProductionItem(const IConstructable* pItem);

    // The item currently being produced, or nullptr if none.
    const IConstructable* GetCurrentProduction() const;

    // True if a production item is currently set.
    bool HasProduction() const;

    // Effective mineral cost of the current production item after CostMultiplier effects
    // in rBaseEffects (e.g. Industry social-rating levels). Returns 0 when nothing is queued
    // or the item never completes (stockpile).
    // bPrototype applies production.json prototype_surcharge_percent, scaled by
    // PrototypeSurchargeScale from rBaseEffects (Skunkworks zeros the extra).
    int GetMineralCost(const BaseEffects_t& rBaseEffects, bool bPrototype) const;

    // Mineral stockpile owned by this manager.
    int GetMineralStockpile() const;
    void SetMineralStockpile(int amount);

    // Add minerals to the stockpile without completing: ConvertMinerals banks this turn's
    // leftover mineral bank here for a real build item. ApplyProduction stamps with 0.
    // BaseManager decides whether completion is allowed (abandon confirmation / riot) and
    // calls CompleteProduction / ProductionCompletion::TryCompleteReady.
    //
    // Also stamps m_pTurnOriginalItem from the item then queued (or clears it when empty).
    // Until that stamp exists, SetProduction does not retool — there is no "original" to
    // switch away from (fresh bases, including a founding mineral bank, stay free until
    // the first BankProduction that banks with something queued).
    void BankProduction(int minerals);

    // True when something is queued, it can complete, and the stockpile meets its effective cost.
    bool IsReadyToComplete(const BaseEffects_t& rBaseEffects, bool bPrototype) const;

    // Complete the current production immediately and return its id. Spends the item's
    // effective cost (same rBaseEffects / bPrototype as IsReadyToComplete); leftover
    // minerals stay on the stockpile — and therefore on whatever is queued next — up to
    // retoolPenaltyThreshold. The rest is lost, so choosing the next item is a free retool.
    // Completing also clears the turn original: the default fallback is not a player choice.
    std::string CompleteProduction(const BaseEffects_t& rBaseEffects,
                                   bool bPrototype = false);

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
    std::function<const IConstructable*()> m_defaultItemProvider;
    const IConstructable* m_pCurrentItem = nullptr;
    int m_mineralStockpile = 0;

    // Queue the default item (or leave the queue empty when there is none), announcing the
    // change if it moved. Never charges retool: the player did not pick this.
    void ResetProduction_();
    void ApplyRetoolPenalty_(const IConstructable* pNewItem,
                             const BaseEffects_t& rBaseEffects);
};

} // namespace ac
