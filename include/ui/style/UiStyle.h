#pragma once

#include "graphics/Graphics.h"
#include "ui/UIElement.h"

#include <string>

namespace ac
{

struct LayoutsStyle_t
{
    RatioLayout_t fullscreen{};
    RatioLayout_t map{};
    RatioLayout_t topPanel{};
    RatioLayout_t leftPanel{};
    RatioLayout_t locationPanel{};
    RatioLayout_t centerPanel{};
    RatioLayout_t bottomPanel{};
    RatioLayout_t rightPanel{};
    RatioLayout_t rightButton{};
    RatioLayout_t popup{};
    RatioLayout_t popupSmall{};
};

struct ViewFactoryStyle_t
{
    float fullscreenOriginX{};
    float fullscreenOriginY{};
};

struct TileRendererStyle_t
{
    Color_t tileBorderColor{};
    Color_t fogTerrainColor{};
    Color_t clearTerrainTextColor{};
    Color_t waterLowColor{};
    Color_t waterHighColor{};
    Color_t landLowColor{};
    Color_t landHighColor{};
    Color_t forestColor{};
    Color_t fungusColor{};
    float fogFillDimRatio{};
    float tileBorderWidth{};
    unsigned int tileFontSize{};
    float tileTextOffsetXRatio{};
    float tileTextOffsetYRatio{};
};

struct WorldDisplayStyle_t
{
    float defaultTileScale{};
    float baseNameFontSizeRatio{};
    float baseTextOffsetRatio{};
    float baseNameWidthRatio{};
    float baseNameCharWidthRatio{};
    Color_t sensorMarkerColor{};
    float sensorMarkerFontSizeRatio{};
    float sensorMarkerWidthRatio{};
    float sensorMarkerHeightRatio{};
    float sensorMarkerInsetRatio{};
    Color_t monolithMarkerColor{};
    float monolithMarkerFontSizeRatio{};
    float monolithMarkerWidthRatio{};
    float monolithMarkerHeightRatio{};
    float monolithMarkerInsetRatio{};
    Color_t shroudColor{};
    Color_t pathPreviewColor{};
    float pathPreviewLineThicknessRatio{};
    Color_t riverColor{};
    float riverLineThicknessRatio{};
    Color_t baseNameColor{};
    Color_t sensorLabelColor{};
    Color_t monolithLabelColor{};
};

struct MinimapDisplayStyle_t
{
    Color_t viewportBorderColor{};
    float viewportBorderWidth{};
};

struct UnitMarkerStyle_t
{
    float fontSizeRatio{};
    float widthRatio{};
    float heightRatio{};
    float spacingRatio{};
    Color_t markerColor{};
    Color_t exhaustedColor{};
    float selectionBorderOffset{};
    float selectionBorderExpansion{};
    float selectionBorderWidth{};
    Color_t selectionBorderColor{};
    Color_t hitOverlayFill{};
    Color_t hitOverlayBorder{};
    float hitOverlayBorderWidth{};
    Color_t initialTextColor{};
};

struct SelectedUnitPanelStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t mutedTextColor{};
    Color_t iconColor{};
    Color_t iconExhaustedColor{};
    Color_t bodyTextColor{};
    Color_t iconInitialTextColor{};
    float paddingRatio{};
    float iconSizeRatio{};
    float iconInitialFontRatio{};
    float textFontRatio{};
    float textGapRatio{};
    float iconInitialOffsetXRatio{};
    float iconInitialOffsetYRatio{};
    float iconCenterRatio{};
};

struct LocationPanelStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t mutedTextColor{};
    Color_t bodyTextColor{};
    float paddingRatio{};
    float previewSizeRatio{};
    float textFontRatio{};
    float textGapRatio{};
};

struct UnitStackPanelStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t statTextColor{};
    float paddingRatio{};
    float slotGapRatio{};
    float iconHeightRatio{};
    float statFontRatio{};
};

struct InfoPanelStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t defaultLineColor{};
    float textHeightEstimate{};
    float textVerticalCenterRatio{};
    float textHorizontalPadding{};
    unsigned int fontSize{};
};

struct EndTurnButtonStyle_t
{
    Color_t idleFillColor{};
    Color_t readyFillColor{};
    Color_t borderColor{};
    Color_t readyBorderColor{};
    Color_t labelColor{};
    unsigned int fontSize{};
    float textPadX{};
    float textPadY{};
    RatioLayout_t layout{};
};

struct CommlinksButtonStyle_t
{
    Color_t fillColor{};
    Color_t borderColor{};
    Color_t labelColor{};
    unsigned int fontSize{};
    float textPadX{};
    float textPadY{};
};

struct WorldViewStyle_t
{
    Color_t researchTextColor{};
    Color_t missionYearColor{};
    Color_t energyTextColor{};
};

struct CombatViewStyle_t
{
    Color_t sideNameColor{};
    Color_t roundHeaderColor{};
    Color_t idleLabelColor{};
    Color_t hpLineColor{};
    Color_t rollsLineColor{};
    Color_t hitLineColor{};
};

struct CombatPresentationStyle_t
{
    int defaultDamageFlashMs{};
    int defaultInterRoundDelayMs{};
};

struct CameraInputStyle_t
{
    float edgeZone{};
    float relativeMin{};
    float relativeMax{};
    int cameraScrollStep{};
    int initialCameraOffset{};
    float edgeScrollSpeed{};
};

struct UnitOrderInputStyle_t
{
    int holdThresholdMs{};
};

struct CommlinksPanelStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t titleColor{};
    Color_t factionNameColor{};
    Color_t statusColor{};
    unsigned int titleFontSize{};
    unsigned int rowFontSize{};
    float titlePadX{};
    float titlePadY{};
    float rowStartY{};
    float rowHeight{};
    float rowPadX{};
    float statusPadX{};
    RatioLayout_t councilButtonLayout{};
};

struct CouncilVoteViewStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t factionNameColor{};
    Color_t ballotColor{};
    Color_t weightColor{};
    Color_t headerColor{};
    Color_t nameColor{};
    Color_t tallyColor{};
    unsigned int factionFontSize{};
    unsigned int ballotFontSize{};
    unsigned int weightFontSize{};
    unsigned int headerFontSize{};
    unsigned int nameFontSize{};
    unsigned int tallyFontSize{};
    float paddingRatio{};
    float lineHeightRatio{};
    RatioLayout_t voteButtonLayout{};
};

struct CurrentResearchPanelStyle_t
{
    RatioLayout_t labelLayout{};
    RatioLayout_t targetLayout{};
    RatioLayout_t progressLayout{};
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t labelColor{};
    Color_t targetColor{};
    Color_t progressColor{};
    unsigned int labelFontSize{};
    unsigned int targetFontSize{};
    unsigned int progressFontSize{};
};

struct SettingsPanelStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t titleColor{};
    Color_t rowColor{};
    unsigned int titleFontSize{};
    unsigned int rowFontSize{};
    RatioLayout_t titleLayout{};
    RatioLayout_t rowLayout{};
};

struct BaseViewStyle_t
{
    RatioLayout_t growthLayout{};
    RatioLayout_t workableLayout{};
    RatioLayout_t buildingsLayout{};
    RatioLayout_t productionLayout{};
    RatioLayout_t buildQueueLayout{};
    RatioLayout_t baseNameLayout{};
    RatioLayout_t populationLayout{};
};

// Header plus stockpile / required / production lines. The growth and production panels are
// the same widget with different numbers in it, so they share one type and one parser — kept
// twice, a one-sided tweak silently desynced the two halves of the base screen.
struct ResourceLinesPanelStyle_t
{
    Color_t backgroundColor{};
    Color_t textColor{};
    float headerFontSizeRatio{};
    float entryFontSizeRatio{};
    float lineHeightRatio{};
    float leftPaddingRatio{};
    float stockpileLineIndex{};
    float requiredLineIndex{};
    float productionLineIndex{};
};

struct PopulationDisplayStyle_t
{
    Color_t backgroundColor{};
    Color_t headerTextColor{};
    float headerFontSizeRatio{};
    float popBoxSizeRatio{};
    float popBoxSpacingRatio{};
    float leftPaddingRatio{};
    float popBoxFontSizeRatio{};
    float popRowYOffsetRatio{};
    float popBoxTextXOffsetRatio{};
    float popBoxTextYOffsetRatio{};
    float popBoxBorderWidth{};
    Color_t popBoxFillColor{};
    Color_t popBoxBorderColor{};
    Color_t popLetterColor{};
};

struct SupportDisplayStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    float paddingRatio{};
    float iconSizeRatio{};
    float iconGapRatio{};
};

struct BaseWorkableAreaDisplayStyle_t
{
    int gridDimension{};
    float gridCenterOffset{};
    Color_t backgroundColor{};
    Color_t tileBorderColor{};
    float tileBorderWidth{};
    unsigned int baseLabelFontSize{};
    unsigned int tileFontSize{};
    float tileTextOffsetXRatio{};
    float tileTextOffsetYRatio{};
    Color_t baseLabelColor{};
    Color_t workedTileTextColor{};
    Color_t unworkedTileTextColor{};
    // Workable, but held by a neighbouring base / another faction / a supply crawler, so this
    // base cannot take it. Distinct from unworked so a refused click is predictable.
    Color_t unavailableTileTextColor{};
};

// Every modal list-selector's metrics. One type, so a tweak cannot land on one screen only;
// distinct looks are distinct *instances* of it (see componentSelectorPopup).
// Chrome for NoticePopup. Its own block rather than borrowing satelliteView's — a generic
// widget reading another screen's style is how a production tweak came to restyle council UI.
struct NoticePopupStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t headerColor{};
    Color_t messageColor{};
    RatioLayout_t okButtonLayout{};
};

struct ListSelectorPopupStyle_t
{
    float headerFontSizeRatio{};
    float entryFontSizeRatio{};
    float lineHeightRatio{};
    float paddingRatio{};
    float headerLineOffset{};
    float borderWidth{};
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t headerColor{};
    Color_t hintColor{};
    Color_t entryColor{};
};

struct SocialEngineeringDisplayStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t activePolicyColor{};
    Color_t inactivePolicyColor{};
    Color_t categoryColor{};
    Color_t scoreHeaderColor{};
    Color_t hiddenPolicySlotColor{};
    Color_t scoreValueColor{};
    Color_t factionBonusBodyColor{};
    float scoresPanelWidthRatio{};
    float categoryRowHeightRatio{};
    float categoryNameRowRatio{};
    float policyNameRowRatio{};
    float policyBonusRowRatio{};
    float horizontalPaddingRatio{};
    float verticalPaddingRatio{};
    float headerFontSizeRatio{};
    float categoryFontSizeRatio{};
    float policyFontSizeRatio{};
    float bonusFontSizeRatio{};
    float scoreFontSizeRatio{};
    float factionBonusSectionRatio{};
    float factionNameRowRatio{};
    float factionBonusRowRatio{};
    float factionNameFontSizeRatio{};
    float factionBonusFontSizeRatio{};
};

struct SocialEngineeringBottomPanelStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t valueColor{};
    float rowHeightRatio{};
    float horizontalPaddingRatio{};
    float verticalPaddingRatio{};
    float valueFontSizeRatio{};
};

struct UnitDesignerViewStyle_t
{
    RatioLayout_t leftSlotColumnLayout{};
    RatioLayout_t designStatsLayout{};
    RatioLayout_t rightSlotColumnLayout{};
};

struct SlotColumnPanelStyle_t
{
    int visibleSlots{};
    float arrowHeightRatio{};
    float labelFontSizeRatio{};
    float nameFontSizeRatio{};
    float paddingRatio{};
    float arrowAreaMultiplier{};
    float arrowFontSizeRatio{};
    float arrowPadXRatio{};
    float nameLabelSpacingMultiplier{};
    Color_t arrowFillColor{};
    Color_t arrowBorderColor{};
    Color_t disabledArrowColor{};
    Color_t enabledArrowColor{};
    Color_t slotFillColor{};
    Color_t slotBorderColor{};
    Color_t labelTextColor{};
    Color_t emptyNameColor{};
    Color_t filledNameColor{};
};

struct DesignStatsDisplayStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t incompleteTextColor{};
    Color_t statTextColor{};
    Color_t saveButtonFillColor{};
    Color_t headerColor{};
    Color_t saveButtonBorderColor{};
    Color_t saveButtonTextColor{};
    float headerFontSizeRatio{};
    float statFontSizeRatio{};
    float lineHeightRatio{};
    float paddingRatio{};
    float saveButtonHeightRatio{};
    float saveButtonTextYOffsetRatio{};
    float horizontalPaddingMultiplier{};
};

struct DesignListPanelStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t emptyListTextColor{};
    Color_t selectedBoxFillColor{};
    Color_t unselectedBoxFillColor{};
    Color_t unselectedBoxBorderColor{};
    Color_t selectedBoxBorderColor{};
    float boxWidthRatio{};
    float boxPaddingRatio{};
    float fontSizeRatio{};
    float labelFontRatio{};
    float emptyListPaddingMultiplier{};
    float verticalPaddingMultiplier{};
    float textPadRatio{};
    float textVerticalRatio{};
};

struct UnitStatusPanelStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t mutedTextColor{};
    Color_t headerColor{};
    float headerFontSizeRatio{};
    float statFontSizeRatio{};
    float lineHeightRatio{};
    float paddingRatio{};
    float designNameLineIndex{};
    float activeCountLineIndex{};
    float inProdLineIndex{};
};

struct SatelliteViewStyle_t
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t headerColor{};
    Color_t factionNameColor{};
    Color_t cellColor{};
    Color_t tabFillColor{};
    Color_t tabSelectedFillColor{};
    Color_t tabBorderColor{};
    Color_t tabLabelColor{};
    unsigned int tabFontSize{};
    unsigned int headerFontSize{};
    unsigned int factionFontSize{};
    unsigned int cellFontSize{};
    float paddingRatio{};
    float tabTextPadX{};
    float tabTextPadY{};
    RatioLayout_t summaryTabLayout{};
    RatioLayout_t attackTabLayout{};
    RatioLayout_t contentLayout{};
    RatioLayout_t attackButtonLayout{};
    RatioLayout_t attackerListLayout{};
    RatioLayout_t attackerConfirmLayout{};
    RatioLayout_t attackerCancelLayout{};
    RatioLayout_t outcomeOkLayout{};
    float listButtonHeight{};
    float listButtonGap{};
    float listHeaderHeight{};
    float listPadX{};
    float listPadY{};
};

class UiStyle
{
public:
    // Loads config/ui/style.json (or an absolute/relative path to that file).
    static void Load(const std::string& filePath);
    static const UiStyle& Get();

    LayoutsStyle_t layouts;
    ViewFactoryStyle_t viewFactory;
    TileRendererStyle_t tileRenderer;
    WorldDisplayStyle_t worldDisplay;
    MinimapDisplayStyle_t minimapDisplay;
    UnitMarkerStyle_t unitMarker;
    SelectedUnitPanelStyle_t selectedUnitPanel;
    LocationPanelStyle_t locationPanel;
    UnitStackPanelStyle_t unitStackPanel;
    InfoPanelStyle_t infoPanel;
    EndTurnButtonStyle_t endTurnButton;
    CommlinksButtonStyle_t commlinksButton;
    WorldViewStyle_t worldView;
    CombatViewStyle_t combatView;
    CombatPresentationStyle_t combatPresentation;
    CameraInputStyle_t cameraInput;
    UnitOrderInputStyle_t unitOrderInput;
    CommlinksPanelStyle_t commlinksPanel;
    CouncilVoteViewStyle_t councilVoteView;
    CurrentResearchPanelStyle_t currentResearchPanel;
    SettingsPanelStyle_t settingsPanel;
    BaseViewStyle_t baseView;
    // Two instances of one type: same widget, independent values.
    ResourceLinesPanelStyle_t growthDisplay;
    ResourceLinesPanelStyle_t productionDisplay;
    PopulationDisplayStyle_t populationDisplay;
    SupportDisplayStyle_t supportDisplay;
    BaseWorkableAreaDisplayStyle_t baseWorkableAreaDisplay;
    ListSelectorPopupStyle_t listSelectorPopup;
    NoticePopupStyle_t noticePopup;
    SocialEngineeringDisplayStyle_t socialEngineeringDisplay;
    SocialEngineeringBottomPanelStyle_t socialEngineeringBottomPanel;
    UnitDesignerViewStyle_t unitDesignerView;
    SlotColumnPanelStyle_t slotColumnPanel;
    // Its own instance of the shared type: the unit designer's picker keeps distinct colours
    // and metrics without a second widget or a second style shape.
    ListSelectorPopupStyle_t componentSelectorPopup;
    DesignStatsDisplayStyle_t designStatsDisplay;
    DesignListPanelStyle_t designListPanel;
    UnitStatusPanelStyle_t unitStatusPanel;
    SatelliteViewStyle_t satelliteView;
};

inline const UiStyle& Style()
{
    return UiStyle::Get();
}

} // namespace ac
