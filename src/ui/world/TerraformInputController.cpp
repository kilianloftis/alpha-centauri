#include "ui/world/TerraformInputController.h"

#include "game/effects/EffectEnums.h"
#include "game/units/Unit.h"

namespace ac
{

bool TerraformInputController::HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit)
{
    m_bTerraformRequested = false;
    m_requestedImprovementId.clear();

    if (!pSelectedUnit || !pSelectedUnit->GetFlag(RuleFlagId_t::Terraform))
    {
        return false;
    }

    const auto it = m_bindings.find(rEvent.key);
    if (it == m_bindings.end())
    {
        return false;
    }

    m_requestedImprovementId = it->second;
    m_bTerraformRequested = true;
    return true;
}

} // namespace ac
