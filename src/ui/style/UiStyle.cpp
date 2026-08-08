#include "ui/style/UiStyle.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace ac
{

namespace
{

UiStyle g_style{};
bool g_loaded = false;

Color_t ParseColor_(const nlohmann::json& j, const char* key)
{
    const auto& arr = j.at(key);
    if (!arr.is_array() || arr.size() < 3 || arr.size() > 4)
    {
        throw std::runtime_error(std::string("Expected an RGB or RGBA array for '") + key
                                 + "'; extra entries are a typo, not optional data");
    }
    const uint8_t a = arr.size() >= 4 ? arr.at(3).get<uint8_t>() : 255;
    return Color_t{
        arr.at(0).get<uint8_t>(),
        arr.at(1).get<uint8_t>(),
        arr.at(2).get<uint8_t>(),
        a};
}

RatioLayout_t ParseLayout_(const nlohmann::json& j, const char* key)
{
    const auto& arr = j.at(key);
    if (!arr.is_array() || arr.size() != 4)
    {
        throw std::runtime_error(std::string("Expected [x,y,w,h] layout for '") + key + "'");
    }
    return RatioLayout_t{
        arr.at(0).get<float>(),
        arr.at(1).get<float>(),
        arr.at(2).get<float>(),
        arr.at(3).get<float>()};
}

LayoutsStyle_t ParseLayoutsStyle_(const nlohmann::json& j)
{
    LayoutsStyle_t s{};
    s.fullscreen = ParseLayout_(j, "fullscreen");
    s.map = ParseLayout_(j, "map");
    s.topPanel = ParseLayout_(j, "top_panel");
    s.leftPanel = ParseLayout_(j, "left_panel");
    s.locationPanel = ParseLayout_(j, "location_panel");
    s.centerPanel = ParseLayout_(j, "center_panel");
    s.bottomPanel = ParseLayout_(j, "bottom_panel");
    s.rightPanel = ParseLayout_(j, "right_panel");
    s.rightButton = ParseLayout_(j, "right_button");
    s.popup = ParseLayout_(j, "popup");
    s.popupSmall = ParseLayout_(j, "popup_small");
    return s;
}

ViewFactoryStyle_t ParseViewFactoryStyle_(const nlohmann::json& j)
{
    ViewFactoryStyle_t s{};
    s.fullscreenOriginX = j.at("fullscreen_origin_x").get<float>();
    s.fullscreenOriginY = j.at("fullscreen_origin_y").get<float>();
    return s;
}

TileRendererStyle_t ParseTileRendererStyle_(const nlohmann::json& j)
{
    TileRendererStyle_t s{};
    s.tileBorderColor = ParseColor_(j, "tile_border_color");
    s.fogTerrainColor = ParseColor_(j, "fog_terrain_color");
    s.clearTerrainTextColor = ParseColor_(j, "clear_terrain_text_color");
    s.waterLowColor = ParseColor_(j, "water_low_color");
    s.waterHighColor = ParseColor_(j, "water_high_color");
    s.landLowColor = ParseColor_(j, "land_low_color");
    s.landHighColor = ParseColor_(j, "land_high_color");
    s.forestColor = ParseColor_(j, "forest_color");
    s.fungusColor = ParseColor_(j, "fungus_color");
    s.fogFillDimRatio = j.at("fog_fill_dim_ratio").get<float>();
    s.tileBorderWidth = j.at("tile_border_width").get<float>();
    s.tileFontSize = j.at("tile_font_size").get<unsigned int>();
    s.tileTextOffsetXRatio = j.at("tile_text_offset_x_ratio").get<float>();
    s.tileTextOffsetYRatio = j.at("tile_text_offset_y_ratio").get<float>();
    return s;
}

WorldDisplayStyle_t ParseWorldDisplayStyle_(const nlohmann::json& j)
{
    WorldDisplayStyle_t s{};
    s.defaultTileScale = j.at("default_tile_scale").get<float>();
    s.baseNameFontSizeRatio = j.at("base_name_font_size_ratio").get<float>();
    s.baseTextOffsetRatio = j.at("base_text_offset_ratio").get<float>();
    s.baseNameWidthRatio = j.at("base_name_width_ratio").get<float>();
    s.baseNameCharWidthRatio = j.at("base_name_char_width_ratio").get<float>();
    s.sensorMarkerColor = ParseColor_(j, "sensor_marker_color");
    s.sensorMarkerFontSizeRatio = j.at("sensor_marker_font_size_ratio").get<float>();
    s.sensorMarkerWidthRatio = j.at("sensor_marker_width_ratio").get<float>();
    s.sensorMarkerHeightRatio = j.at("sensor_marker_height_ratio").get<float>();
    s.sensorMarkerInsetRatio = j.at("sensor_marker_inset_ratio").get<float>();
    s.monolithMarkerColor = ParseColor_(j, "monolith_marker_color");
    s.monolithMarkerFontSizeRatio = j.at("monolith_marker_font_size_ratio").get<float>();
    s.monolithMarkerWidthRatio = j.at("monolith_marker_width_ratio").get<float>();
    s.monolithMarkerHeightRatio = j.at("monolith_marker_height_ratio").get<float>();
    s.monolithMarkerInsetRatio = j.at("monolith_marker_inset_ratio").get<float>();
    s.shroudColor = ParseColor_(j, "shroud_color");
    s.pathPreviewColor = ParseColor_(j, "path_preview_color");
    s.pathPreviewLineThicknessRatio = j.at("path_preview_line_thickness_ratio").get<float>();
    s.riverColor = ParseColor_(j, "river_color");
    s.riverLineThicknessRatio = j.at("river_line_thickness_ratio").get<float>();
    s.baseNameColor = ParseColor_(j, "base_name_color");
    s.sensorLabelColor = ParseColor_(j, "sensor_label_color");
    s.monolithLabelColor = ParseColor_(j, "monolith_label_color");
    return s;
}

MinimapDisplayStyle_t ParseMinimapDisplayStyle_(const nlohmann::json& j)
{
    MinimapDisplayStyle_t s{};
    s.viewportBorderColor = ParseColor_(j, "viewport_border_color");
    s.viewportBorderWidth = j.at("viewport_border_width").get<float>();
    return s;
}

UnitMarkerStyle_t ParseUnitMarkerStyle_(const nlohmann::json& j)
{
    UnitMarkerStyle_t s{};
    s.fontSizeRatio = j.at("font_size_ratio").get<float>();
    s.widthRatio = j.at("width_ratio").get<float>();
    s.heightRatio = j.at("height_ratio").get<float>();
    s.spacingRatio = j.at("spacing_ratio").get<float>();
    s.markerColor = ParseColor_(j, "marker_color");
    s.exhaustedColor = ParseColor_(j, "exhausted_color");
    s.selectionBorderOffset = j.at("selection_border_offset").get<float>();
    s.selectionBorderExpansion = j.at("selection_border_expansion").get<float>();
    s.selectionBorderWidth = j.at("selection_border_width").get<float>();
    s.selectionBorderColor = ParseColor_(j, "selection_border_color");
    s.hitOverlayFill = ParseColor_(j, "hit_overlay_fill");
    s.hitOverlayBorder = ParseColor_(j, "hit_overlay_border");
    s.hitOverlayBorderWidth = j.at("hit_overlay_border_width").get<float>();
    s.initialTextColor = ParseColor_(j, "initial_text_color");
    return s;
}

SelectedUnitPanelStyle_t ParseSelectedUnitPanelStyle_(const nlohmann::json& j)
{
    SelectedUnitPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.mutedTextColor = ParseColor_(j, "muted_text_color");
    s.iconColor = ParseColor_(j, "icon_color");
    s.iconExhaustedColor = ParseColor_(j, "icon_exhausted_color");
    s.bodyTextColor = ParseColor_(j, "body_text_color");
    s.iconInitialTextColor = ParseColor_(j, "icon_initial_text_color");
    s.paddingRatio = j.at("padding_ratio").get<float>();
    s.iconSizeRatio = j.at("icon_size_ratio").get<float>();
    s.iconInitialFontRatio = j.at("icon_initial_font_ratio").get<float>();
    s.textFontRatio = j.at("text_font_ratio").get<float>();
    s.textGapRatio = j.at("text_gap_ratio").get<float>();
    s.iconInitialOffsetXRatio = j.at("icon_initial_offset_x_ratio").get<float>();
    s.iconInitialOffsetYRatio = j.at("icon_initial_offset_y_ratio").get<float>();
    s.iconCenterRatio = j.at("icon_center_ratio").get<float>();
    return s;
}

LocationPanelStyle_t ParseLocationPanelStyle_(const nlohmann::json& j)
{
    LocationPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.mutedTextColor = ParseColor_(j, "muted_text_color");
    s.bodyTextColor = ParseColor_(j, "body_text_color");
    s.paddingRatio = j.at("padding_ratio").get<float>();
    s.previewSizeRatio = j.at("preview_size_ratio").get<float>();
    s.textFontRatio = j.at("text_font_ratio").get<float>();
    s.textGapRatio = j.at("text_gap_ratio").get<float>();
    return s;
}

UnitStackPanelStyle_t ParseUnitStackPanelStyle_(const nlohmann::json& j)
{
    UnitStackPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.statTextColor = ParseColor_(j, "stat_text_color");
    s.paddingRatio = j.at("padding_ratio").get<float>();
    s.slotGapRatio = j.at("slot_gap_ratio").get<float>();
    s.iconHeightRatio = j.at("icon_height_ratio").get<float>();
    s.statFontRatio = j.at("stat_font_ratio").get<float>();
    return s;
}

InfoPanelStyle_t ParseInfoPanelStyle_(const nlohmann::json& j)
{
    InfoPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.defaultLineColor = ParseColor_(j, "default_line_color");
    s.textHeightEstimate = j.at("text_height_estimate").get<float>();
    s.textVerticalCenterRatio = j.at("text_vertical_center_ratio").get<float>();
    s.textHorizontalPadding = j.at("text_horizontal_padding").get<float>();
    s.fontSize = j.at("font_size").get<unsigned int>();
    return s;
}

EndTurnButtonStyle_t ParseEndTurnButtonStyle_(const nlohmann::json& j)
{
    EndTurnButtonStyle_t s{};
    s.idleFillColor = ParseColor_(j, "idle_fill_color");
    s.readyFillColor = ParseColor_(j, "ready_fill_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.readyBorderColor = ParseColor_(j, "ready_border_color");
    s.labelColor = ParseColor_(j, "label_color");
    s.fontSize = j.at("font_size").get<unsigned int>();
    s.textPadX = j.at("text_pad_x").get<float>();
    s.textPadY = j.at("text_pad_y").get<float>();
    s.layout = ParseLayout_(j, "layout");
    return s;
}

CommlinksButtonStyle_t ParseCommlinksButtonStyle_(const nlohmann::json& j)
{
    CommlinksButtonStyle_t s{};
    s.fillColor = ParseColor_(j, "fill_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.labelColor = ParseColor_(j, "label_color");
    s.fontSize = j.at("font_size").get<unsigned int>();
    s.textPadX = j.at("text_pad_x").get<float>();
    s.textPadY = j.at("text_pad_y").get<float>();
    return s;
}

WorldViewStyle_t ParseWorldViewStyle_(const nlohmann::json& j)
{
    WorldViewStyle_t s{};
    s.researchTextColor = ParseColor_(j, "research_text_color");
    s.missionYearColor = ParseColor_(j, "mission_year_color");
    s.energyTextColor = ParseColor_(j, "energy_text_color");
    return s;
}

CombatViewStyle_t ParseCombatViewStyle_(const nlohmann::json& j)
{
    CombatViewStyle_t s{};
    s.sideNameColor = ParseColor_(j, "side_name_color");
    s.roundHeaderColor = ParseColor_(j, "round_header_color");
    s.idleLabelColor = ParseColor_(j, "idle_label_color");
    s.hpLineColor = ParseColor_(j, "hp_line_color");
    s.rollsLineColor = ParseColor_(j, "rolls_line_color");
    s.hitLineColor = ParseColor_(j, "hit_line_color");
    return s;
}

CombatPresentationStyle_t ParseCombatPresentationStyle_(const nlohmann::json& j)
{
    CombatPresentationStyle_t s{};
    s.defaultDamageFlashMs = j.at("default_damage_flash_ms").get<int>();
    s.defaultInterRoundDelayMs = j.at("default_inter_round_delay_ms").get<int>();
    return s;
}

CameraInputStyle_t ParseCameraInputStyle_(const nlohmann::json& j)
{
    CameraInputStyle_t s{};
    s.edgeZone = j.at("edge_zone").get<float>();
    s.relativeMin = j.at("relative_min").get<float>();
    s.relativeMax = j.at("relative_max").get<float>();
    s.cameraScrollStep = j.at("camera_scroll_step").get<int>();
    s.initialCameraOffset = j.at("initial_camera_offset").get<int>();
    s.edgeScrollSpeed = j.at("edge_scroll_speed").get<float>();
    return s;
}

UnitOrderInputStyle_t ParseUnitOrderInputStyle_(const nlohmann::json& j)
{
    UnitOrderInputStyle_t s{};
    s.holdThresholdMs = j.at("hold_threshold_ms").get<int>();
    return s;
}

CommlinksPanelStyle_t ParseCommlinksPanelStyle_(const nlohmann::json& j)
{
    CommlinksPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.titleColor = ParseColor_(j, "title_color");
    s.factionNameColor = ParseColor_(j, "faction_name_color");
    s.statusColor = ParseColor_(j, "status_color");
    s.titleFontSize = j.at("title_font_size").get<unsigned int>();
    s.rowFontSize = j.at("row_font_size").get<unsigned int>();
    s.titlePadX = j.at("title_pad_x").get<float>();
    s.titlePadY = j.at("title_pad_y").get<float>();
    s.rowStartY = j.at("row_start_y").get<float>();
    s.rowHeight = j.at("row_height").get<float>();
    s.rowPadX = j.at("row_pad_x").get<float>();
    s.statusPadX = j.at("status_pad_x").get<float>();
    s.councilButtonLayout = ParseLayout_(j, "council_button_layout");
    return s;
}

CouncilVoteViewStyle_t ParseCouncilVoteViewStyle_(const nlohmann::json& j)
{
    CouncilVoteViewStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.factionNameColor = ParseColor_(j, "faction_name_color");
    s.ballotColor = ParseColor_(j, "ballot_color");
    s.weightColor = ParseColor_(j, "weight_color");
    s.headerColor = ParseColor_(j, "header_color");
    s.nameColor = ParseColor_(j, "name_color");
    s.tallyColor = ParseColor_(j, "tally_color");
    s.factionFontSize = j.at("faction_font_size").get<unsigned int>();
    s.ballotFontSize = j.at("ballot_font_size").get<unsigned int>();
    s.weightFontSize = j.at("weight_font_size").get<unsigned int>();
    s.headerFontSize = j.at("header_font_size").get<unsigned int>();
    s.nameFontSize = j.at("name_font_size").get<unsigned int>();
    s.tallyFontSize = j.at("tally_font_size").get<unsigned int>();
    s.paddingRatio = j.at("padding_ratio").get<float>();
    s.lineHeightRatio = j.at("line_height_ratio").get<float>();
    s.voteButtonLayout = ParseLayout_(j, "vote_button_layout");
    return s;
}

CurrentResearchPanelStyle_t ParseCurrentResearchPanelStyle_(const nlohmann::json& j)
{
    CurrentResearchPanelStyle_t s{};
    s.labelLayout = ParseLayout_(j, "label_layout");
    s.targetLayout = ParseLayout_(j, "target_layout");
    s.progressLayout = ParseLayout_(j, "progress_layout");
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.labelColor = ParseColor_(j, "label_color");
    s.targetColor = ParseColor_(j, "target_color");
    s.progressColor = ParseColor_(j, "progress_color");
    s.labelFontSize = j.at("label_font_size").get<unsigned int>();
    s.targetFontSize = j.at("target_font_size").get<unsigned int>();
    s.progressFontSize = j.at("progress_font_size").get<unsigned int>();
    return s;
}

SettingsPanelStyle_t ParseSettingsPanelStyle_(const nlohmann::json& j)
{
    SettingsPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.titleColor = ParseColor_(j, "title_color");
    s.rowColor = ParseColor_(j, "row_color");
    s.titleFontSize = j.at("title_font_size").get<unsigned int>();
    s.rowFontSize = j.at("row_font_size").get<unsigned int>();
    s.titleLayout = ParseLayout_(j, "title_layout");
    s.rowLayout = ParseLayout_(j, "row_layout");
    return s;
}

BaseViewStyle_t ParseBaseViewStyle_(const nlohmann::json& j)
{
    BaseViewStyle_t s{};
    s.growthLayout = ParseLayout_(j, "growth_layout");
    s.workableLayout = ParseLayout_(j, "workable_layout");
    s.buildingsLayout = ParseLayout_(j, "buildings_layout");
    s.productionLayout = ParseLayout_(j, "production_layout");
    s.buildQueueLayout = ParseLayout_(j, "build_queue_layout");
    s.baseNameLayout = ParseLayout_(j, "base_name_layout");
    s.populationLayout = ParseLayout_(j, "population_layout");
    return s;
}

ResourceLinesPanelStyle_t ParseResourceLinesPanelStyle_(const nlohmann::json& j)
{
    ResourceLinesPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.textColor = ParseColor_(j, "text_color");
    s.headerFontSizeRatio = j.at("header_font_size_ratio").get<float>();
    s.entryFontSizeRatio = j.at("entry_font_size_ratio").get<float>();
    s.lineHeightRatio = j.at("line_height_ratio").get<float>();
    s.leftPaddingRatio = j.at("left_padding_ratio").get<float>();
    s.stockpileLineIndex = j.at("stockpile_line_index").get<float>();
    s.requiredLineIndex = j.at("required_line_index").get<float>();
    s.productionLineIndex = j.at("production_line_index").get<float>();
    return s;
}

PopulationDisplayStyle_t ParsePopulationDisplayStyle_(const nlohmann::json& j)
{
    PopulationDisplayStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.headerTextColor = ParseColor_(j, "header_text_color");
    s.headerFontSizeRatio = j.at("header_font_size_ratio").get<float>();
    s.popBoxSizeRatio = j.at("pop_box_size_ratio").get<float>();
    s.popBoxSpacingRatio = j.at("pop_box_spacing_ratio").get<float>();
    s.leftPaddingRatio = j.at("left_padding_ratio").get<float>();
    s.popBoxFontSizeRatio = j.at("pop_box_font_size_ratio").get<float>();
    s.popRowYOffsetRatio = j.at("pop_row_y_offset_ratio").get<float>();
    s.popBoxTextXOffsetRatio = j.at("pop_box_text_x_offset_ratio").get<float>();
    s.popBoxTextYOffsetRatio = j.at("pop_box_text_y_offset_ratio").get<float>();
    s.popBoxBorderWidth = j.at("pop_box_border_width").get<float>();
    s.popBoxFillColor = ParseColor_(j, "pop_box_fill_color");
    s.popBoxBorderColor = ParseColor_(j, "pop_box_border_color");
    s.popLetterColor = ParseColor_(j, "pop_letter_color");
    return s;
}

SupportDisplayStyle_t ParseSupportDisplayStyle_(const nlohmann::json& j)
{
    SupportDisplayStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.paddingRatio = j.at("padding_ratio").get<float>();
    s.iconSizeRatio = j.at("icon_size_ratio").get<float>();
    s.iconGapRatio = j.at("icon_gap_ratio").get<float>();
    return s;
}

BaseWorkableAreaDisplayStyle_t ParseBaseWorkableAreaDisplayStyle_(const nlohmann::json& j)
{
    BaseWorkableAreaDisplayStyle_t s{};
    s.gridDimension = j.at("grid_dimension").get<int>();
    s.gridCenterOffset = j.at("grid_center_offset").get<float>();
    s.backgroundColor = ParseColor_(j, "background_color");
    s.tileBorderColor = ParseColor_(j, "tile_border_color");
    s.tileBorderWidth = j.at("tile_border_width").get<float>();
    s.baseLabelFontSize = j.at("base_label_font_size").get<unsigned int>();
    s.tileFontSize = j.at("tile_font_size").get<unsigned int>();
    s.tileTextOffsetXRatio = j.at("tile_text_offset_x_ratio").get<float>();
    s.tileTextOffsetYRatio = j.at("tile_text_offset_y_ratio").get<float>();
    s.baseLabelColor = ParseColor_(j, "base_label_color");
    s.workedTileTextColor = ParseColor_(j, "worked_tile_text_color");
    s.unworkedTileTextColor = ParseColor_(j, "unworked_tile_text_color");
    s.unavailableTileTextColor = ParseColor_(j, "unavailable_tile_text_color");
    return s;
}

ListSelectorPopupStyle_t ParseListSelectorPopupStyle_(const nlohmann::json& j)
{
    ListSelectorPopupStyle_t s{};
    s.headerFontSizeRatio = j.at("header_font_size_ratio").get<float>();
    s.entryFontSizeRatio = j.at("entry_font_size_ratio").get<float>();
    s.lineHeightRatio = j.at("line_height_ratio").get<float>();
    s.paddingRatio = j.at("padding_ratio").get<float>();
    s.headerLineOffset = j.at("header_line_offset").get<float>();
    s.borderWidth = j.at("border_width").get<float>();
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.headerColor = ParseColor_(j, "header_color");
    s.hintColor = ParseColor_(j, "hint_color");
    s.entryColor = ParseColor_(j, "entry_color");
    return s;
}

SocialEngineeringDisplayStyle_t ParseSocialEngineeringDisplayStyle_(const nlohmann::json& j)
{
    SocialEngineeringDisplayStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.activePolicyColor = ParseColor_(j, "active_policy_color");
    s.inactivePolicyColor = ParseColor_(j, "inactive_policy_color");
    s.categoryColor = ParseColor_(j, "category_color");
    s.scoreHeaderColor = ParseColor_(j, "score_header_color");
    s.hiddenPolicySlotColor = ParseColor_(j, "hidden_policy_slot_color");
    s.scoreValueColor = ParseColor_(j, "score_value_color");
    s.factionBonusBodyColor = ParseColor_(j, "faction_bonus_body_color");
    s.scoresPanelWidthRatio = j.at("scores_panel_width_ratio").get<float>();
    s.categoryRowHeightRatio = j.at("category_row_height_ratio").get<float>();
    s.categoryNameRowRatio = j.at("category_name_row_ratio").get<float>();
    s.policyNameRowRatio = j.at("policy_name_row_ratio").get<float>();
    s.policyBonusRowRatio = j.at("policy_bonus_row_ratio").get<float>();
    s.horizontalPaddingRatio = j.at("horizontal_padding_ratio").get<float>();
    s.verticalPaddingRatio = j.at("vertical_padding_ratio").get<float>();
    s.headerFontSizeRatio = j.at("header_font_size_ratio").get<float>();
    s.categoryFontSizeRatio = j.at("category_font_size_ratio").get<float>();
    s.policyFontSizeRatio = j.at("policy_font_size_ratio").get<float>();
    s.bonusFontSizeRatio = j.at("bonus_font_size_ratio").get<float>();
    s.scoreFontSizeRatio = j.at("score_font_size_ratio").get<float>();
    s.factionBonusSectionRatio = j.at("faction_bonus_section_ratio").get<float>();
    s.factionNameRowRatio = j.at("faction_name_row_ratio").get<float>();
    s.factionBonusRowRatio = j.at("faction_bonus_row_ratio").get<float>();
    s.factionNameFontSizeRatio = j.at("faction_name_font_size_ratio").get<float>();
    s.factionBonusFontSizeRatio = j.at("faction_bonus_font_size_ratio").get<float>();
    return s;
}

SocialEngineeringBottomPanelStyle_t ParseSocialEngineeringBottomPanelStyle_(const nlohmann::json& j)
{
    SocialEngineeringBottomPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.valueColor = ParseColor_(j, "value_color");
    s.rowHeightRatio = j.at("row_height_ratio").get<float>();
    s.horizontalPaddingRatio = j.at("horizontal_padding_ratio").get<float>();
    s.verticalPaddingRatio = j.at("vertical_padding_ratio").get<float>();
    s.valueFontSizeRatio = j.at("value_font_size_ratio").get<float>();
    return s;
}

UnitDesignerViewStyle_t ParseUnitDesignerViewStyle_(const nlohmann::json& j)
{
    UnitDesignerViewStyle_t s{};
    s.leftSlotColumnLayout = ParseLayout_(j, "left_slot_column_layout");
    s.designStatsLayout = ParseLayout_(j, "design_stats_layout");
    s.rightSlotColumnLayout = ParseLayout_(j, "right_slot_column_layout");
    return s;
}

SlotColumnPanelStyle_t ParseSlotColumnPanelStyle_(const nlohmann::json& j)
{
    SlotColumnPanelStyle_t s{};
    s.visibleSlots = j.at("visible_slots").get<int>();
    s.arrowHeightRatio = j.at("arrow_height_ratio").get<float>();
    s.labelFontSizeRatio = j.at("label_font_size_ratio").get<float>();
    s.nameFontSizeRatio = j.at("name_font_size_ratio").get<float>();
    s.paddingRatio = j.at("padding_ratio").get<float>();
    s.arrowAreaMultiplier = j.at("arrow_area_multiplier").get<float>();
    s.arrowFontSizeRatio = j.at("arrow_font_size_ratio").get<float>();
    s.arrowPadXRatio = j.at("arrow_pad_x_ratio").get<float>();
    s.nameLabelSpacingMultiplier = j.at("name_label_spacing_multiplier").get<float>();
    s.arrowFillColor = ParseColor_(j, "arrow_fill_color");
    s.arrowBorderColor = ParseColor_(j, "arrow_border_color");
    s.disabledArrowColor = ParseColor_(j, "disabled_arrow_color");
    s.enabledArrowColor = ParseColor_(j, "enabled_arrow_color");
    s.slotFillColor = ParseColor_(j, "slot_fill_color");
    s.slotBorderColor = ParseColor_(j, "slot_border_color");
    s.labelTextColor = ParseColor_(j, "label_text_color");
    s.emptyNameColor = ParseColor_(j, "empty_name_color");
    s.filledNameColor = ParseColor_(j, "filled_name_color");
    return s;
}

DesignStatsDisplayStyle_t ParseDesignStatsDisplayStyle_(const nlohmann::json& j)
{
    DesignStatsDisplayStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.incompleteTextColor = ParseColor_(j, "incomplete_text_color");
    s.statTextColor = ParseColor_(j, "stat_text_color");
    s.saveButtonFillColor = ParseColor_(j, "save_button_fill_color");
    s.headerColor = ParseColor_(j, "header_color");
    s.saveButtonBorderColor = ParseColor_(j, "save_button_border_color");
    s.saveButtonTextColor = ParseColor_(j, "save_button_text_color");
    s.headerFontSizeRatio = j.at("header_font_size_ratio").get<float>();
    s.statFontSizeRatio = j.at("stat_font_size_ratio").get<float>();
    s.lineHeightRatio = j.at("line_height_ratio").get<float>();
    s.paddingRatio = j.at("padding_ratio").get<float>();
    s.saveButtonHeightRatio = j.at("save_button_height_ratio").get<float>();
    s.saveButtonTextYOffsetRatio = j.at("save_button_text_y_offset_ratio").get<float>();
    s.horizontalPaddingMultiplier = j.at("horizontal_padding_multiplier").get<float>();
    return s;
}

DesignListPanelStyle_t ParseDesignListPanelStyle_(const nlohmann::json& j)
{
    DesignListPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.emptyListTextColor = ParseColor_(j, "empty_list_text_color");
    s.selectedBoxFillColor = ParseColor_(j, "selected_box_fill_color");
    s.unselectedBoxFillColor = ParseColor_(j, "unselected_box_fill_color");
    s.unselectedBoxBorderColor = ParseColor_(j, "unselected_box_border_color");
    s.selectedBoxBorderColor = ParseColor_(j, "selected_box_border_color");
    s.boxWidthRatio = j.at("box_width_ratio").get<float>();
    s.boxPaddingRatio = j.at("box_padding_ratio").get<float>();
    s.fontSizeRatio = j.at("font_size_ratio").get<float>();
    s.labelFontRatio = j.at("label_font_ratio").get<float>();
    s.emptyListPaddingMultiplier = j.at("empty_list_padding_multiplier").get<float>();
    s.verticalPaddingMultiplier = j.at("vertical_padding_multiplier").get<float>();
    s.textPadRatio = j.at("text_pad_ratio").get<float>();
    s.textVerticalRatio = j.at("text_vertical_ratio").get<float>();
    return s;
}

UnitStatusPanelStyle_t ParseUnitStatusPanelStyle_(const nlohmann::json& j)
{
    UnitStatusPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.mutedTextColor = ParseColor_(j, "muted_text_color");
    s.headerColor = ParseColor_(j, "header_color");
    s.headerFontSizeRatio = j.at("header_font_size_ratio").get<float>();
    s.statFontSizeRatio = j.at("stat_font_size_ratio").get<float>();
    s.lineHeightRatio = j.at("line_height_ratio").get<float>();
    s.paddingRatio = j.at("padding_ratio").get<float>();
    s.designNameLineIndex = j.at("design_name_line_index").get<float>();
    s.activeCountLineIndex = j.at("active_count_line_index").get<float>();
    s.inProdLineIndex = j.at("in_prod_line_index").get<float>();
    return s;
}

SatelliteViewStyle_t ParseSatelliteViewStyle_(const nlohmann::json& j)
{
    SatelliteViewStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.headerColor = ParseColor_(j, "header_color");
    s.factionNameColor = ParseColor_(j, "faction_name_color");
    s.cellColor = ParseColor_(j, "cell_color");
    s.tabFillColor = ParseColor_(j, "tab_fill_color");
    s.tabSelectedFillColor = ParseColor_(j, "tab_selected_fill_color");
    s.tabBorderColor = ParseColor_(j, "tab_border_color");
    s.tabLabelColor = ParseColor_(j, "tab_label_color");
    s.tabFontSize = j.at("tab_font_size").get<unsigned int>();
    s.headerFontSize = j.at("header_font_size").get<unsigned int>();
    s.factionFontSize = j.at("faction_font_size").get<unsigned int>();
    s.cellFontSize = j.at("cell_font_size").get<unsigned int>();
    s.paddingRatio = j.at("padding_ratio").get<float>();
    s.tabTextPadX = j.at("tab_text_pad_x").get<float>();
    s.tabTextPadY = j.at("tab_text_pad_y").get<float>();
    s.summaryTabLayout = ParseLayout_(j, "summary_tab_layout");
    s.attackTabLayout = ParseLayout_(j, "attack_tab_layout");
    s.contentLayout = ParseLayout_(j, "content_layout");
    s.attackButtonLayout = ParseLayout_(j, "attack_button_layout");
    s.attackerListLayout = ParseLayout_(j, "attacker_list_layout");
    s.attackerConfirmLayout = ParseLayout_(j, "attacker_confirm_layout");
    s.attackerCancelLayout = ParseLayout_(j, "attacker_cancel_layout");
    s.outcomeOkLayout = ParseLayout_(j, "outcome_ok_layout");
    s.listButtonHeight = j.at("list_button_height").get<float>();
    s.listButtonGap = j.at("list_button_gap").get<float>();
    s.listHeaderHeight = j.at("list_header_height").get<float>();
    s.listPadX = j.at("list_pad_x").get<float>();
    s.listPadY = j.at("list_pad_y").get<float>();
    return s;
}

} // namespace

void UiStyle::Load(const std::string& filePath)
{
    std::cout << "Loading UI style from: " << filePath << "\n";

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open UI style file: " + filePath);
    }

    nlohmann::json root;
    file >> root;
    if (!root.is_object())
    {
        throw std::runtime_error("UI style root must be a JSON object");
    }

    UiStyle style{};
    style.layouts = ParseLayoutsStyle_(root.at("layouts"));
    style.viewFactory = ParseViewFactoryStyle_(root.at("view_factory"));
    style.tileRenderer = ParseTileRendererStyle_(root.at("tile_renderer"));
    style.worldDisplay = ParseWorldDisplayStyle_(root.at("world_display"));
    style.minimapDisplay = ParseMinimapDisplayStyle_(root.at("minimap_display"));
    style.unitMarker = ParseUnitMarkerStyle_(root.at("unit_marker"));
    style.selectedUnitPanel = ParseSelectedUnitPanelStyle_(root.at("selected_unit_panel"));
    style.locationPanel = ParseLocationPanelStyle_(root.at("location_panel"));
    style.unitStackPanel = ParseUnitStackPanelStyle_(root.at("unit_stack_panel"));
    style.infoPanel = ParseInfoPanelStyle_(root.at("info_panel"));
    style.endTurnButton = ParseEndTurnButtonStyle_(root.at("end_turn_button"));
    style.commlinksButton = ParseCommlinksButtonStyle_(root.at("commlinks_button"));
    style.worldView = ParseWorldViewStyle_(root.at("world_view"));
    style.combatView = ParseCombatViewStyle_(root.at("combat_view"));
    style.combatPresentation = ParseCombatPresentationStyle_(root.at("combat_presentation"));
    style.cameraInput = ParseCameraInputStyle_(root.at("camera_input"));
    style.unitOrderInput = ParseUnitOrderInputStyle_(root.at("unit_order_input"));
    style.commlinksPanel = ParseCommlinksPanelStyle_(root.at("commlinks_panel"));
    style.councilVoteView = ParseCouncilVoteViewStyle_(root.at("council_vote_view"));
    style.currentResearchPanel = ParseCurrentResearchPanelStyle_(root.at("current_research_panel"));
    style.settingsPanel = ParseSettingsPanelStyle_(root.at("settings_panel"));
    style.baseView = ParseBaseViewStyle_(root.at("base_view"));
    style.growthDisplay = ParseResourceLinesPanelStyle_(root.at("growth_display"));
    style.productionDisplay = ParseResourceLinesPanelStyle_(root.at("production_display"));
    style.populationDisplay = ParsePopulationDisplayStyle_(root.at("population_display"));
    style.supportDisplay = ParseSupportDisplayStyle_(root.at("support_display"));
    style.baseWorkableAreaDisplay = ParseBaseWorkableAreaDisplayStyle_(root.at("base_workable_area_display"));
    style.listSelectorPopup = ParseListSelectorPopupStyle_(root.at("list_selector_popup"));
    style.socialEngineeringDisplay = ParseSocialEngineeringDisplayStyle_(root.at("social_engineering_display"));
    style.socialEngineeringBottomPanel = ParseSocialEngineeringBottomPanelStyle_(root.at("social_engineering_bottom_panel"));
    style.unitDesignerView = ParseUnitDesignerViewStyle_(root.at("unit_designer_view"));
    style.slotColumnPanel = ParseSlotColumnPanelStyle_(root.at("slot_column_panel"));
    style.componentSelectorPopup = ParseListSelectorPopupStyle_(root.at("component_selector_popup"));
    style.designStatsDisplay = ParseDesignStatsDisplayStyle_(root.at("design_stats_display"));
    style.designListPanel = ParseDesignListPanelStyle_(root.at("design_list_panel"));
    style.unitStatusPanel = ParseUnitStatusPanelStyle_(root.at("unit_status_panel"));
    style.satelliteView = ParseSatelliteViewStyle_(root.at("satellite_view"));

    g_style = style;
    g_loaded = true;
    std::cout << "Loaded UI style\n";
}

const UiStyle& UiStyle::Get()
{
    if (!g_loaded)
    {
        throw std::runtime_error("UiStyle::Get called before UiStyle::Load");
    }
    return g_style;
}

} // namespace ac
