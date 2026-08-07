#include "ui/world/TerraformInputController.h"

#include "game/effects/EffectEnums.h"
#include "game/map/ImprovementRegistry.h"
#include "game/units/Unit.h"
#include "lib/config/EnumNames.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

TerraformInputController::TerraformInputController(const std::string& rConfigPath,
                                                   const ImprovementRegistry& rImprovements)
{
    m_bindings = JsonConfigLoader::LoadObjectFile<std::unordered_map<Key_t, std::string>>(
        rConfigPath, "terraform binding",
        [&rConfigPath, &rImprovements](const nlohmann::json& rJson) {
            const auto fail = [&rConfigPath](const std::string& rMessage) {
                throw std::runtime_error("Terraform bindings '" + rConfigPath + "': " + rMessage);
            };

            if (!rJson.contains("bindings") || !rJson.at("bindings").is_array())
            {
                fail("missing required array 'bindings'");
            }

            std::unordered_map<Key_t, std::string> bindings;
            for (const nlohmann::json& rEntry : rJson.at("bindings"))
            {
                const std::string keyName = rEntry.at("key").get<std::string>();
                const std::string improvementId = rEntry.at("improvement").get<std::string>();

                const Key_t key = EnumFromName<Key_t>(keyName, "terraform binding key");
                if (!rImprovements.Find(improvementId))
                {
                    fail("key '" + keyName + "' is bound to '" + improvementId
                         + "', which is not a known improvement");
                }
                if (!bindings.emplace(key, improvementId).second)
                {
                    fail("key '" + keyName + "' is bound more than once");
                }
            }
            return bindings;
        });
}

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
