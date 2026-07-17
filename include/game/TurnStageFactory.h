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
    using Creator_t = std::function<std::unique_ptr<TurnStageBase>(HookContext)>;

    TurnStageFactory();
    ~TurnStageFactory() = default;

    // Throws if the config cannot be loaded or yields no stages.
    void LoadConfig(const std::string& configPath);
    TurnStageRegistries_t CreateStages();
    const std::vector<TurnStageConfig_t>& GetStageConfigs() const { return m_stageConfigs; }

    // Registers the creator for a built-in stage id, keyed by that id. Invoked by
    // file-scope TurnStageRegistrar<T> instances (see TurnStageRegistrar.h) so that
    // adding a new built-in stage never requires editing this factory.
    static void RegisterCreator(const std::string& id, Creator_t creator);

private:
    std::unique_ptr<TurnStageBase> CreateStageInstance(const TurnStageConfig_t& config);

    std::vector<TurnStageConfig_t> m_stageConfigs;
};

} // namespace ac
