#include "game/faction/Diplomacy.h"

namespace ac
{

Diplomacy::Diplomacy(std::shared_ptr<std::vector<std::shared_ptr<Faction>>> factions)
{
    m_factions = factions;
}

void Diplomacy::Update()
{
    DecayRelationships_();
    CheckTreatyExpirations_();
}

void Diplomacy::CreateTreaty()
{
    // TODO: Implement treaty creation logic
}

void Diplomacy::ApplyTreatyEffects_()
{
    // TODO: Implement treaty effect application logic
}

void Diplomacy::DecayRelationships_()
{
    // TODO: Implement relationship decay logic
}

void Diplomacy::CheckTreatyExpirations_()
{
    // TODO: Implement treaty expiration logic
}

Diplomacy::~Diplomacy()
{
}

} // namespace ac
