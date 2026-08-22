#include "game/faction/AIProfile.h"

namespace ac
{

AIProfile::AIProfile()
{
}

AIProfile::AIProfile(const AITendenciesConfig& rConfig)
    : m_tendencies(rConfig)
{
    // TODO(difficulty): when rules.ai_auto_personality is false, skip auto personality assign.
}

AIProfile::~AIProfile()
{
}

} // namespace ac
