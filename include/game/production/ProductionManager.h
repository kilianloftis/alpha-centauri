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

// ProductionManager is the API surface for the production component.
// Current production and the upcoming queue are separate: SetProduction changes the
// current item (and may retool); Replace / Insert / Delete edit only the queue.
class ProductionManager
{
public:
    // rConfig supplies the retooling rule; it outlives every base (GameDataContext owns it).
    // rBase outlives this manager (BaseManager owns it) and is used for conversion credits and
    // to build the completion probe (riot / abandon / effects / prototype).
    //
    // defaultItemProvider answers "what should this base build when nothing is chosen?" —
    // the first available stockpile, or nullptr when a mod ships none. It is consulted
    // whenever current production would otherwise be empty (construction, completion,
    // clearing), which is what makes "a base is never idle while a stockpile is available"
    // an invariant of this class rather than a fix-up its owner applies after the fact.
    // Selecting the default never charges the retool penalty: the player did not choose it.
    ProductionManager(const ProductionConfig_t& rConfig,
                      std::function<const IConstructable*()> defaultItemProvider,
                      BaseManager& rBase);
    ~ProductionManager() = default;

    // Set the item to produce; nullptr resets to the default item (see the constructor).
    // The upcoming queue is left as-is. Setting the item already current is a no-op. A
    // once-only building already on the queue is pulled off it. rBaseEffects scales the
    // retool forfeit via RetoolPenaltyScale.
    void SetProduction(const IConstructable* pItem, const BaseEffects_t& rBaseEffects);

    void Replace(int slot, const IConstructable& rItem);
    void Insert(int slot, const IConstructable& rItem);
    void Delete(int slot);

    // Drop queue items that ValidQueueItems rejects. An unconstructable current item is
    // cleared and its name is returned so the stage can prompt. An empty current then takes
    // the first remaining queue item, or the default stockpile. No retool.
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

    // Add minerals to the stockpile without completing. Also stamps m_pTurnOriginalItem from
    // the item then current (or clears it when empty). Until that stamp exists, SetProduction
    // does not retool — there is no "original" to switch away from (fresh bases, including a
    // founding mineral bank, stay free until the first BankProduction with something current).
    void BankProduction(int minerals);

    // After leftovers are banked: convert if the current item is a stockpile, otherwise leave
    // the minerals on it. Completion is a separate step (TryCompleteReady).
    ProductionApplyResult_t ApplyProduction();

    // Finish the current item if cost is already met, without banking new minerals.
    // bNewTurn clears last turn's deferral (turn tick); mid-turn hurry / sibling re-eval pass
    // false so a deferral still blocks.
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

    // Ungated spend: only FinishReady_ after Completion returns ReadyToFinish. Game code must
    // go through TryCompleteReady / CompletePending so abandon / riot / deferral gates apply.
    // Clears the finished item and holds overflow minerals; does not touch the queue.
    std::string CompleteProduction_();

    // Charging the penalty: what this base was producing when the player got control this turn.
    // Null means no turn original yet (new base / BankProduction with nothing current) —
    // retool does not apply until BankProduction stamps a non-null original.
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
