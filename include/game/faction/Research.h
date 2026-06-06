#pragma once

namespace ac
{

class Research
{
public:
    Research();
    ~Research();

    int GetResearchPoints() const;
    void AddResearchPoints(int points);
    void SetPointsNeededForNextTech(int points);
    int GetPointsNeededForNextTech() const;

private:
    int m_researchPoints = 0;
    int m_pointsNeededForNextTech = 10;
};

} // namespace ac
