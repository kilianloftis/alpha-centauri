#pragma once

#include <variant>

namespace ac
{

using TechId = int;
using FactionId = int;

struct EvTurnStarted
{
    int turn;
};

struct EvTechDiscovered
{
    FactionId factionId;
    TechId techId;
};

struct EvBaseBuilt
{
    FactionId factionId;
    int baseId;
};

struct EvFactionElim
{
    FactionId factionId;
};

struct EvDroneRiot
{
    FactionId factionId;
    int baseId;
};

using GameEvent = std::variant<
    EvTurnStarted,
    EvTechDiscovered,
    EvBaseBuilt,
    EvFactionElim,
    EvDroneRiot
>;

} // namespace ac
