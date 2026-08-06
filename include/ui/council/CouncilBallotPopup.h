#pragma once

#include "game/council/PlanetaryCouncil.h"
#include "ui/UIElement.h"
#include "input/Input.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ac
{

class Faction;

// Ballot chooser for the player's council vote (standard or election).
class CouncilBallotPopup : public UIElement
{
public:
    // Standard: options are Yea / Nay / Abstain. Election: candidate faction or nullopt abstain.
    using StandardCallback_t = std::function<void(CouncilBallot_t)>;
    using ElectionCallback_t = std::function<void(const Faction*)>;

    static std::unique_ptr<CouncilBallotPopup> CreateStandard(
        WindowLayout_t layout,
        StandardCallback_t onSelected
    );
    static std::unique_ptr<CouncilBallotPopup> CreateElection(
        WindowLayout_t layout,
        std::vector<Faction*> candidates,
        ElectionCallback_t onSelected
    );

    void Render(Graphics& rGraphics) override;
    bool IsModal() const override { return true; }
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    enum class Mode_t
    {
        Standard,
        Election,
    };

    CouncilBallotPopup(
        WindowLayout_t layout,
        Mode_t mode,
        std::vector<std::string> labels,
        std::vector<CouncilBallot_t> standardOptions,
        std::vector<Faction*> electionOptions,
        StandardCallback_t onStandard,
        ElectionCallback_t onElection
    );

    void CacheEntryRects_();

    Mode_t m_mode;
    std::vector<std::string> m_labels;
    std::vector<CouncilBallot_t> m_standardOptions;
    std::vector<Faction*> m_electionOptions; // nullptr entry = abstain
    std::vector<Rectangle_t> m_entryRects;
    StandardCallback_t m_onStandard;
    ElectionCallback_t m_onElection;
};

} // namespace ac
