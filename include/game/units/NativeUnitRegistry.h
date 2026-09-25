#pragma once

#include "game/units/NativeUnitConfig.h"
#include "game/units/NativeUnitConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

class NativeUnitRegistry : public Registry<NativeUnitConfig_t, NativeUnitConfigParser>
{
public:
    // Parses the object form of config/native_units.json: the lifeform range and the units
    // array. Hides Registry::Load, which would keep the units and drop the range.
    void Load(const std::string& rConfigPath);

    const NativeLifeConfig_t& FungalBloomLifeforms() const { return m_lifeforms; }
    void SetFungalBloomLifeforms(const NativeLifeConfig_t& rLifeforms) { m_lifeforms = rLifeforms; }

private:
    NativeLifeConfig_t m_lifeforms;
};

} // namespace ac
