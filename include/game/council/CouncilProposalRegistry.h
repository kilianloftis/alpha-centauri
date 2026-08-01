#pragma once

#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalConfigParser.h"
#include "lib/Registry.h"
#include <stdexcept>

namespace ac
{

class CouncilProposalRegistry
    : public Registry<CouncilProposalConfig_t, CouncilProposalConfigParser>
{
protected:
    void Validate_() override
    {
        ValidateNoDuplicates_();
        for (const CouncilProposalConfig_t& rConfig : m_configs)
        {
            for (const std::string& rRequired : rConfig.requiredProposals)
            {
                if (!Find(rRequired))
                {
                    throw std::runtime_error(
                        "Council proposal '" + rConfig.id + "' requires unknown proposal '"
                        + rRequired + "'");
                }
            }
            for (const std::string& rRepeal : rConfig.repeals)
            {
                if (!Find(rRepeal))
                {
                    throw std::runtime_error(
                        "Council proposal '" + rConfig.id + "' repeals unknown proposal '"
                        + rRepeal + "'");
                }
            }
        }
    }
};

} // namespace ac
