#include "game/TurnStageFactory.h"
#include "game/TurnStageConfigParser.h"
#include "game/stages/TurnStart.h"
#include "game/stages/NewYearBegins.h"
#include "game/stages/ResourceCollection.h"
#include "game/stages/ResearchAccumulation.h"
#include "game/stages/BaseProduction.h"
#include "game/stages/Population.h"
#include "game/stages/Upkeep.h"
#include "game/stages/PlayerActions.h"
#include "game/stages/WorldEvents.h"
#include "game/stages/VictoryConditionChecks.h"
#include "game/stages/TurnEnd.h"
#include "game/stages/Save.h"
#include "game/stages/CustomTurnStage.h"
#include <iostream>
#include <magic_enum.hpp>

namespace ac
{

TurnStageFactory::TurnStageFactory()
{
}

void TurnStageFactory::SetCompositionCalculator(PopCompositionCalculator* pCalculator)
{
    m_pCalculator = pCalculator;
}

bool TurnStageFactory::LoadConfig(const std::string& configPath)
{
    TurnStageConfigParser parser;
    m_stageConfigs = parser.ParseConfig(configPath);
    
    return !m_stageConfigs.empty();
}

TurnStageRegistry_t TurnStageFactory::CreateStages()
{
    TurnStageRegistry_t registry;
    for (const auto& config : m_stageConfigs)
    {
        // Create and register the stage instance
        auto pStageInstance = CreateStageInstance(config);
        if (pStageInstance)
        {
            // Convert string id to TurnStage enum if valid
            auto stageEnum = magic_enum::enum_cast<TurnStage>(config.id);
            if (stageEnum.has_value())
            {
                registry[stageEnum.value()] = std::move(pStageInstance);
                std::cout << "Registered stage: " << config.id << "\n";
            }
        }
    }
    return registry;
}

std::unique_ptr<TurnStageBase> TurnStageFactory::CreateStageInstance(const TurnStageConfig& config)
{
    auto stageEnum = magic_enum::enum_cast<TurnStage>(config.id);
    
    if (!stageEnum.has_value())
    {
        return std::make_unique<CustomTurnStage>(config.hookContext, config.name);
    }
    
    switch (stageEnum.value())
    {
        case TurnStage::TurnStart:
            return std::make_unique<TurnStart>(config.hookContext);
        case TurnStage::NewYearBegins:
            return std::make_unique<NewYearBegins>(config.hookContext);
        case TurnStage::ResourceCollection:
            return std::make_unique<ResourceCollection>(config.hookContext);
        case TurnStage::ResearchAccumulation:
            return std::make_unique<ResearchAccumulation>(config.hookContext);
        case TurnStage::BaseProduction:
            return std::make_unique<BaseProduction>(config.hookContext);
        case TurnStage::Population:
            return std::make_unique<Population>(config.hookContext, m_pCalculator);
        case TurnStage::Upkeep:
            return std::make_unique<Upkeep>(config.hookContext);
        case TurnStage::PlayerActions:
            return std::make_unique<PlayerActions>(config.hookContext);
        case TurnStage::WorldEvents:
            return std::make_unique<WorldEvents>(config.hookContext);
        case TurnStage::VictoryConditionChecks:
            return std::make_unique<VictoryConditionChecks>(config.hookContext);
        case TurnStage::TurnEnd:
            return std::make_unique<TurnEnd>(config.hookContext);
        case TurnStage::Save:
            return std::make_unique<Save>(config.hookContext);
        default:
            return nullptr;
    }
}

} // namespace ac
