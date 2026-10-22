#pragma once

#include "game/base/BaseTypes.h"

#include <map>

namespace ac
{

// Each faction's mind-control total (SMAC mind_control_total). GameState owns one.
// TODO: keep per-act records once the cost calculator and diplo_mind_control need them.
class MindControlLedger
{
public:
    MindControlLedger() = default;

    // weight must be positive.
    void Record(FactionId_t actor, int weight);
    // Sum of every weight recorded for actor; 0 when it has none.
    int Total(FactionId_t actor) const;

private:
    std::map<FactionId_t, int> m_totals;
};

} // namespace ac
