#pragma once

namespace ac
{

class Faction;
class GameSettings;

// Permanently mark the entire map explored for this faction (e.g. launching a satellite).
void ApplyRemoveShroud(Faction& rFaction);

// Union rSource's explored tiles into rDest and record tile memory for tiles rDest newly sees.
// True when rDest learned at least one tile.
bool CopyExploredMap(Faction& rDest, const Faction& rSource);

// Stickily remove fog of war for this faction (e.g. completing a secret project).
// Survives visibility rebuilds until cleared; also sets the visible-map bypass immediately.

// Re-applies RemoveShroud and RemoveFog from rule flags and settings, after
// RebuildFromSources.
void ApplyVisibilityRules(Faction& rFaction, const GameSettings& rSettings);

} // namespace ac
