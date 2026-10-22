#pragma once

namespace ac
{

class Unit;

// The best same-faction scrambler that can reach the defender's tile within range and moves,
// or null. TryAttack walks it there.
Unit* FindScrambler(const Unit& rAttacker, Unit& rOriginalDefender);

} // namespace ac
