#pragma once

#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "lib/Registry.h"
#include <stdexcept>

namespace ac
{

class PopTypeRegistry : public Registry<PopTypeConfig_t, PopTypeConfigParser, Pop>
{
public:
    const PopTypeConfig_t& GetDefault() const
    {
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (rConfig.bIsDefault)
            {
                return rConfig;
            }
        }
        throw std::runtime_error("No pop type marked as is_default in pop_types config");
    }

protected:
    void Validate_() override
    {
        Registry::Validate_();

        int defaultCount = 0;
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (rConfig.bIsDefault)
            {
                ++defaultCount;
            }
        }
        if (defaultCount != 1)
        {
            throw std::runtime_error(
                "pop_types config must have exactly one is_default entry, found " +
                std::to_string(defaultCount));
        }

        // A typo'd fallback surfaces only when a pop actually converts, and a typo'd obsoletes
        // entry never surfaces at all - it is silently inert in ResolveCurrentType.
        for (const PopTypeConfig_t& rConfig : this->m_configs)
        {
            if (!rConfig.fallbackPopTypeId.empty() && !this->Find(rConfig.fallbackPopTypeId))
            {
                throw std::runtime_error("Pop type '" + rConfig.id + "': fallback_pop_type '"
                                         + rConfig.fallbackPopTypeId + "' is not a known pop type");
            }
            for (const std::string& rObsoleteId : rConfig.obsoletes)
            {
                if (!this->Find(rObsoleteId))
                {
                    throw std::runtime_error("Pop type '" + rConfig.id + "': obsoletes entry '"
                                             + rObsoleteId + "' is not a known pop type");
                }
            }
        }
    }
};

} // namespace ac
