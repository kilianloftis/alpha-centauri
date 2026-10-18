#pragma once

namespace ac
{

class Faction;

// Founding a base while the faction has no headquarters makes that base the headquarters.
bool GrantsHeadquarters(const Faction& rFaction);

} // namespace ac
