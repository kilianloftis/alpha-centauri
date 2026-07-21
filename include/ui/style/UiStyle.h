#pragma once

#include "graphics/Graphics.h"
#include "ui/UIElement.h"

#include <string>

namespace ac
{

struct LayoutsStyle
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

struct ViewFactoryStyle
{
    float fullscreenOriginX{};
    float fullscreenOriginY{};
};

struct TileRendererStyle
{
    Color_t tileBorderColor{};
    Color_t fogTerrainColor{};
    Color_t clearTerrainTextColor{};
    float tileBorderWidth{};
    unsigned int tileFontSize{};
    float tileTextOffsetXRatio{};
    float tileTextOffsetYRatio{};
};

struct WorldDisplayStyle
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
    Color_t shroudColor{};
    Color_t pathPreviewColor{};
    float pathPreviewLineThicknessRatio{};
    Color_t baseNameColor{};
    Color_t sensorLabelColor{};
};

struct UnitMarkerStyle
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

struct SelectedUnitPanelStyle
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

struct LocationPanelStyle
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

struct UnitStackPanelStyle
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t statTextColor{};
    float paddingRatio{};
    float slotGapRatio{};
    float iconHeightRatio{};
    float statFontRatio{};
};

struct InfoPanelStyle
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t defaultLineColor{};
    float textHeightEstimate{};
    float textVerticalCenterRatio{};
    float textHorizontalPadding{};
    unsigned int fontSize{};
};

struct EndTurnButtonStyle
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

struct CommlinksButtonStyle
{
    Color_t fillColor{};
    Color_t borderColor{};
    Color_t labelColor{};
    unsigned int fontSize{};
    float textPadX{};
    float textPadY{};
};

struct WorldViewStyle
{
    Color_t researchTextColor{};
    Color_t missionYearColor{};
    Color_t energyTextColor{};
};

struct CombatViewStyle
{
    Color_t sideNameColor{};
    Color_t roundHeaderColor{};
    Color_t idleLabelColor{};
    Color_t hpLineColor{};
    Color_t rollsLineColor{};
    Color_t hitLineColor{};
};

struct CombatPresentationStyle
{
    int defaultDamageFlashMs{};
    int defaultInterRoundDelayMs{};
};

struct CameraInputStyle
{
    float edgeZone{};
    float relativeMin{};
    float relativeMax{};
    int cameraScrollStep{};
    int initialCameraOffset{};
    float edgeScrollSpeed{};
};

struct UnitOrderInputStyle
{
    int holdThresholdMs{};
};

struct CommlinksPanelStyle
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
};

struct CurrentResearchPanelStyle
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

struct SettingsPanelStyle
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

struct BaseViewStyle
{
    float leftColumnY{};
    float leftColumnHeightMultiplier{};
    RatioLayout_t growthHalfLayout{};
    RatioLayout_t productionHalfLayout{};
};

struct GrowthDisplayStyle
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

struct ProductionDisplayStyle
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

struct PopulationDisplayStyle
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

struct BaseWorkableAreaDisplayStyle
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
};

struct PopTypeSelectorPopupStyle
{
    float headerFontSizeRatio{};
    float entryFontSizeRatio{};
    float lineHeightRatio{};
    float paddingRatio{};
    float headerLineOffset{};
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t headerColor{};
    Color_t hintColor{};
    Color_t entryColor{};
};

struct ProductionSelectorPopupStyle
{
    float headerFontSizeRatio{};
    float entryFontSizeRatio{};
    float lineHeightRatio{};
    float paddingRatio{};
    float headerLineOffset{};
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t headerColor{};
    Color_t hintColor{};
    Color_t entryColor{};
};

struct SocialEngineeringDisplayStyle
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

struct SocialEngineeringBottomPanelStyle
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t valueColor{};
    float rowHeightRatio{};
    float horizontalPaddingRatio{};
    float verticalPaddingRatio{};
    float valueFontSizeRatio{};
};

struct UnitDesignerViewStyle
{
    RatioLayout_t leftSlotColumnLayout{};
    RatioLayout_t designStatsLayout{};
    RatioLayout_t rightSlotColumnLayout{};
};

struct SlotColumnPanelStyle
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

struct ComponentSlotDisplayStyle
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t labelTextColor{};
    Color_t emptyNameColor{};
    Color_t filledNameColor{};
    float labelFontSizeRatio{};
    float nameFontSizeRatio{};
    float paddingRatio{};
    float nameLabelSpacingMultiplier{};
};

struct ComponentSelectorPopupStyle
{
    Color_t backgroundColor{};
    Color_t borderColor{};
    Color_t titleColor{};
    Color_t entryColor{};
    float borderWidth{};
    float titleFontSizeRatio{};
    float entryFontSizeRatio{};
    float entryHeightRatio{};
    float paddingRatio{};
    float titleHeightMultiplier{};
};

struct DesignStatsDisplayStyle
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

struct DesignListPanelStyle
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

struct UnitStatusPanelStyle
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

class UiStyle
{
public:
    // Loads config/ui/style.json (or an absolute/relative path to that file).
    static void Load(const std::string& filePath);
    static const UiStyle& Get();
    static bool IsLoaded();

    LayoutsStyle layouts;
    ViewFactoryStyle viewFactory;
    TileRendererStyle tileRenderer;
    WorldDisplayStyle worldDisplay;
    UnitMarkerStyle unitMarker;
    SelectedUnitPanelStyle selectedUnitPanel;
    LocationPanelStyle locationPanel;
    UnitStackPanelStyle unitStackPanel;
    InfoPanelStyle infoPanel;
    EndTurnButtonStyle endTurnButton;
    CommlinksButtonStyle commlinksButton;
    WorldViewStyle worldView;
    CombatViewStyle combatView;
    CombatPresentationStyle combatPresentation;
    CameraInputStyle cameraInput;
    UnitOrderInputStyle unitOrderInput;
    CommlinksPanelStyle commlinksPanel;
    CurrentResearchPanelStyle currentResearchPanel;
    SettingsPanelStyle settingsPanel;
    BaseViewStyle baseView;
    GrowthDisplayStyle growthDisplay;
    ProductionDisplayStyle productionDisplay;
    PopulationDisplayStyle populationDisplay;
    BaseWorkableAreaDisplayStyle baseWorkableAreaDisplay;
    PopTypeSelectorPopupStyle popTypeSelectorPopup;
    ProductionSelectorPopupStyle productionSelectorPopup;
    SocialEngineeringDisplayStyle socialEngineeringDisplay;
    SocialEngineeringBottomPanelStyle socialEngineeringBottomPanel;
    UnitDesignerViewStyle unitDesignerView;
    SlotColumnPanelStyle slotColumnPanel;
    ComponentSlotDisplayStyle componentSlotDisplay;
    ComponentSelectorPopupStyle componentSelectorPopup;
    DesignStatsDisplayStyle designStatsDisplay;
    DesignListPanelStyle designListPanel;
    UnitStatusPanelStyle unitStatusPanel;
};

inline const UiStyle& Style()
{
    return UiStyle::Get();
}

} // namespace ac
