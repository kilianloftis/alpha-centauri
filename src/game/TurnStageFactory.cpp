#include "game/TurnStageFactory.h"
#include "game/TurnStageConfigParser.h"
#include "game/stages/CustomTurnStage.h"
#include <stdexcept>
#include <unordered_map>

namespace ac
{

namespace
{

std::unordered_map<std::string, TurnStageFactory::GlobalCreator_t>& GlobalCreatorRegistry()
{
    static std::unordered_map<std::string, TurnStageFactory::GlobalCreator_t> registry;
    return registry;
}

std::unordered_map<std::string, TurnStageFactory::PerFactionCreator_t>& PerFactionCreatorRegistry()
{
    static std::unordered_map<std::string, TurnStageFactory::PerFactionCreator_t> registry;
    return registry;
}

} // namespace

void TurnStageFactory::RegisterGlobalCreator(const std::string& id, GlobalCreator_t creator)
{
    GlobalCreatorRegistry()[id] = std::move(creator);
}

void TurnStageFactory::RegisterPerFactionCreator(const std::string& id, PerFactionCreator_t creator)
{
    PerFactionCreatorRegistry()[id] = std::move(creator);
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
        if (registries.global.contains(config.id) || registries.perFaction.contains(config.id))
        {
            throw std::runtime_error("Duplicate turn stage id '" + config.id + "'");
        }

        const auto& globalCreators = GlobalCreatorRegistry();
        const auto& perFactionCreators = PerFactionCreatorRegistry();

        auto globalIt = globalCreators.find(config.id);
        if (globalIt != globalCreators.end())
        {
            if (config.bRepeatForEachFaction)
            {
                throw std::runtime_error(
                    "Turn stage '" + config.id
                    + "' is a global built-in but repeatForEachFaction is true");
            }
            registries.global[config.id] = globalIt->second(config.hookContext);
            continue;
        }

        auto perFactionIt = perFactionCreators.find(config.id);
        if (perFactionIt != perFactionCreators.end())
        {
            if (!config.bRepeatForEachFaction)
            {
                throw std::runtime_error(
                    "Turn stage '" + config.id
                    + "' is a per-faction built-in but repeatForEachFaction is false");
            }
            registries.perFaction[config.id] = perFactionIt->second(config.hookContext);
            continue;
        }

        // Mod-defined id: shape comes from the config flag (no C++ type to derive it from).
        if (config.bRepeatForEachFaction)
        {
            registries.perFaction[config.id] =
                std::make_unique<CustomPerFactionTurnStage>(config.hookContext, config.name);
        }
        else
        {
            registries.global[config.id] =
                std::make_unique<CustomGlobalTurnStage>(config.hookContext, config.name);
        }
    }
    return registries;
}

} // namespace ac
