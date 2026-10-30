#pragma once

namespace ac
{

class Unit;

// Destroys rUnit when it is SingleUse, after the action that spends it. Returns true when it did.
bool ExpendIfSingleUse(Unit& rUnit);

} // namespace ac
