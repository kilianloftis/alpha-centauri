#pragma once

namespace ac
{

class Faction;
class GameSettings;

// Permanently mark the entire map explored for this faction (e.g. launching a satellite).
void ApplyRemoveShroud(Faction& rFaction);

// Stickily remove fog of war for this faction (e.g. completing a secret project).
// Survives visibility rebuilds until cleared; also sets the visible-map bypass immediately.
void ApplyRemoveFog(Faction& rFaction);

// Re-apply continuous RemoveShroud / RemoveFog from RuleFlags and session settings.
// Call after FactionVisibleMap::RebuildFromSources. Settings are a Faction constructor
// dependency, so there is no "no settings" mode to degrade to.
void ApplyVisibilityRules(Faction& rFaction, const GameSettings& rSettings);

} // namespace ac
