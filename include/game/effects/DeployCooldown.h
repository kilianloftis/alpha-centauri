#pragma once

namespace ac
{

// Mission year at which a source that deployed in missionYear becomes ready again.
// Mirrors the formula documented on OrbitalAttackEffect_t::cooldownTurns and
// InterceptAttemptEffect_t::cooldownTurns: ready when missionYear >= deployYear + cooldown + 1.
inline int ReadyYearAfterDeploy(int missionYear, int cooldownTurns)
{
    return missionYear + cooldownTurns + 1;
}

} // namespace ac
