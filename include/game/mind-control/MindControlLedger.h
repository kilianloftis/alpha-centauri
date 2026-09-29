#pragma once

#include "game/faction/base/BaseTypes.h"

#include <map>

namespace ac
{

// World-scoped register of mind control each faction has done: SMAC's mind_control_total.
// Sibling of AtrocityLedger: GameState owns one, and the triggered RecordMindControl writes it.
// TODO: a stub. Keep per-act records (victim, kind, year) once the mind-control cost calculator
// and SMAC's per-target diplo_mind_control need them.
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
