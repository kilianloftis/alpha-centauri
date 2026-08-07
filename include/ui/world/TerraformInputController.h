#pragma once

#include "input/Input.h"

#include <string>
#include <unordered_map>

namespace ac
{

class ImprovementRegistry;
class Unit;

// Former-only hotkeys that request a terraform project by improvement id.
// WorldView calls TryStartTerraform when WasTerraformRequested() is true.
class TerraformInputController
{
public:
    // Bindings come from config/ui/terraform_bindings.json and every improvement id in it is
    // checked against rImprovements here, so a renamed or misspelled id fails at startup
    // naming the key rather than doing nothing when the player presses it.
    TerraformInputController(const std::string& rConfigPath,
                             const ImprovementRegistry& rImprovements);

    bool HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit);

    bool WasTerraformRequested() const { return m_bTerraformRequested; }
    const std::string& GetRequestedImprovementId() const { return m_requestedImprovementId; }

private:
    bool m_bTerraformRequested = false;
    std::string m_requestedImprovementId;

    // Consulted only for units with RuleFlagId_t::Terraform, so letters stay free elsewhere.
    std::unordered_map<Key_t, std::string> m_bindings;
};

} // namespace ac
