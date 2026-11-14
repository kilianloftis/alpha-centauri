#pragma once

#include "game/RequirementListsParser.h"
#include "game/research/TechRequirement.h"
#include "lib/config/ConfigReader.h"

namespace ac
{

inline TechRequirement_t ParseTechRequirementEntry(const ConfigReader& rEntry)
{
    rEntry.RejectUnknownKeys({"tech"});
    TechRequirement_t entry;
    entry.tech = rEntry.NonEmptyString("tech");
    return entry;
}

// Parses required_techs (any / all only). Absent → empty.
inline RequiredTechs_t ParseRequiredTechs(const ConfigReader& rReader)
{
    return ParseRequirementLists<TechRequirement_t>(
        rReader, "required_techs", ParseTechRequirementEntry, {"any", "all"});
}

} // namespace ac
