#pragma once

#include "game/TurnStages.h"
#include "game/TurnStageConfigParser.h"
#include <string>
#include <memory>
#include <vector>

namespace ac
{

class PopCompositionCalculator;

class TurnStageFactory
{
public:
    TurnStageFactory();
    ~TurnStageFactory() = default;

    void SetCompositionCalculator(PopCompositionCalculator* pCalculator);

    bool LoadConfig(const std::string& configPath);
    TurnStageRegistry_t CreateStages();
    const std::vector<TurnStageConfig>& GetStageConfigs() const { return m_stageConfigs; }

private:
    bool ValidateStageId(const std::string& stageId) const;
    bool StageRequiresHook(const std::string& stageId) const;
    std::unique_ptr<TurnStageBase> CreateStageInstance(const TurnStageConfig& config);

    std::vector<TurnStageConfig> m_stageConfigs;
    PopCompositionCalculator* m_pCalculator = nullptr;
};

} // namespace ac
