#include "game/PlayerInteraction.h"
#include "ui/ProbeNotices.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

constexpr FactionId_t k_ActorId = 1;
constexpr FactionId_t k_TargetId = 2;

ProbeNoticeInteraction_t BaseNotice_(ProbeActionId_t actionId)
{
    ProbeNoticeInteraction_t notice;
    notice.actorId = k_ActorId;
    notice.targetFactionId = k_TargetId;
    notice.actor.title = "Sister";
    notice.actor.leader = "Miriam";
    notice.actor.name = "The Lord's Believers";
    notice.actor.noun = "Believers";
    notice.actor.adjective = "Believer";
    notice.target.title = "Commissioner";
    notice.target.leader = "Pravin Lal";
    notice.target.name = "Peacekeeping Forces";
    notice.target.noun = "Peacekeepers";
    notice.target.adjective = "Peacekeeper";
    notice.subjectName = "Garland Crater";
    notice.actionId = actionId;
    notice.result.outcome = ProbeActionOutcome_t::Succeeded;
    return notice;
}

void RequireCopy_(const ProbeNoticeInteraction_t& rNotice, FactionId_t audienceId,
                  const std::string& rBody)
{
    const std::optional<ProbeNoticeCopy_t> copy = ProbeNoticeCopy(rNotice, audienceId);
    REQUIRE(copy.has_value());
    CHECK(copy->title == "Operations Director");
    CHECK(copy->body == rBody);
}

void RequireNoCopy_(const ProbeNoticeInteraction_t& rNotice, FactionId_t audienceId)
{
    CHECK_FALSE(ProbeNoticeCopy(rNotice, audienceId).has_value());
}

} // namespace

TEST_CASE("Incite notice copy names riots or increased activity", "[ui][probe]")
{
    ProbeNoticeInteraction_t riot = BaseNotice_(ProbeActionId_t::InciteDroneRiots);
    riot.result.detail = ProbeRiot_t{};
    RequireCopy_(riot, k_ActorId, "Drone riots incited at Garland Crater!");
    RequireCopy_(riot, k_TargetId, "Drone riots incited at Garland Crater!");

    ProbeNoticeInteraction_t activity = BaseNotice_(ProbeActionId_t::InciteDroneRiots);
    activity.result.detail = ProbeIncreasedActivity_t{};
    RequireCopy_(activity, k_ActorId, "Drone activity increased at Garland Crater!");
}

TEST_CASE("Infiltrate copy is target-only DETECTINFILTRATE", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::Infiltrate);
    notice.result.detail = ProbeInfiltrated_t{};
    RequireNoCopy_(notice, k_ActorId);
    RequireCopy_(notice, k_TargetId,
                 "Sister Miriam of the Believers has infiltrated our data networks!");
}

TEST_CASE("Hunter-Seeker copy names the Algorithm for both sides", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::Infiltrate);
    notice.result.outcome = ProbeActionOutcome_t::MissionFailed;
    notice.result.detail = ProbeHunterSeeker_t{};
    RequireCopy_(notice, k_ActorId,
                 "The Hunter-Seeker Algorithm of the Peacekeeper has destroyed our probe team!");
    RequireCopy_(notice, k_TargetId,
                 "A Believer probe team has been destroyed by our Hunter-Seeker Algorithm!");
}

TEST_CASE("Steal-tech copy uses DECIPHERED0 or DECIPHERED1", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::StealTech);
    notice.techName = "Doctrine: Mobility";
    notice.result.detail = ProbeStolenTech_t{"doctrine_mobility"};
    RequireCopy_(notice, k_ActorId,
                 "Our probe team has successfully extracted the Peacekeeper files on "
                 "Doctrine: Mobility!");
    RequireCopy_(notice, k_TargetId,
                 "The Believer probe team has downloaded our files on Doctrine: Mobility!");
}

TEST_CASE("No-tech copy is actor-only STOLENOTHING", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::StealTech);
    notice.result.detail = ProbeNoTech_t{};
    RequireCopy_(notice, k_ActorId, "Our probe team found nothing of interest.");
    RequireNoCopy_(notice, k_TargetId);
}

TEST_CASE("Stolen-map copy uses STOLEMAP", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::StealTech);
    notice.result.detail = ProbeStolenMap_t{};
    RequireCopy_(notice, k_ActorId,
                 "Our probe team has stolen a copy of the Peacekeeper map files!");
    RequireCopy_(notice, k_TargetId,
                 "The Believer probe team has stolen a copy of our map files!");
}

TEST_CASE("Drain-energy copy uses GOTENERGY or TOOKENERGY", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::DrainEnergy);
    notice.result.detail = ProbeEnergyStolen_t{40};
    RequireCopy_(notice, k_ActorId,
                 "Our probe team has successfully drained 40 energy units from the "
                 "Peacekeeper energy grid!");
    RequireCopy_(notice, k_TargetId,
                 "A Believer probe team has drained 40 energy units from our reserves network!");
}

TEST_CASE("A zero energy drain uses GOTNOENERGY or TOOKNOENERGY", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::DrainEnergy);
    notice.result.detail = ProbeEnergyStolen_t{0};
    RequireCopy_(notice, k_ActorId,
                 "Our probe team reached the Peacekeeper energy grid but was unable to drain "
                 "any energy.");
    RequireCopy_(notice, k_TargetId,
                 "A Believer probe team was detected tapping into our energy grid, but discovered "
                 "to its chagrin that we have little energy available to drain.");
}

TEST_CASE("Facility sabotage copy uses FACVIRUS0 or FACVIRUS1", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::SabotageFacility);
    notice.facilityName = "Recreation Commons";
    notice.result.detail = ProbeDestroyedFacility_t{"recreation_commons"};
    RequireCopy_(notice, k_ActorId,
                 "Virus activated! Recreation Commons disabled at Garland Crater!");
    RequireCopy_(notice, k_TargetId,
                 "A Believer virus has destroyed the Recreation Commons at Garland Crater!");
}

TEST_CASE("Production wipe copy uses PRODVIRUS0 or PRODVIRUS1", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::SabotageRandom);
    notice.result.detail = ProbeProductionWiped_t{"Impact Rover"};
    RequireCopy_(notice, k_ActorId,
                 "Virus activated! Impact Rover production has been terminated at Garland Crater.");
    RequireCopy_(notice, k_TargetId,
                 "A Believer virus has dealt a severe blow to Impact Rover production at "
                 "Garland Crater.");
}

TEST_CASE("Assassinate copy names the lost research points", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::Assassinate);
    notice.result.detail = ProbeResearchWiped_t{17};
    RequireCopy_(notice, k_ActorId,
                 "Prominent Peacekeeper researcher assassinated at Garland Crater!\n"
                 "17 research points lost!");
    RequireCopy_(notice, k_TargetId,
                 "Prominent Peacekeeper researcher assassinated at Garland Crater!\n"
                 "17 research points lost!");
}

TEST_CASE("Mind-control copy uses MINDCONTROL0 or MINDCONTROL1", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::MindControlBase);
    notice.result.detail = ProbeBaseCaptured_t{};
    RequireCopy_(notice, k_ActorId,
                 "The Mind Control operation is a success, Sister! Garland Crater is ours!");
    RequireCopy_(notice, k_TargetId,
                 "Commissioner Pravin Lal! The Believers used some kind of mind control probe "
                 "to seize control of Garland Crater!");
}

TEST_CASE("Genetic plague copy uses GENETICWARFARE0 or GENETICWARFARE1", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::GeneticPlague);
    notice.result.detail = ProbePopulationKilled_t{3};
    RequireCopy_(notice, k_ActorId,
                 "Our gene-tailored retrovirus has caused mass casualties at Garland Crater!");
    RequireCopy_(notice, k_TargetId,
                 "A gene-tailored retrovirus launched by the Believers has caused mass "
                 "casualties at Garland Crater!");
}

TEST_CASE("Unit subvert copy is target-only SUBVERTED", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::SubvertUnit);
    notice.subjectName = "Impact Rover";
    notice.result.detail = ProbeUnitSubverted_t{};
    RequireNoCopy_(notice, k_ActorId);
    RequireCopy_(notice, k_TargetId,
                 "The Believers captured our Impact Rover unit! A virus was used to penetrate "
                 "the security interlocks!");
}

TEST_CASE("A failed mission tells the actor the team was eliminated", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::Infiltrate);
    notice.result.outcome = ProbeActionOutcome_t::MissionFailed;
    RequireCopy_(notice, k_ActorId,
                 "I regret to inform you that our probe team has been compromised and eliminated.");
    RequireNoCopy_(notice, k_TargetId);
}

TEST_CASE("A captured probe tells the actor the team was lost", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::StealTech);
    notice.result.outcome = ProbeActionOutcome_t::Captured;
    RequireCopy_(notice, k_ActorId,
                 "I have received the code signal for mission success, but our probe team has "
                 "been lost.");

    ProbeNoticeInteraction_t mind = BaseNotice_(ProbeActionId_t::MindControlBase);
    mind.result.outcome = ProbeActionOutcome_t::Captured;
    RequireCopy_(mind, k_ActorId,
                 "Although the mission was successful, our probe team succumbed to the side "
                 "effects of the Mind Control Probe.");
}

TEST_CASE("NoTarget has no post-mission script", "[ui][probe]")
{
    ProbeNoticeInteraction_t notice = BaseNotice_(ProbeActionId_t::SabotageFacility);
    notice.result.detail = ProbeNoTarget_t{};
    RequireNoCopy_(notice, k_ActorId);
    RequireNoCopy_(notice, k_TargetId);
}
