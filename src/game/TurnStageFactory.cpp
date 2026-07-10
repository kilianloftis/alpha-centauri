#include "game/TurnStageFactory.h"
#include "game/TurnStageConfigParser.h"
#include "game/stages/CustomTurnStage.h"
#include <iostream>
#include <stdexcept>
#include <unordered_map>

namespace ac
{

namespace
{

std::unordered_map<std::string, TurnStageFactory::Creator_t>& CreatorRegistry()
{
    static std::unordered_map<std::string, TurnStageFactory::Creator_t> registry;
    return registry;
}

// unique_ptr equivalent of std::dynamic_pointer_cast: on success, transfers ownership
// into the returned pointer; on failure, leaves pBase untouched and returns nullptr.
template <typename Derived>
std::unique_ptr<Derived> DynamicUniquePtrCast(std::unique_ptr<TurnStageBase>& pBase)
{
    if (auto* pDerived = dynamic_cast<Derived*>(pBase.get()))
    {
        pBase.release();
        return std::unique_ptr<Derived>(pDerived);
    }
    return nullptr;
}

} // namespace

TurnStageFactory::TurnStageFactory()
{
}

void TurnStageFactory::RegisterCreator(const std::string& id, Creator_t creator)
{
    CreatorRegistry()[id] = std::move(creator);
}

void TurnStageFactory::LoadConfig(const std::string& configPath)
{
    TurnStageConfigParser parser;
    m_stageConfigs = parser.ParseConfig(configPath);
    if (m_stageConfigs.empty())
    {
        throw std::runtime_error("Turn stage config '" + configPath + "' produced no stages");
    }
}

TurnStageRegistries_t TurnStageFactory::CreateStages()
{
    TurnStageRegistries_t registries;
    for (const auto& config : m_stageConfigs)
    {
        std::unique_ptr<TurnStageBase> pStage = CreateStageInstance(config);
        std::cout << "Registered stage: " << config.id << "\n";

        if (auto pGlobal = DynamicUniquePtrCast<GlobalTurnStage>(pStage))
        {
            registries.global[config.id] = std::move(pGlobal);
        }
        else if (auto pPerFaction = DynamicUniquePtrCast<PerFactionTurnStage>(pStage))
        {
            registries.perFaction[config.id] = std::move(pPerFaction);
        }
        else
        {
            throw std::runtime_error(
                "Turn stage '" + config.id + "' is neither a GlobalTurnStage nor a PerFactionTurnStage");
        }
    }
    return registries;
}

std::unique_ptr<TurnStageBase> TurnStageFactory::CreateStageInstance(const TurnStageConfig_t& config)
{
    const auto& registry = CreatorRegistry();
    auto it = registry.find(config.id);
    if (it != registry.end())
    {
        return it->second(config.hookContext);
    }

    if (config.repeat_for_each_faction)
    {
        return std::make_unique<CustomPerFactionTurnStage>(config.hookContext, config.name);
    }
    return std::make_unique<CustomGlobalTurnStage>(config.hookContext, config.name);
}

} // namespace ac
