#pragma once

namespace ac
{

class BaseManager;
class Pop;

// The pop a shrinking base loses: player-chosen types go last, then the one producing least.
// Ties keep the earliest, so the choice does not depend on allocation order.
const Pop& SelectPopToRemove(const BaseManager& rBase);

} // namespace ac
