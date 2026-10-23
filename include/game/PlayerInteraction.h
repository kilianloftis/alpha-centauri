#pragma once

#include "game/diplomacy/DiplomaticStatus.h"
#include "game/base/BaseTypes.h"
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
    // The pause-on-events setting that gates it; unset for a notice that always shows.
    std::optional<PauseOnEventId_t> event;
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

// The current item can no longer be built; this turn's minerals are already on it.
struct ProductionPreemptedInteraction_t
{
    FactionId_t factionId = 0;
    BaseId_t baseId = 0;
    std::string droppedItemName;
};

// Unit stepped onto a tile with improvement on_visit_effects (Monolith). Investigate applies
// those effects to this unit; Leave it Alone completes without firing.
struct ImprovementVisitInteraction_t
{
    UnitId_t unitId = 0;
};

// A Hold at a friendly base with a matching on_hold_effects entry, or a building completed
// under one. Link applies the list.
struct ArtifactLinkInteraction_t
{
    UnitId_t unitId = 0;
};

// aggressor attacked allyId, to whom the player owes defence: declare Vendetta on aggressor, or
// decline and let the agreement with allyId step down.
struct PactObligationInteraction_t
{
    FactionId_t allyId = 0;
    FactionId_t aggressorId = 0;
    VendettaKind_t kind = VendettaKind_t::Declaration;
};

// A player unit's move order stopped at the border of territory its faction may not enter
// (a Treaty). Break the agreement (Vendetta with the owner) and continue, or cancel the order.
struct TerritoryEntryInteraction_t
{
    UnitId_t unitId = 0;
    FactionId_t ownerId = 0;
};

using PlayerInteraction_t = std::variant<
    NoticeInteraction_t,
    OpenViewInteraction_t,
    ProductionWouldEmptyInteraction_t,
    ProductionIdleInteraction_t,
    ProductionPreemptedInteraction_t,
    ImprovementVisitInteraction_t,
    ArtifactLinkInteraction_t,
    PactObligationInteraction_t,
    TerritoryEntryInteraction_t
>;

// Every queued item pauses turn processing for its audience until CompleteFront.
struct QueuedInteraction_t
{
    PlayerInteraction_t payload;
    // Faction that should see this (normally the player). AI news still uses the player id.
    FactionId_t audience = 0;
};

} // namespace ac
