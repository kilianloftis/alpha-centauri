#include "game/faction/base/BaseManager.h"
#include "game/faction/base/WorkerAssignmentManager.h"
#include "game/faction/population/PopulationManager.h"
#include "game/faction/population/PopContainer.h"
#include <cmath>

namespace ac
{

BaseManager::BaseManager()
    : m_factionId(-1)
    , m_baseId(-1)
    , m_x(0)
    , m_y(0)
    , m_pPopulation(std::make_unique<PopulationManager>())
    , m_pWorkerAssignments(std::make_unique<WorkerAssignmentManager>())
{
    m_pPopulation->on_pop_gained.connect([this](int) {
        m_pWorkerAssignments->OnPopulationChanged(m_pPopulation->GetContainer());
    });
    m_pPopulation->on_pop_lost.connect([this](int) {
        m_pWorkerAssignments->OnPopulationChanged(m_pPopulation->GetContainer());
    });
}

BaseManager::~BaseManager()
{
}

PopulationManager* BaseManager::GetPopulation()
{
    return m_pPopulation.get();
}

const PopulationManager* BaseManager::GetPopulation() const
{
    return m_pPopulation.get();
}

void BaseManager::AddPop()
{
    if (m_pPopulation)
    {
        m_pPopulation->AddPop();
    }
}

WorkerAssignmentManager& BaseManager::GetWorkerAssignments()
{
    return *m_pWorkerAssignments;
}

const WorkerAssignmentManager& BaseManager::GetWorkerAssignments() const
{
    return *m_pWorkerAssignments;
}

ResourceManager* BaseManager::GetResourceManager()
{
    return nullptr;
}

const ResourceManager* BaseManager::GetResourceManager() const
{
    return nullptr;
}

void BaseManager::SetPosition(int x, int y)
{
    m_x = x;
    m_y = y;
}

int BaseManager::GetX() const
{
    return m_x;
}

int BaseManager::GetY() const
{
    return m_y;
}

std::vector<std::pair<int, int>> BaseManager::GetWorkableTilePositions() const
{
    static constexpr int kGridHalfExtent = 2;   // [-2, 2] bounding box
    static constexpr int kManhattanLimit  = 3;   // excludes corners (|dx|+|dy|==4)
    std::vector<std::pair<int, int>> tiles;
    for (int dy = -kGridHalfExtent; dy <= kGridHalfExtent; ++dy)
    {
        for (int dx = -kGridHalfExtent; dx <= kGridHalfExtent; ++dx)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }
            if (std::abs(dx) + std::abs(dy) <= kManhattanLimit)
            {
                tiles.emplace_back(m_x + dx, m_y + dy);
            }
        }
    }
    return tiles;
}

void BaseManager::SetName(const std::string& name)
{
    m_name = name;
}

const std::string& BaseManager::GetName() const
{
    return m_name;
}

void BaseManager::SetFactionId(FactionId factionId)
{
    m_factionId = factionId;
}

FactionId BaseManager::GetFactionId() const
{
    return m_factionId;
}

void BaseManager::SetBaseId(int baseId)
{
    m_baseId = baseId;
}

int BaseManager::GetBaseId() const
{
    return m_baseId;
}

} // namespace ac
