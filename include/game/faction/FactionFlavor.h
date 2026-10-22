#pragma once

#include "game/faction/FactionConfig.h"
#include "game/faction/FactionIdentity.h"

#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

class FactionFlavor
{
public:
    // seed comes from the faction: base names are save state, so they must reproduce.
    FactionFlavor(const FactionFlavorConfig& rFlavor, const FactionIdentity& rIdentity,
                  uint32_t seed);

    std::string PickBaseName();
    std::string PickPhrase(const std::string& category) const;
    std::string Format(const std::string& rTemplate) const;

private:
    const FactionFlavorConfig& m_flavor;
    const FactionIdentity& m_identity;
    std::unordered_set<std::string> m_usedBaseNames;
    int m_fallbackBaseCounter = 1;
    mutable std::mt19937 m_rng;
};

} // namespace ac
