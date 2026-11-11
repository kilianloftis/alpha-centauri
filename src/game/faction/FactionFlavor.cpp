#include "game/faction/FactionFlavor.h"

#include "lib/RandomRoll.h"

#include <sstream>

namespace ac
{

FactionFlavor::FactionFlavor(const FactionFlavorConfig& rFlavor, const FactionIdentity& rIdentity,
                             uint32_t seed)
    : m_flavor(rFlavor)
    , m_identity(rIdentity)
    , m_rng(seed)
{
}

std::string FactionFlavor::PickBaseName()
{
    std::vector<const std::string*> available;
    available.reserve(m_flavor.baseNames.size());
    for (const std::string& rName : m_flavor.baseNames)
    {
        if (m_usedBaseNames.find(rName) == m_usedBaseNames.end())
        {
            available.push_back(&rName);
        }
    }

    if (!available.empty())
    {
        const std::string& rChosen = *available[PickUniformIndex(available.size(), m_rng)];
        m_usedBaseNames.insert(rChosen);
        return rChosen;
    }

    std::ostringstream oss;
    oss << m_identity.GetAdjective() << " Base " << m_fallbackBaseCounter++;
    return oss.str();
}

} // namespace ac
