#pragma once

namespace ac
{

class BaseManager;
class Unit;

// Credits starting minerals into rBase's stockpile. Founding only, after CreateBase while the
// founding unit still exists.
void ApplyStartingMinerals(BaseManager& rBase, const Unit* pFoundingUnit = nullptr);

} // namespace ac
