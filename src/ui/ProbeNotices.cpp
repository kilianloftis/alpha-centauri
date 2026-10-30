#include "ui/ProbeNotices.h"
#include "game/probes/ProbeActionId.h"

#include <string>
#include <type_traits>
#include <variant>

namespace ac
{

namespace
{

constexpr const char* k_DirectorTitle = "Operations Director";

bool AudienceIsActor_(const ProbeNoticeInteraction_t& rNotice, FactionId_t audienceId)
{
    return audienceId == rNotice.actorId;
}

std::optional<std::string> InciteRiotBody_(const ProbeNoticeInteraction_t& rNotice)
{
    return "Drone riots incited at " + rNotice.subjectName + "!";
}

std::optional<std::string> InciteActivityBody_(const ProbeNoticeInteraction_t& rNotice)
{
    return "Drone activity increased at " + rNotice.subjectName + "!";
}

std::optional<std::string> InfiltrateBody_(const ProbeNoticeInteraction_t& rNotice,
                                           FactionId_t audienceId)
{
    if (AudienceIsActor_(rNotice, audienceId))
    {
        return std::nullopt;
    }
    return rNotice.actor.title + " " + rNotice.actor.leader + " of the " + rNotice.actor.noun
        + " has infiltrated our data networks!";
}

std::optional<std::string> ContactDestroyedBody_(const ProbeNoticeInteraction_t& rNotice,
                                                 FactionId_t audienceId)
{
    if (AudienceIsActor_(rNotice, audienceId))
    {
        return "The " + rNotice.facilityName + " of the " + rNotice.target.adjective
            + " has destroyed our probe team!";
    }
    return "A " + rNotice.actor.adjective + " probe team has been destroyed by our "
        + rNotice.facilityName + "!";
}

std::optional<std::string> NoTechBody_(const ProbeNoticeInteraction_t& rNotice,
                                       FactionId_t audienceId)
{
    if (!AudienceIsActor_(rNotice, audienceId))
    {
        return std::nullopt;
    }
    return "Our probe team found nothing of interest.";
}

std::optional<std::string> StolenMapBody_(const ProbeNoticeInteraction_t& rNotice,
                                          FactionId_t audienceId)
{
    if (AudienceIsActor_(rNotice, audienceId))
    {
        return "Our probe team has stolen a copy of the " + rNotice.target.adjective
            + " map files!";
    }
    return "The " + rNotice.actor.adjective + " probe team has stolen a copy of our map files!";
}

std::optional<std::string> MindControlBody_(const ProbeNoticeInteraction_t& rNotice,
                                            FactionId_t audienceId)
{
    if (AudienceIsActor_(rNotice, audienceId))
    {
        return "The Mind Control operation is a success, " + rNotice.actor.title + "! "
            + rNotice.subjectName + " is ours!";
    }
    return rNotice.target.title + " " + rNotice.target.leader + "! The " + rNotice.actor.noun
        + " used some kind of mind control probe to seize control of " + rNotice.subjectName + "!";
}

std::optional<std::string> SubvertBody_(const ProbeNoticeInteraction_t& rNotice,
                                        FactionId_t audienceId)
{
    if (AudienceIsActor_(rNotice, audienceId))
    {
        return std::nullopt;
    }
    return "The " + rNotice.actor.noun + " captured our " + rNotice.subjectName
        + " unit! A virus was used to penetrate the security interlocks!";
}

std::optional<std::string> StolenTechBody_(const ProbeNoticeInteraction_t& rNotice,
                                           FactionId_t audienceId)
{
    if (AudienceIsActor_(rNotice, audienceId))
    {
        return "Our probe team has successfully extracted the " + rNotice.target.adjective
            + " files on " + rNotice.techName + "!";
    }
    return "The " + rNotice.actor.adjective + " probe team has downloaded our files on "
        + rNotice.techName + "!";
}

std::optional<std::string> EnergyBody_(const ProbeNoticeInteraction_t& rNotice,
                                       FactionId_t audienceId, int amount)
{
    if (AudienceIsActor_(rNotice, audienceId))
    {
        if (amount == 0)
        {
            return "Our probe team reached the " + rNotice.target.adjective
                + " energy grid but was unable to drain any energy.";
        }
        return "Our probe team has successfully drained " + std::to_string(amount)
            + " energy units from the " + rNotice.target.adjective + " energy grid!";
    }
    if (amount == 0)
    {
        return "A " + rNotice.actor.adjective
            + " probe team was detected tapping into our energy grid, but discovered to its "
              "chagrin that we have little energy available to drain.";
    }
    return "A " + rNotice.actor.adjective + " probe team has drained " + std::to_string(amount)
        + " energy units from our reserves network!";
}

std::optional<std::string> FacilityBody_(const ProbeNoticeInteraction_t& rNotice,
                                         FactionId_t audienceId)
{
    if (AudienceIsActor_(rNotice, audienceId))
    {
        return "Virus activated! " + rNotice.facilityName + " disabled at " + rNotice.subjectName
            + "!";
    }
    return "A " + rNotice.actor.adjective + " virus has destroyed the " + rNotice.facilityName
        + " at " + rNotice.subjectName + "!";
}

std::optional<std::string> ProductionBody_(const ProbeNoticeInteraction_t& rNotice,
                                           FactionId_t audienceId, const std::string& rItemName)
{
    if (AudienceIsActor_(rNotice, audienceId))
    {
        return "Virus activated! " + rItemName + " production has been terminated at "
            + rNotice.subjectName + ".";
    }
    return "A " + rNotice.actor.adjective + " virus has dealt a severe blow to " + rItemName
        + " production at " + rNotice.subjectName + ".";
}

std::optional<std::string> ResearchBody_(const ProbeNoticeInteraction_t& rNotice, int points)
{
    return "Prominent " + rNotice.target.adjective + " researcher assassinated at "
        + rNotice.subjectName + "!\n" + std::to_string(points) + " research points lost!";
}

std::optional<std::string> PlagueBody_(const ProbeNoticeInteraction_t& rNotice,
                                       FactionId_t audienceId)
{
    if (AudienceIsActor_(rNotice, audienceId))
    {
        return "Our gene-tailored retrovirus has caused mass casualties at " + rNotice.subjectName
            + "!";
    }
    return "A gene-tailored retrovirus launched by the " + rNotice.actor.noun
        + " has caused mass casualties at " + rNotice.subjectName + "!";
}

std::optional<std::string> BodyFromDetail_(const ProbeNoticeInteraction_t& rNotice,
                                           FactionId_t audienceId)
{
    return std::visit(
        [&](const auto& rDetail) -> std::optional<std::string>
        {
            using T = std::decay_t<decltype(rDetail)>;
            if constexpr (std::is_same_v<T, ProbeRiot_t>)
            {
                return InciteRiotBody_(rNotice);
            }
            else if constexpr (std::is_same_v<T, ProbeIncreasedActivity_t>)
            {
                return InciteActivityBody_(rNotice);
            }
            else if constexpr (std::is_same_v<T, ProbeInfiltrated_t>)
            {
                return InfiltrateBody_(rNotice, audienceId);
            }
            else if constexpr (std::is_same_v<T, ProbeContactDestroyed_t>)
            {
                return ContactDestroyedBody_(rNotice, audienceId);
            }
            else if constexpr (std::is_same_v<T, ProbeNoTech_t>)
            {
                return NoTechBody_(rNotice, audienceId);
            }
            else if constexpr (std::is_same_v<T, ProbeStolenMap_t>)
            {
                return StolenMapBody_(rNotice, audienceId);
            }
            else if constexpr (std::is_same_v<T, ProbeBaseCaptured_t>)
            {
                return MindControlBody_(rNotice, audienceId);
            }
            else if constexpr (std::is_same_v<T, ProbeUnitSubverted_t>)
            {
                return SubvertBody_(rNotice, audienceId);
            }
            else if constexpr (std::is_same_v<T, ProbeStolenTech_t>)
            {
                return StolenTechBody_(rNotice, audienceId);
            }
            else if constexpr (std::is_same_v<T, ProbeEnergyStolen_t>)
            {
                return EnergyBody_(rNotice, audienceId, rDetail.amount);
            }
            else if constexpr (std::is_same_v<T, ProbeDestroyedFacility_t>)
            {
                return FacilityBody_(rNotice, audienceId);
            }
            else if constexpr (std::is_same_v<T, ProbeProductionWiped_t>)
            {
                return ProductionBody_(rNotice, audienceId, rDetail.itemName);
            }
            else if constexpr (std::is_same_v<T, ProbeResearchWiped_t>)
            {
                return ResearchBody_(rNotice, rDetail.points);
            }
            else if constexpr (std::is_same_v<T, ProbePopulationKilled_t>)
            {
                return PlagueBody_(rNotice, audienceId);
            }
            else
            {
                // monostate, ProbeNoTarget_t
                return std::nullopt;
            }
        },
        rNotice.result.detail);
}

std::optional<std::string> BodyFromOutcome_(const ProbeNoticeInteraction_t& rNotice,
                                            FactionId_t audienceId)
{
    if (!AudienceIsActor_(rNotice, audienceId))
    {
        return std::nullopt;
    }
    switch (rNotice.result.outcome)
    {
    case ProbeActionOutcome_t::MissionFailed:
        return "I regret to inform you that our probe team has been compromised and eliminated.";
    case ProbeActionOutcome_t::Captured:
        if (rNotice.actionId == ProbeActionId_t::MindControlBase
            || rNotice.actionId == ProbeActionId_t::TotalThoughtControl)
        {
            return "Although the mission was successful, our probe team succumbed to the side "
                   "effects of the Mind Control Probe.";
        }
        return "I have received the code signal for mission success, but our probe team has been "
               "lost.";
    case ProbeActionOutcome_t::Rejected:
    case ProbeActionOutcome_t::Succeeded:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

std::optional<ProbeNoticeCopy_t> ProbeNoticeCopy(const ProbeNoticeInteraction_t& rNotice,
                                                FactionId_t audienceId)
{
    const std::optional<std::string> detailBody = BodyFromDetail_(rNotice, audienceId);
    if (detailBody.has_value())
    {
        return ProbeNoticeCopy_t{k_DirectorTitle, *detailBody};
    }
    const std::optional<std::string> outcomeBody = BodyFromOutcome_(rNotice, audienceId);
    if (!outcomeBody.has_value())
    {
        return std::nullopt;
    }
    return ProbeNoticeCopy_t{k_DirectorTitle, *outcomeBody};
}

// The script picks the framed leader's pronoun; we have no leader gender, so FRAMEFLOP takes its
// plural form.
std::optional<ProbeNoticeCopy_t> ProbeFrameNoticeCopy(const ProbeFrameNoticeInteraction_t& rNotice,
                                                     FactionId_t audienceId)
{
    if (audienceId != rNotice.actorId)
    {
        return std::nullopt;
    }
    if (rNotice.outcome == ProbeFrameOutcome_t::Flopped)
    {
        return ProbeNoticeCopy_t{
            k_DirectorTitle, rNotice.framed.title + " " + rNotice.framed.leader
                                 + " has learned of our attempt to frame them and has pronounced "
                                   "Vendetta!"};
    }
    return ProbeNoticeCopy_t{
        k_DirectorTitle, "We have successfully framed the " + rNotice.framed.noun + "! "
                             + rNotice.target.title + " " + rNotice.target.leader
                             + " has pronounced Vendetta against " + rNotice.framed.title + " "
                             + rNotice.framed.leader + "!"};
}

} // namespace ac
