#include "game/faction/AIProfile.h"

namespace ac
{

AIProfile::AIProfile()
{
}

AIProfile::AIProfile(const AITendenciesConfig& rConfig)
    : m_tendencies(rConfig)
{
}

AIProfile::~AIProfile()
{
}

} // namespace ac
