#include "game/PlayerInteraction.h"
#include "ui/SubmergenceNotices.h"

#include <catch2/catch_test_macros.hpp>

using namespace ac;

namespace
{

constexpr FactionId_t k_OwnerId = 1;
constexpr FactionId_t k_AudienceId = 2;

SubmergenceNoticeInteraction_t Notice_()
{
    SubmergenceNoticeInteraction_t notice;
    notice.ownerId = k_OwnerId;
    notice.baseName = "Garland Crater";
    notice.ownerFactionName = "Gaia's Stepdaughters";
    return notice;
}

} // namespace

TEST_CASE("Own submerged base omits the faction name", "[submergence][notice][ui]")
{
    const SubmergenceNoticeCopy_t copy = SubmergenceNoticeCopy(Notice_(), k_OwnerId);
    CHECK(copy.title == "Base Submerged");
    CHECK(copy.body == "The base Garland Crater has been lost beneath the rising sea.");
}

TEST_CASE("Foreign submerged base names the owning faction", "[submergence][notice][ui]")
{
    const SubmergenceNoticeCopy_t copy = SubmergenceNoticeCopy(Notice_(), k_AudienceId);
    CHECK(copy.title == "Base Submerged");
    CHECK(copy.body
          == "The Gaia's Stepdaughters base Garland Crater has been lost beneath the rising sea.");
}
