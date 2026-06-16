#pragma once

namespace ac
{

struct SocialScores
{
    int economy    = 0;  // → energy credits per base per turn
    int efficiency = 0;  // → bureaucracy loss reduction
    int support    = 0;  // → free unit support per base
    int police     = 0;  // → drone control capacity
    int morale     = 0;  // → unit combat bonus
    int growth     = 0;  // → population growth rate modifier
    int planet     = 0;  // → native life interaction
    int research   = 0;  // → research output modifier
    int industry   = 0;  // → production output modifier
    int probe      = 0;  // → probe effectiveness
};

} // namespace ac