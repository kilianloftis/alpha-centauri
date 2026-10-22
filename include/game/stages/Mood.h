#pragma once

#include "game/stages/TurnStages.h"
#include "game/base/BaseTypes.h"

#include <unordered_set>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class GameState;

// After PlayerActions: commit pending riot / golden-age state that Population forecasted.
class Mood : public PerFactionTurnStage
{
public:
    explicit Mood(HookContext hookContext);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
    void OnEnterImpl() override;
    void OnExitImpl() override;

private:
    // Snapshot before the pass: Rebel on_enter may TransferBaseTo and invalidate Bases().
    static std::vector<BaseId_t> SnapshotFactionBaseIds_(const Faction& rFaction);
    // Recompose, then commit riot / golden age for one base.
    static void CommitBaseMood_(BaseManager& rBase);
    // Dispatch the active riot tier's on_enter_effects while committed rioting.
    static void DispatchRiotTierOnEnter_(GameState& rGameState, BaseManager& rBase);

    // Bases committed this pass: a rebelling base changes hands mid-pass and must not be committed
    // again by its new owner. Cleared on enter and exit.
    std::unordered_set<BaseId_t> m_committedBaseIds;
};

} // namespace ac
