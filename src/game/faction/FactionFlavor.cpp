#include "game/faction/FactionFlavor.h"

#include <sstream>

namespace ac
{

namespace
{

void ReplaceAll_(std::string& rHaystack, const std::string& rNeedle, const std::string& rReplacement)
{
    if (rNeedle.empty())
    {
        return;
    }

    size_t pos = 0;
    while ((pos = rHaystack.find(rNeedle, pos)) != std::string::npos)
    {
        rHaystack.replace(pos, rNeedle.size(), rReplacement);
        pos += rReplacement.size();
    }
}

} // namespace

FactionFlavor::FactionFlavor(const FactionFlavorConfig& rFlavor, const FactionIdentity& rIdentity)
    : m_flavor(rFlavor)
    , m_identity(rIdentity)
    , m_rng(std::random_device{}())
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
        std::uniform_int_distribution<size_t> dist(0, available.size() - 1);
        const std::string& rChosen = *available[dist(m_rng)];
        m_usedBaseNames.insert(rChosen);
        return rChosen;
    }

    std::ostringstream oss;
    oss << m_identity.GetAdjective() << " Base " << m_fallbackBaseCounter++;
    return oss.str();
}

std::string FactionFlavor::PickPhrase(const std::string& category) const
{
    const auto it = m_flavor.phrases.find(category);
    if (it == m_flavor.phrases.end() || it->second.empty())
    {
        return {};
    }

    const std::vector<std::string>& lines = it->second;
    std::uniform_int_distribution<size_t> dist(0, lines.size() - 1);
    return Format(lines[dist(m_rng)]);
}

std::string FactionFlavor::Format(const std::string& rTemplate) const
{
    std::string result = rTemplate;
    ReplaceAll_(result, "{faction.descriptive_name}", m_identity.GetDescriptiveName());
    ReplaceAll_(result, "{faction.adjective}", m_identity.GetAdjective());
    ReplaceAll_(result, "{faction.noun}", m_identity.GetNoun());
    ReplaceAll_(result, "{leader.title}", m_identity.GetLeaderTitle());
    ReplaceAll_(result, "{faction}", m_identity.GetName());
    ReplaceAll_(result, "{leader}", m_identity.GetLeader());
    return result;
}

} // namespace ac
