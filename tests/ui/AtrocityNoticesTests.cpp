#include "game/PlayerInteraction.h"
#include "ui/AtrocityNotices.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

AtrocityNoticeInteraction_t Notice_(AtrocitySeverityId_t severity, AtrocityExcuse_t excuse)
{
    AtrocityNoticeInteraction_t notice;
    notice.perpetratorName = "Gaia's Stepdaughters";
    notice.victimName = "Spartan Federation";
    notice.severity = severity;
    notice.excuse = excuse;
    return notice;
}

void RequireBody_(const AtrocityNoticeInteraction_t& rNotice, const std::string& rBody)
{
    const AtrocityNoticeCopy_t copy = AtrocityNoticeCopy(rNotice);
    CHECK(copy.title == "Atrocity");
    CHECK(copy.body == rBody);
}

} // namespace

TEST_CASE("A counted simple atrocity names the sanction year", "[ui][atrocity][notice]")
{
    AtrocityNoticeInteraction_t notice = Notice_(AtrocitySeverityId_t::Simple, AtrocityExcuse_t::None);
    notice.sanctionUntilYear = 2110;
    RequireBody_(notice,
                 "Gaia's Stepdaughters has committed a simple atrocity against Spartan "
                 "Federation. Commerce sanctions are in force until 2110.");
}

TEST_CASE("The notice names the tier in prose, not as the enumerator", "[ui][atrocity][notice]")
{
    const AtrocityNoticeCopy_t copy =
        AtrocityNoticeCopy(Notice_(AtrocitySeverityId_t::Major, AtrocityExcuse_t::None));
    CHECK(copy.body.find("major atrocity") != std::string::npos);
    CHECK(copy.body.find("Major") == std::string::npos);
}

TEST_CASE("An unpunished atrocity says so rather than naming a sanction", "[ui][atrocity][notice]")
{
    RequireBody_(Notice_(AtrocitySeverityId_t::Simple, AtrocityExcuse_t::CharterRepealed),
                 "Gaia's Stepdaughters has committed a simple atrocity against Spartan "
                 "Federation. The Planetary Council no longer forbids it, so no sanctions "
                 "follow.");
}

TEST_CASE("An atrocity with a Progenitor party names that exemption", "[ui][atrocity][notice]")
{
    const AtrocityNoticeCopy_t copy =
        AtrocityNoticeCopy(Notice_(AtrocitySeverityId_t::Simple, AtrocityExcuse_t::ProgenitorParty));
    CHECK(copy.body.find("take no interest") != std::string::npos);
    CHECK(copy.body.find("no longer forbids") == std::string::npos);
}

TEST_CASE("A victimless atrocity omits an against clause", "[ui][atrocity][notice]")
{
    AtrocityNoticeInteraction_t notice = Notice_(AtrocitySeverityId_t::Simple, AtrocityExcuse_t::None);
    notice.victimName.reset();
    RequireBody_(notice, "Gaia's Stepdaughters has committed a simple atrocity.");
}
