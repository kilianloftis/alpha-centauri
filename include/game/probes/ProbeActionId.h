#pragma once

namespace ac
{

// Fixed C++ probe handlers. JSON action "id" strings parse to these.
enum class ProbeActionId_t
{
    Infiltrate,
    StealTech,
    DrainEnergy,
    SabotageRandom,
    SabotageFacility,
    InciteDroneRiots,
    Assassinate,
    MindControlBase,
    TotalThoughtControl,
    GeneticPlague,
    SubvertUnit,
};

} // namespace ac
