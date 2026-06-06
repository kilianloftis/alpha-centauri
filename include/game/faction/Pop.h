#pragma once

#include <memory>

namespace ac
{

// Forward declarations
struct SpecialistOutput_t;
class Specialist;

// Production output from a pop
struct PopProduction_t
{
    int nutrients;
    int energy;
    int minerals;
    int labs;   // Research output (mainly from specialists)
    int psych;  // Psych output (mainly from talents/specialists)
};

// Abstract base class for all population units
class Pop
{
public:
    Pop();
    virtual ~Pop();

    // Get the type identifier for this pop
    virtual const char* GetPopType() const = 0;

    // Check if this pop works a tile
    virtual bool IsWorker() const = 0;

    // Check if this pop is a drone (does nothing)
    virtual bool IsDrone() const = 0;

    // Check if this pop is a specialist
    virtual bool IsSpecialist() const = 0;

    // Get production from this pop (tile resources provided for workers)
    virtual PopProduction_t GetProduction(const PopProduction_t& tileResources) const = 0;

protected:
    int m_tileId;  // Tile assignment (-1 if none)
};

// Worker - works a tile to collect resources
class WorkerPop : public Pop
{
public:
    WorkerPop();
    ~WorkerPop() override;

    const char* GetPopType() const override;
    bool IsWorker() const override;
    bool IsDrone() const override;
    bool IsSpecialist() const override;

    // Tile assignment
    void SetTileId(int tileId);
    int GetTileId() const;

    // Get production from worked tile
    PopProduction_t GetProduction(const PopProduction_t& tileResources) const override;

protected:
    bool m_bIsTalent;
};

// Talent - works a tile like a worker, also contributes to psych stability
class TalentPop : public WorkerPop
{
public:
    TalentPop();
    ~TalentPop() override;

    const char* GetPopType() const override;

    // Get production from worked tile + talent psych bonus
    PopProduction_t GetProduction(const PopProduction_t& tileResources) const override;
};

// Drone - does nothing, causes unrest
class DronePop : public Pop
{
public:
    DronePop();
    ~DronePop() override;

    const char* GetPopType() const override;
    bool IsWorker() const override;
    bool IsDrone() const override;
    bool IsSpecialist() const override;

    // Drones produce nothing
    PopProduction_t GetProduction(const PopProduction_t& tileResources) const override;
};

// Specialist - produces resources directly, not from tiles
class SpecialistPop : public Pop
{
public:
    SpecialistPop(std::unique_ptr<Specialist> pSpecialist);
    ~SpecialistPop() override;

    const char* GetPopType() const override;
    bool IsWorker() const override;
    bool IsDrone() const override;
    bool IsSpecialist() const override;

    // Get the specialist type for output calculation
    const Specialist* GetSpecialist() const;

    // Get production from specialist abilities (tile resources ignored)
    PopProduction_t GetProduction(const PopProduction_t& tileResources) const override;

protected:
    std::unique_ptr<Specialist> m_pSpecialist;
};

} // namespace ac
