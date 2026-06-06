#pragma once

#include <memory>
#include <vector>

namespace ac
{

class Faction;

class Diplomacy
{
public:
    Diplomacy(std::shared_ptr<std::vector<std::shared_ptr<Faction>>> factions);
    ~Diplomacy();

    void Update();
    void CreateTreaty();

private:
    void ApplyTreatyEffects_();
    void DecayRelationships_();
    void CheckTreatyExpirations_();

    std::shared_ptr<std::vector<std::shared_ptr<Faction>>> m_factions;
};

} // namespace ac
