#include "game/stages/TurnStageFactory.h"
#include "game/stages/TurnStageConfigParser.h"
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

bool TryAddGlobalBuiltIn_(TurnStageRegistries_t& rRegistries, const TurnStageConfig_t& rConfig)
{
    const auto& rCreators = GlobalCreatorRegistry();
    const auto it = rCreators.find(rConfig.id);
    if (it == rCreators.end())
    {
        return false;
    }
    if (rConfig.bRepeatForEachFaction)
    {
        throw std::runtime_error("Turn stage '" + rConfig.id
                                 + "' is a global built-in but repeatForEachFaction is true");
    }
    rRegistries.global[rConfig.id] = it->second(rConfig.hookContext);
    return true;
}

bool TryAddPerFactionBuiltIn_(TurnStageRegistries_t& rRegistries,
                              const TurnStageConfig_t& rConfig)
{
    const auto& rCreators = PerFactionCreatorRegistry();
    const auto it = rCreators.find(rConfig.id);
    if (it == rCreators.end())
    {
        return false;
    }
    if (!rConfig.bRepeatForEachFaction)
    {
        throw std::runtime_error("Turn stage '" + rConfig.id
                                 + "' is a per-faction built-in but repeatForEachFaction is false");
    }
    rRegistries.perFaction[rConfig.id] = it->second(rConfig.hookContext);
    return true;
}

// Mod-defined id: shape comes from the config flag (no C++ type to derive it from).
void AddCustomStage_(TurnStageRegistries_t& rRegistries, const TurnStageConfig_t& rConfig)
{
    if (rConfig.bRepeatForEachFaction)
    {
        rRegistries.perFaction[rConfig.id] =
            std::make_unique<CustomPerFactionTurnStage>(rConfig.hookContext, rConfig.name);
        return;
    }
    rRegistries.global[rConfig.id] =
        std::make_unique<CustomGlobalTurnStage>(rConfig.hookContext, rConfig.name);
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
    m_stageConfigs = TurnStageConfigParser::ParseConfig(configPath);
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
        if (!TryAddGlobalBuiltIn_(registries, config)
            && !TryAddPerFactionBuiltIn_(registries, config))
        {
            AddCustomStage_(registries, config);
        }
    }
    return registries;
}

} // namespace ac
