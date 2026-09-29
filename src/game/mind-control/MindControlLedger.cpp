#include "game/mind-control/MindControlLedger.h"

#include <stdexcept>

namespace ac
{

void MindControlLedger::Record(FactionId_t actor, int weight)
{
    if (weight < 1)
    {
        throw std::invalid_argument("MindControlLedger: weight must be positive");
    }
    m_totals[actor] += weight;
}

int MindControlLedger::Total(FactionId_t actor) const
{
    const auto it = m_totals.find(actor);
    return it == m_totals.end() ? 0 : it->second;
}

} // namespace ac
