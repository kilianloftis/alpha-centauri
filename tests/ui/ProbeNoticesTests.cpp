#include "game/PlayerInteraction.h"
#include "ui/ProbeNotices.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

ProbeNoticeInteraction_t InciteNotice_(ProbeActionStatus_t status)
{
    ProbeNoticeInteraction_t notice;
    notice.subjectName = "Garland Crater";
    notice.actionId = ProbeActionId_t::InciteDroneRiots;
    notice.result.detail = status;
    return notice;
}

void RequireInciteCopy_(ProbeActionStatus_t status, const std::string& rBody)
{
    const std::optional<ProbeNoticeCopy_t> copy = ProbeNoticeCopy(InciteNotice_(status));
    REQUIRE(copy.has_value());
    CHECK(copy->title == "Operations Director");
    CHECK(copy->body == rBody);
}

} // namespace

TEST_CASE("Incite notice copy names riots or increased activity", "[ui][probe]")
{
    RequireInciteCopy_(ProbeActionStatus_t::Riot, "Drone riots incited at Garland Crater!");
    RequireInciteCopy_(ProbeActionStatus_t::IncreasedActivity,
                       "Drone activity increased at Garland Crater!");
}
