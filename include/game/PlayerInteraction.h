#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/PauseOnEventsConfig.h"
#include "game/units/Unit.h"
#include "lib/GameEvent.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

// Informational notice (OK to dismiss). Optional camera focus on a tile.
// `event` selects the PauseOnEventsConfig_t flag that can skip presentation.
struct NoticeInteraction_t
{
    PauseOnEventId_t event = PauseOnEventId_t::NewFacilityBuilt;
    std::string title;
    std::string body;
    std::optional<TileCoord_t> cameraTile;
};

// Open a full overlay; completing the interaction waits until that view closes.
struct OpenViewInteraction_t
{
    enum class View_t
    {
        Base,
        Research,
        UnitDesigner,
        SocialEngineering,
    };
    View_t view = View_t::Base;
    std::optional<BaseId_t> baseId;
    std::string techId;
};

// Production would empty the base; player Complete (allow emptying) or Disable production.
struct ProductionWouldEmptyInteraction_t
{
    FactionId_t factionId = 0;
    BaseId_t baseId = 0;
};

// Production finished with an empty queue; offer Continue or Zoom to base control.
struct ProductionIdleInteraction_t
{
    FactionId_t factionId = 0;
    BaseId_t baseId = 0;
    bool afterCompletion = false;
    std::string completedItemId;
    std::string completedItemName;
    // Applicable pause gates after a completion (OR-matched). Empty idle-without-completion
    // falls back to BuildOrdersOutOfDate in the presenter.
    std::vector<PauseOnEventId_t> completedEvents;
};

// Unit stepped onto a tile with improvement on_visit_effects (Monolith). Investigate applies
// those effects to this unit; Leave it Alone completes without firing.
struct ImprovementVisitInteraction_t
{
    UnitId_t unitId = 0;
};

// A unit whose design has a matching on_hold_effects entry was ordered to Hold at a friendly
// base, or a building was completed under a unit already Holding there. Link applies that
// list; Do nothing leaves the Hold.
struct ArtifactLinkInteraction_t
{
    UnitId_t unitId = 0;
};

using PlayerInteraction_t = std::variant<
    NoticeInteraction_t,
    OpenViewInteraction_t,
    ProductionWouldEmptyInteraction_t,
    ProductionIdleInteraction_t,
    ImprovementVisitInteraction_t,
    ArtifactLinkInteraction_t
>;

// Every queued item pauses turn processing for its audience until CompleteFront.
struct QueuedInteraction_t
{
    PlayerInteraction_t payload;
    // Faction that should see this (normally the player). AI news still uses the player id.
    FactionId_t audience = 0;
};

} // namespace ac
