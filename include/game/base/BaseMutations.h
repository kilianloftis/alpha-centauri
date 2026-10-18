#pragma once

namespace ac
{

class BaseManager;
class Unit;

// Credit ResolveStartingMinerals into rBase's production stockpile. Founding-only — call
// once from TryFoundBase after CreateBase, while the founding unit still exists when unit
// bonuses should apply. Not for transfers, snapshot restore, or any later path (those keep
// or restore the stockpile directly). Retool is free until BankProduction stamps a turn
// original (null turn original ⇒ no penalty).
void ApplyStartingMinerals(BaseManager& rBase, const Unit* pFoundingUnit = nullptr);

} // namespace ac
