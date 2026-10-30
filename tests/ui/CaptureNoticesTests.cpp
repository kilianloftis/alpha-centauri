#include "game/PlayerInteraction.h"
#include "ui/CaptureNotices.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

constexpr FactionId_t k_CaptorId = 1;
constexpr FactionId_t k_VictimId = 2;
constexpr FactionId_t k_BystanderId = 3;

CaptureNoticeInteraction_t Notice_(CaptureNoticeKind_t kind)
{
    CaptureNoticeInteraction_t notice;
    notice.kind = kind;
    notice.captorId = k_CaptorId;
    notice.victimId = k_VictimId;
    notice.captorNoun = "Pirates";
    notice.captorAdjective = "Pirate";
    notice.victimAdjective = "Gaian";
    notice.unitName = "Alien Artifact";
    notice.count = 1;
    return notice;
}

std::string Body_(const CaptureNoticeInteraction_t& rNotice, FactionId_t audienceId)
{
    const std::optional<CaptureNoticeCopy_t> copy = CaptureNoticeCopy(rNotice, audienceId);
    REQUIRE(copy.has_value());
    CHECK(copy->title == "Battle Report");
    return copy->body;
}

} // namespace

TEST_CASE("Boarding reports to the captor and the victim", "[capture][notice][ui]")
{
    CaptureNoticeInteraction_t notice = Notice_(CaptureNoticeKind_t::Boarded);
    notice.unitName = "Cruiser";

    CHECK(Body_(notice, k_CaptorId)
          == "Our Marine Detachment boarded and captured the Gaian Cruiser!");
    CHECK(Body_(notice, k_VictimId) == "Pirate Marine Detachment boarded and captured our Cruiser!");
    CHECK_FALSE(CaptureNoticeCopy(notice, k_BystanderId).has_value());
}

TEST_CASE("Units taken on defeat report by count and the victim's Vendetta",
          "[capture][notice][ui]")
{
    CaptureNoticeInteraction_t notice = Notice_(CaptureNoticeKind_t::TakenOnDefeat);
    CHECK(Body_(notice, k_CaptorId) == "We have captured the Alien Artifact!");
    CHECK(Body_(notice, k_VictimId) == "The Pirates have appropriated our Alien Artifact!");

    notice.bVictimInVendetta = true;
    notice.count = 2;
    CHECK(Body_(notice, k_CaptorId) == "We have captured 2 Alien Artifact units!");
    CHECK(Body_(notice, k_VictimId) == "The enemy has captured 2 of our Alien Artifact units!");
}
