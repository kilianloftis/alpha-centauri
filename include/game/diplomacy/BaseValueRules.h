#pragma once

namespace ac
{

class BaseManager;
class Faction;

// terranx value_of_base (docs/thinker/terranx-map.md "Base value"): what rEvaluator weighs rBase
// at in a deal with rCounterparty. Throws when rBase is rEvaluator's headquarters, which the
// binary prices at -1 and every caller refuses first.
int BaseValue(const BaseManager& rBase, const Faction& rEvaluator, const Faction& rCounterparty);

} // namespace ac
