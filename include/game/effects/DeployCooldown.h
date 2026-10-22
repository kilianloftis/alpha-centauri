#pragma once

namespace ac
{

// Mission year a source deployed in missionYear is ready again: deployYear + cooldown + 1.
inline int ReadyYearAfterDeploy(int missionYear, int cooldownTurns)
{
    return missionYear + cooldownTurns + 1;
}

} // namespace ac
