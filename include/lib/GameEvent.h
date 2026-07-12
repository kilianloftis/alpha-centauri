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

struct EvFactionElim
{
    FactionId_t factionId;
};

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
    EvFactionElim,
    EvDroneRiot,
    EvBaseGainedPop,
    EvBaseLostPop
>;

} // namespace ac
