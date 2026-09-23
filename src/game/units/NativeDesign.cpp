#include "game/units/NativeDesign.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/InteractionResolve.h"

#include <sstream>

namespace ac
{

NativeDesign::NativeDesign(const NativeUnitConfig_t& rConfig)
    : m_config(rConfig)
{
    const int turnsOfFuel = ResolveStat(*this, StatId_t::TurnsOfFuel);
    m_bUsesFuel = turnsOfFuel > 0;
    m_maxFuel = m_bUsesFuel ? turnsOfFuel * GetMovementPoints() : 0;
    m_interactionMask = InteractionMaskOf(m_config.effects);
}

const std::string& NativeDesign::GetId() const { return m_config.id; }
const std::string& NativeDesign::GetName() const { return m_config.name; }

ConstructableKind_t NativeDesign::GetConstructableKind() const
{
    return ConstructableKind_t::Unit;
}

std::vector<ActiveEffect_t> NativeDesign::CollectEffects() const
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(m_config.effects, nullptr, m_config.id, result);
    return result;
}

bool NativeDesign::IsCombatUnit() const
{
    return ResolveAdditiveStat(*this, StatId_t::Attack) > 0
           || ResolveFlag(*this, RuleFlagId_t::ForcesPsiCombat);
}

bool NativeDesign::HasComponent(const std::string& /*rComponentId*/) const
{
    return false;
}

UnitDomain_t NativeDesign::GetDomain() const
{
    return m_config.domain;
}

bool NativeDesign::UsesFuel() const
{
    return m_bUsesFuel;
}

int NativeDesign::MaxFuel() const
{
    return m_maxFuel;
}

std::string NativeDesign::FormatCombatRating() const
{
    std::ostringstream oss;
    oss << ResolveAdditiveStat(*this, StatId_t::Attack) << "-"
        << ResolveAdditiveStat(*this, StatId_t::Defense) << "-"
        << ResolveAdditiveStat(*this, StatId_t::Movement);
    return oss.str();
}

} // namespace ac
