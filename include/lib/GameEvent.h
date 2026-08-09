#pragma once

#include <string>
#include <variant>

namespace ac
{

using FactionId_t = int;
using BaseId_t = int;
using TechId = std::string;

struct EvTurnStarted
{
    int turn;
};

struct EvTechDiscovered
{
    FactionId_t factionId;
    TechId techId;
};

struct EvBaseBuilt
{
    FactionId_t factionId;
    BaseId_t baseId;
};

// No EvFactionElim: factions are never removed from the game — a defeated faction's leader can
// be freed to re-establish it — so there is no elimination to observe. See
// docs/game-rules-decisions.md.

struct EvDroneRiot
{
    FactionId_t factionId;
    BaseId_t baseId;
};

struct EvBaseGainedPop
{
    FactionId_t factionId;
    BaseId_t baseId;
    int newSize;
};

struct EvBaseLostPop
{
    FactionId_t factionId;
    BaseId_t baseId;
    int newSize;
};

using GameEvent = std::variant<
    EvTurnStarted,
    EvTechDiscovered,
    EvBaseBuilt,
    EvDroneRiot,
    EvBaseGainedPop,
    EvBaseLostPop
>;

} // namespace ac
