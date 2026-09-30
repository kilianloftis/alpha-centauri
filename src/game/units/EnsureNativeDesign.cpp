#include "game/units/EnsureNativeDesign.h"

#include "game/Faction.h"
#include "game/faction/Military.h"
#include "game/units/NativeDesign.h"
#include "game/units/NativeUnitRegistry.h"

#include <memory>
#include <stdexcept>
#include <string>

namespace ac
{

const NativeDesign* EnsureNativeDesign(Faction& rFaction, const NativeUnitRegistry& rNatives,
                                       std::string_view nativeId)
{
    if (nativeId.empty())
    {
        return nullptr;
    }

    const NativeUnitConfig_t* pConfig = rNatives.Find(std::string(nativeId));
    if (!pConfig)
    {
        throw std::runtime_error("EnsureNativeDesign: native unit '" + std::string(nativeId)
                                 + "' is not in the native unit registry");
    }

    if (const IDesign* pExisting = rFaction.GetMilitary().GetDesign(pConfig->id))
    {
        if (const auto* pNative = dynamic_cast<const NativeDesign*>(pExisting))
        {
            return pNative;
        }
        throw std::runtime_error(
            "EnsureNativeDesign: design id '" + pConfig->id
            + "' is already registered as a non-native design");
    }

    auto pDesign = std::make_unique<NativeDesign>(*pConfig);
    rFaction.GetMilitary().AddDesign(std::move(pDesign));
    return dynamic_cast<const NativeDesign*>(rFaction.GetMilitary().GetDesign(pConfig->id));
}

} // namespace ac
