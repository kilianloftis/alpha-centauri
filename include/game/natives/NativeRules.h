#pragma once

namespace ac
{

class Unit;

// Native life owned by a native-life faction, as opposed to a captured or bred native.
bool IsWildNative(const Unit& rUnit);

} // namespace ac
