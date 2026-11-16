#pragma once

#include "game/units/UnitTurnLatches.h"

namespace ac
{

class Unit;

// Highest HP this tile allows (maxHp * MaxHealPercent / 100).
int MaxHealableHp(const Unit& rUnit);

// Absolute HP after one idle heal tick (unchanged when ineligible or already full).
int IdleHealNewHp(const Unit& rUnit);

// Whether this turn-event blocks idle heal under healing.json.
bool BlocksIdleHeal(UnitTurnEvent_t event);

// True when any this-turn ledger event blocks idle heal.
bool IsHealIneligible(const Unit& rUnit);

} // namespace ac
