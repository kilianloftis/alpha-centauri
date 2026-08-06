#pragma once

#include "game/TurnStages.h"
#include "game/TurnStageConfigParser.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ac
{

struct TurnStageRegistries_t
{
    GlobalTurnStageRegistry_t global;
    PerFactionTurnStageRegistry_t perFaction;
};

class TurnStageFactory
{
public:
    using GlobalCreator_t = std::function<std::unique_ptr<GlobalTurnStage>(HookContext)>;
    using PerFactionCreator_t = std::function<std::unique_ptr<PerFactionTurnStage>(HookContext)>;

    TurnStageFactory() = default;

    // Throws if the config cannot be loaded or yields no stages.
    void LoadConfig(const std::string& configPath);
    TurnStageRegistries_t CreateStages();
    const std::vector<TurnStageConfig_t>& GetStageConfigs() const { return m_stageConfigs; }

    // Typed registration so CreateStages buckets without RTTI. Invoked by file-scope
    // TurnStageRegistrar<T> instances.
    static void RegisterGlobalCreator(const std::string& id, GlobalCreator_t creator);
    static void RegisterPerFactionCreator(const std::string& id, PerFactionCreator_t creator);

private:
    std::vector<TurnStageConfig_t> m_stageConfigs;
};

} // namespace ac
