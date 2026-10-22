#pragma once

#include "game/production/IConstructable.h"
#include "game/effects/ActiveEffect.h"
#include "game/production/ProductionApplyResult.h"
#include "game/production/ProductionCompletion.h"
#include "game/production/ProductionConfigParser.h"
#include "game/production/ProductionQueue.h"
#include "game/production/ProductionQueueRules.h"
#include "lib/Signal.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

class BaseManager;

// Current production and the upcoming queue: SetProduction changes the current item (and may
// retool); Replace, Insert and Delete edit the queue.
class ProductionManager
{
public:
    // defaultItemProvider names what to build when nothing is chosen (the first available
    // stockpile), so a base is never idle while one exists. Choosing it never retools.
    ProductionManager(const ProductionConfig_t& rConfig,
                      std::function<const IConstructable*()> defaultItemProvider,
                      BaseManager& rBase);
    ~ProductionManager() = default;

    // nullptr resets to the default item. A once-only building on the queue is pulled off it;
    // rBaseEffects scales the retool forfeit.
    void SetProduction(const IConstructable* pItem, const BaseEffects_t& rBaseEffects);

    void Replace(int slot, const IConstructable& rItem);
    void Insert(int slot, const IConstructable& rItem);
    void Delete(int slot);

    // Drops invalid queue items; an invalid current item is cleared and its name returned. The
    // current then takes the next queued item or the default. No retool.
    std::optional<std::string> ValidateQueue();

    // Replace current (nullptr → default) without retooling. Used on ownership transfer
    // after the item is re-homed or dropped.
    void RebindCurrent(const IConstructable* pItem);

    // Replace the upcoming list without retooling. Used on ownership transfer after each
    // slot is re-homed or dropped.
    void RebindQueue(std::vector<const IConstructable*> items);

    const std::vector<const IConstructable*>& GetQueue() const;

    // The item currently being produced, or nullptr if none.
    const IConstructable* GetCurrentProduction() const;

    // True if a production item is currently set.
    bool HasProduction() const;

    // Mineral stockpile owned by this manager.
    int GetMineralStockpile() const;
    void SetMineralStockpile(int amount);

    // Banks minerals without completing, and stamps the turn's original item; retooling applies
    // only once that stamp exists.
    void BankProduction(int minerals);

    // After leftovers are banked: convert if the current item is a stockpile, otherwise leave
    // the minerals on it. Completion is a separate step (TryCompleteReady).
    ProductionApplyResult_t ApplyProduction();

    // Finishes the current item if its cost is met. bNewTurn clears last turn's deferral.
    ProductionApplyResult_t TryCompleteReady(bool bNewTurn = false);

    // True when something is current, it is still constructable, it can complete, and the
    // stockpile meets its effective cost.
    bool IsReadyToComplete() const;

    bool HasPendingConfirmation() const;
    bool IsCompletionBlocked() const;
    // True when the current item is a unit design the faction has not yet prototyped.
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
    // Spend the current item after ReadyToFinish / AcceptPending and pack pause gates + name
    // for the player (BaseProduction / ProductionIdleInteraction).
    ProductionApplyResult_t FinishReady_(const ProductionCompletion::EvaluateResult_t& ready);
    ProductionApplyResult_t ApplyCompletionOutcome_(
        const ProductionCompletion::EvaluateResult_t& outcome);

    // Ungated: only after Completion returns ReadyToFinish. Keeps overflow minerals.
    std::string CompleteProduction_();

    // What the base was producing when the player got control this turn; null until the first
    // BankProduction, so retooling is free until then.
    const IConstructable* m_pTurnOriginalItem = nullptr;
    const IConstructable* m_pCurrentItem = nullptr;
    const ProductionConfig_t& m_rConfig;
    std::function<const IConstructable*()> m_defaultItemProvider;
    BaseManager& m_rBase;
    ProductionCompletion m_completion;
    ProductionQueue m_queue;
    int m_mineralStockpile = 0;

    const IConstructable* DefaultItem_() const;
    // Set current to the default item (or none when there is none), announcing the change if
    // it moved. The queue is left as-is. Never charges retool: the player did not pick this.
    void ResetProduction_();
    void ApplyQueuePlan_(const QueueEditPlan_t& rPlan);
    void ApplyRetoolPenalty_(const IConstructable* pNewItem,
                             const BaseEffects_t& rBaseEffects);
};

} // namespace ac
