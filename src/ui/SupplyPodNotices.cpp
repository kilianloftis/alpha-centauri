#include "ui/SupplyPodNotices.h"

#include "ui/text/ScriptMarkup.h"

namespace ac
{

SupplyPodNoticeCopy_t SupplyPodNoticeCopy(const SupplyPodNoticeInteraction_t& rNotice,
                                          const TextPack& rText)
{
    const ScriptEntry_t& rEntry = rText.Entry(rNotice.noticeKey);
    const ScriptSlots_t needed = SlotsOf(rEntry.body);

    ScriptArgs args;
    for (int slot : needed.numbers)
    {
        args.SetNumber(slot, 0);
    }
    for (int slot : needed.strings)
    {
        args.SetString(slot, ScriptValue_t{});
    }

    if (rNotice.creditAmount.has_value())
    {
        args.SetNumber(0, *rNotice.creditAmount);
    }
    if (rNotice.techName.has_value())
    {
        args.SetString(0, ScriptValue_t{*rNotice.techName});
    }
    if (rNotice.unitName.has_value())
    {
        args.SetString(0, ScriptValue_t{*rNotice.unitName});
    }
    if (rNotice.resourceBonusId.has_value())
    {
        args.SetString(0, ScriptValue_t{*rNotice.resourceBonusId});
    }
    if (rNotice.itemName.has_value())
    {
        args.SetString(0, ScriptValue_t{*rNotice.itemName});
    }
    if (rNotice.baseName.has_value())
    {
        args.SetString(1, ScriptValue_t{*rNotice.baseName});
    }
    if (rNotice.leaderTitle.has_value())
    {
        args.SetString(0, ScriptValue_t{*rNotice.leaderTitle});
    }
    if (rNotice.leaderName.has_value())
    {
        args.SetString(1, ScriptValue_t{*rNotice.leaderName});
    }
    if (rNotice.factionName.has_value())
    {
        args.SetString(2, ScriptValue_t{*rNotice.factionName});
    }

    SupplyPodNoticeCopy_t copy;
    copy.title = "Supply Pod";
    copy.body = FormatScript(rEntry.body, args);
    copy.imagePath = rNotice.imagePath;
    return copy;
}

} // namespace ac
