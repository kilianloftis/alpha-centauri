#include "ui/SubmergenceNotices.h"

namespace ac
{

SubmergenceNoticeCopy_t SubmergenceNoticeCopy(const SubmergenceNoticeInteraction_t& rNotice,
                                              FactionId_t audienceId)
{
    SubmergenceNoticeCopy_t copy;
    copy.title = "Base Submerged";
    if (rNotice.ownerId == audienceId)
    {
        copy.body = "The base " + rNotice.baseName + " has been lost beneath the rising sea.";
        return copy;
    }
    copy.body = "The " + rNotice.ownerFactionName + " base " + rNotice.baseName
                + " has been lost beneath the rising sea.";
    return copy;
}

} // namespace ac
