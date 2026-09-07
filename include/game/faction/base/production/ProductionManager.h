#pragma once

#include "game/IConstructable.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/base/production/ProductionApplyResult.h"
#include "game/faction/base/production/ProductionCompletion.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "lib/Signal.h"
#include <functional>
#include <string>

namespace ac
{

class BaseManager;

// ProductionManager is the API surface for the production component.
// A base can build one item at a time (building, unit design, or stockpile — all IConstructable).
class ProductionManager
{
public:
    // rConfig supplies the retooling rule; it outlives every base (GameDataContext owns it).
    // rBase outlives this manager (BaseManager owns it) and is used for conversion credits and
    // to build the completion probe (riot / abandon / effects / prototype).
    //
    // defaultItemProvider answers "what should this base build when nothing is chosen?" —
    // the first available stockpile, or nullptr when a mod ships none. It is consulted
    // whenever the queue would otherwise become empty (construction, completion, clearing),
    // which is what makes "a base is never idle while a stockpile is available" an invariant
    // of this class rather than a fix-up its owner applies after the fact. Selecting the
    // default never charges the retool penalty: the player did not choose it.
    ProductionManager(const ProductionConfig_t& rConfig,
                      std::function<const IConstructable*()> defaultItemProvider,
                      BaseManager& rBase);
    ~ProductionManager() = default;

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
    // or the item is a stockpile.
    // bPrototype applies production.json prototype_surcharge_percent, scaled by
    // PrototypeSurchargeScale from rBaseEffects (Skunkworks zeros the extra).
    int GetMineralCost(const BaseEffects_t& rBaseEffects, bool bPrototype) const;

    // Mineral stockpile owned by this manager.
    int GetMineralStockpile() const;
    void SetMineralStockpile(int amount);

    // Add minerals to the stockpile without completing. Also stamps m_pTurnOriginalItem from
    // the item then queued (or clears it when empty). Until that stamp exists, SetProduction
    // does not retool — there is no "original" to switch away from (fresh bases, including a
    // founding mineral bank, stay free until the first BankProduction with something queued).
    void BankProduction(int minerals);

    // After leftovers are banked: convert if the queue is a stockpile, otherwise try to
    // complete. One exclusive path per call — never both.
    ProductionApplyResult_t ApplyProduction();

    // Finish the queued item if cost is already met, without banking new minerals.
    // bNewTurn clears last turn's deferral (turn tick); mid-turn hurry / sibling re-eval pass
    // false so a deferral still blocks.
    ProductionApplyResult_t TryCompleteReady(bool bNewTurn = false);

    // True when something is queued, it can complete, and the stockpile meets its effective cost.
    bool IsReadyToComplete(const BaseEffects_t& rBaseEffects, bool bPrototype) const;

    bool HasPendingConfirmation() const;
    bool IsCompletionBlocked() const;
    // True when the queued item is a unit design the faction has not yet prototyped.
    bool IsCurrentPrototype() const;
    std::string CompletePending();
    void DeferCompletion();

    // Emitted when a production item is completed, with the completed item id.
    Signal<std::string> OnProductionCompleted;

    // Emitted when the current production item changes (including on clear).
    Signal<> OnProductionChanged;

private:
    ProductionApplyResult_t ConvertStockpile_();
    ProductionCompletionProbe_t BuildCompletionProbe_() const;
    // Spend the queue after ReadyToFinish / AcceptPending and pack pause gates + name for the
    // player (BaseProduction / ProductionIdleInteraction).
    ProductionApplyResult_t FinishReady_(const ProductionCompletion::EvaluateResult_t& ready);
    ProductionApplyResult_t ApplyCompletionOutcome_(
        const ProductionCompletion::EvaluateResult_t& outcome);

    // Ungated spend: only FinishReady_ after Completion returns ReadyToFinish. Game code must
    // go through ApplyProduction / TryCompleteReady / CompletePending so abandon / riot /
    // deferral gates apply. Spends the item's effective cost from GetBaseEffects(); leftover
    // minerals stay on the stockpile — and therefore on whatever is queued next — up to
    // retoolPenaltyThreshold. Completing also clears the turn original: the default fallback
    // is not a player choice.
    std::string CompleteProduction_(bool bPrototype);

    // Charging the penalty: what this base was producing when the player got control this turn.
    // Null means no turn original yet (new base / BankProduction with nothing queued) —
    // retool does not apply until BankProduction stamps a non-null original.
    const IConstructable* m_pTurnOriginalItem = nullptr;
    const ProductionConfig_t& m_rConfig;
    std::function<const IConstructable*()> m_defaultItemProvider;
    BaseManager& m_rBase;
    ProductionCompletion m_completion;
    const IConstructable* m_pCurrentItem = nullptr;
    int m_mineralStockpile = 0;

    // Queue the default item (or leave the queue empty when there is none), announcing the
    // change if it moved. Never charges retool: the player did not pick this.
    void ResetProduction_();
    void ApplyRetoolPenalty_(const IConstructable* pNewItem,
                             const BaseEffects_t& rBaseEffects);
};

} // namespace ac
