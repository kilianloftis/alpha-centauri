#pragma once

namespace ac
{

// Output contributed by a specialist to base resources
struct SpecialistOutput_t
{
    int nutrients;  // Nutrients contribution (rare for specialists)
    int energy;     // Energy (econ) contribution
    int minerals;   // Minerals contribution (rare for specialists)
    int labs;       // Research contribution
    int psych;      // Psych contribution
};

// Abstract base class for all specialist types
// Specialists directly add to base outputs (econ, labs, psych)
// Player selects which pops become specialists and their type
// Specific specialist types ( Librarian, Engineer, etc.) will be implemented later
class Specialist
{
public:
    Specialist();
    virtual ~Specialist();

    // Get the combined output this specialist contributes to all resources
    virtual SpecialistOutput_t GetOutput() const = 0;

    // Get the base output values (before modifiers)
    virtual SpecialistOutput_t GetBaseOutput() const = 0;

protected:
    SpecialistOutput_t m_baseOutput;
};

} // namespace ac
