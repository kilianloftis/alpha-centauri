#include "ui/style/UiStyle.h"

#include "lib/config/ConfigReader.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

namespace
{

UiStyle g_style{};
bool g_loaded = false;

constexpr const char* k_Context = "UI style";

// Float fields take a number or a fraction string ("1/3"), as every config number does.
float ReadFloat_(const nlohmann::json& j, const char* key)
{
    return ConfigReader(j, k_Context).Float(key);
}

float FloatAt_(const nlohmann::json& arr, std::size_t index)
{
    return static_cast<float>(ConfigReader::NumberFromJson(arr.at(index)));
}

Color_t ParseColorValue_(const nlohmann::json& arr, const std::string& key)
{
    if (!arr.is_array() || arr.size() < 3 || arr.size() > 4)
    {
        throw std::runtime_error("Expected an RGB or RGBA array for '" + key
                                 + "'; extra entries are a typo, not optional data");
    }
    const uint8_t a = arr.size() >= 4 ? arr.at(3).get<uint8_t>() : 255;
    return Color_t{
        arr.at(0).get<uint8_t>(),
        arr.at(1).get<uint8_t>(),
        arr.at(2).get<uint8_t>(),
        a};
}

Color_t ParseColor_(const nlohmann::json& j, const char* key)
{
    return ParseColorValue_(j.at(key), key);
}

RatioLayout_t ParseLayout_(const nlohmann::json& j, const char* key)
{
    const auto& arr = j.at(key);
    if (!arr.is_array() || arr.size() != 4)
    {
        throw std::runtime_error(std::string("Expected [x,y,w,h] layout for '") + key + "'");
    }
    return RatioLayout_t{
        FloatAt_(arr, 0),
        FloatAt_(arr, 1),
        FloatAt_(arr, 2),
        FloatAt_(arr, 3)};
}

FixedLayout_t ParseFixedLayout_(const nlohmann::json& j, const char* key)
{
    const auto& obj = j.at(key);
    if (!obj.is_object())
    {
        throw std::runtime_error(std::string("Expected fixed layout object for '") + key + "'");
    }

    const auto& align = obj.at("align");
    if (!align.is_array() || align.size() != 2)
    {
        throw std::runtime_error(std::string("Expected [x,y] align for '") + key + "'");
    }
    const float alignX = FloatAt_(align, 0);
    const float alignY = FloatAt_(align, 1);
    if (alignX < 0.0f || alignX > 1.0f || alignY < 0.0f || alignY > 1.0f)
    {
        throw std::runtime_error(std::string("align for '") + key + "' must be in [0, 1]");
    }

    const auto& variants = obj.at("variants");
    if (!variants.is_array() || variants.empty())
    {
        throw std::runtime_error(std::string("variants for '") + key + "' must be a non-empty array");
    }

    FixedLayout_t s{};
    s.alignX = alignX;
    s.alignY = alignY;
    for (const auto& rVariant : variants)
    {
        FixedLayoutVariant_t variant{};
        variant.width = ReadFloat_(rVariant, "width");
        variant.height = ReadFloat_(rVariant, "height");
        if (variant.width <= 0.0f || variant.height <= 0.0f)
        {
            throw std::runtime_error(std::string("variant size for '") + key
                                     + "' must be positive");
        }
        variant.mapOverlap = ReadFloat_(rVariant, "map_overlap");
        if (variant.mapOverlap < 0.0f || variant.mapOverlap > variant.height)
        {
            throw std::runtime_error(std::string("map_overlap for '") + key
                                     + "' must be in [0, height]");
        }
        variant.spriteOffsetX = ConfigReader(rVariant, k_Context).FloatOr("sprite_offset_x", 0.0f);
        variant.sprite = rVariant.at("sprite").get<std::string>();
        s.variants.push_back(std::move(variant));
    }
    return s;
}

LayoutsStyle_t ParseLayoutsStyle_(const nlohmann::json& j)
{
    LayoutsStyle_t s{};
    s.fullscreen = ParseLayout_(j, "fullscreen");
    s.console = ParseFixedLayout_(j, "console");
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
    s.fullscreenOriginX = ReadFloat_(j, "fullscreen_origin_x");
    s.fullscreenOriginY = ReadFloat_(j, "fullscreen_origin_y");
    return s;
}

WaterShadingStyle_t ParseWaterShadingStyle_(const nlohmann::json& j)
{
    WaterShadingStyle_t s{};
    s.depthShades = j.at("depth_shades").get<std::vector<int>>();
    if (s.depthShades.empty())
    {
        throw std::runtime_error("tile_renderer.water_shading.depth_shades must not be empty");
    }
    for (const int shade : s.depthShades)
    {
        if (shade < 0)
        {
            throw std::runtime_error(
                "tile_renderer.water_shading.depth_shades must not be negative");
        }
    }
    s.coastShades = j.at("coast_shades").get<std::string>();
    s.detailMeters = ReadFloat_(j, "detail_meters");
    if (s.detailMeters <= 0.0f)
    {
        throw std::runtime_error("tile_renderer.water_shading.detail_meters must be positive");
    }
    s.deepLandform = j.at("deep_landform").get<std::string>();
    s.shelfLandform = j.at("shelf_landform").get<std::string>();
    s.deepFromShade = j.at("deep_from_shade").get<int>();
    if (s.deepFromShade < 0)
    {
        throw std::runtime_error("tile_renderer.water_shading.deep_from_shade must not be negative");
    }
    return s;
}

ReliefStyle_t ParseReliefStyle_(const nlohmann::json& j)
{
    ReliefStyle_t s{};
    s.liftPerLevelRatio = ReadFloat_(j, "lift_per_level_ratio");
    s.levelMeters = ReadFloat_(j, "level_meters");
    s.fullShadeRiseMeters = ReadFloat_(j, "full_shade_rise_meters");
    s.altitudeLightSteps = ReadFloat_(j, "altitude_light_steps");
    if (s.liftPerLevelRatio <= 0.0f || s.levelMeters <= 0.0f || s.fullShadeRiseMeters <= 0.0f)
    {
        throw std::runtime_error(
            "tile_renderer.relief: lift_per_level_ratio, level_meters and full_shade_rise_meters "
            "must be positive");
    }
    if (s.altitudeLightSteps < 0.0f)
    {
        throw std::runtime_error("tile_renderer.relief: altitude_light_steps must not be negative");
    }
    return s;
}

TileRendererStyle_t ParseTileRendererStyle_(const nlohmann::json& j)
{
    TileRendererStyle_t s{};
    s.waterLowColor = ParseColor_(j, "water_low_color");
    s.waterHighColor = ParseColor_(j, "water_high_color");
    s.landLowColor = ParseColor_(j, "land_low_color");
    s.landHighColor = ParseColor_(j, "land_high_color");
    s.fogFillDimRatio = ReadFloat_(j, "fog_fill_dim_ratio");
    s.fogLandShade = ReadFloat_(j, "fog_land_shade");
    if (s.fogLandShade < 0.0f)
    {
        throw std::runtime_error("tile_renderer.fog_land_shade must not be negative");
    }
    s.fogHazeColor = ParseColor_(j, "fog_haze_color");
    s.coastSpriteDir = j.at("coast_sprite_dir").get<std::string>();
    if (s.coastSpriteDir.empty())
    {
        throw std::runtime_error("tile_renderer.coast_sprite_dir must not be empty");
    }
    s.palettePath = j.at("palette_path").get<std::string>();
    if (s.palettePath.empty())
    {
        throw std::runtime_error("tile_renderer.palette_path must not be empty");
    }
    s.waterShading = ParseWaterShadingStyle_(j.at("water_shading"));
    s.relief = ParseReliefStyle_(j.at("relief"));
    s.missingArtColor = ParseColor_(j, "missing_art_color");
    s.missingArtAltColor = ParseColor_(j, "missing_art_alt_color");
    s.missingArtSizeRatio = ReadFloat_(j, "missing_art_size_ratio");
    if (s.missingArtSizeRatio <= 0.0f)
    {
        throw std::runtime_error("tile_renderer.missing_art_size_ratio must be positive");
    }
    s.shroudColor = ParseColor_(j, "shroud_color");
    return s;
}

MapRendererStyle_t ParseMapRendererStyle_(const nlohmann::json& j)
{
    MapRendererStyle_t s{};
    s.gridLandColor = ParseColor_(j, "grid_land_color");
    s.gridWaterColor = ParseColor_(j, "grid_water_color");
    s.gridLineWidth = ReadFloat_(j, "grid_line_width");
    if (s.gridLineWidth <= 0.0f)
    {
        throw std::runtime_error("map_renderer.grid_line_width must be positive");
    }
    s.baseSpriteOverhangRatio = ReadFloat_(j, "base_sprite_overhang_ratio");
    if (!(s.baseSpriteOverhangRatio >= 0.0f))
    {
        throw std::runtime_error("map_renderer.base_sprite_overhang_ratio must be >= 0");
    }
    s.baseNameFontSizeRatio = ReadFloat_(j, "base_name_font_size_ratio");
    s.baseNameOffsetXRatio = ReadFloat_(j, "base_name_offset_x_ratio");
    s.baseNameOffsetYRatio = ReadFloat_(j, "base_name_offset_y_ratio");
    s.baseNameWidthRatio = ReadFloat_(j, "base_name_width_ratio");
    s.baseNameColor = ParseColor_(j, "base_name_color");
    return s;
}

WorldDisplayStyle_t ParseWorldDisplayStyle_(const nlohmann::json& j)
{
    WorldDisplayStyle_t s{};
    s.defaultTileScale = ReadFloat_(j, "default_tile_scale");
    s.pathPreviewColor = ParseColor_(j, "path_preview_color");
    s.pathPreviewLineThicknessRatio = ReadFloat_(j, "path_preview_line_thickness_ratio");
    s.airdropCursorPath = j.at("airdrop_cursor_path").get<std::string>();
    const auto& hotspot = j.at("airdrop_cursor_hotspot");
    if (!hotspot.is_array() || hotspot.size() != 2)
    {
        throw std::runtime_error("world_display.airdrop_cursor_hotspot must be [x, y]");
    }
    s.airdropCursorHotspotX = hotspot.at(0).get<unsigned int>();
    s.airdropCursorHotspotY = hotspot.at(1).get<unsigned int>();
    s.bombardCursorPath = j.at("bombard_cursor_path").get<std::string>();
    const auto& bombardHotspot = j.at("bombard_cursor_hotspot");
    if (!bombardHotspot.is_array() || bombardHotspot.size() != 2)
    {
        throw std::runtime_error("world_display.bombard_cursor_hotspot must be [x, y]");
    }
    s.bombardCursorHotspotX = bombardHotspot.at(0).get<unsigned int>();
    s.bombardCursorHotspotY = bombardHotspot.at(1).get<unsigned int>();
    return s;
}

MinimapDisplayStyle_t ParseMinimapDisplayStyle_(const nlohmann::json& j)
{
    MinimapDisplayStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.viewportBorderColor = ParseColor_(j, "viewport_border_color");
    s.viewportBorderWidth = ReadFloat_(j, "viewport_border_width");
    return s;
}

UnitMarkerStyle_t ParseUnitMarkerStyle_(const nlohmann::json& j)
{
    UnitMarkerStyle_t s{};
    s.fontSizeRatio = ReadFloat_(j, "font_size_ratio");
    s.widthRatio = ReadFloat_(j, "width_ratio");
    s.heightRatio = ReadFloat_(j, "height_ratio");
    s.spacingRatio = ReadFloat_(j, "spacing_ratio");
    s.markerColor = ParseColor_(j, "marker_color");
    s.exhaustedColor = ParseColor_(j, "exhausted_color");
    s.selectionBorderOffset = ReadFloat_(j, "selection_border_offset");
    s.selectionBorderExpansion = ReadFloat_(j, "selection_border_expansion");
    s.selectionBorderWidth = ReadFloat_(j, "selection_border_width");
    s.selectionBorderColor = ParseColor_(j, "selection_border_color");
    s.hitOverlayFill = ParseColor_(j, "hit_overlay_fill");
    s.hitOverlayBorder = ParseColor_(j, "hit_overlay_border");
    s.hitOverlayBorderWidth = ReadFloat_(j, "hit_overlay_border_width");
    s.initialTextColor = ParseColor_(j, "initial_text_color");
    return s;
}

SelectedUnitPanelStyle_t ParseSelectedUnitPanelStyle_(const nlohmann::json& j)
{
    SelectedUnitPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.backgroundSprite = j.value("background_sprite", "");
    s.mutedTextColor = ParseColor_(j, "muted_text_color");
    s.iconColor = ParseColor_(j, "icon_color");
    s.iconExhaustedColor = ParseColor_(j, "icon_exhausted_color");
    s.bodyTextColor = ParseColor_(j, "body_text_color");
    s.iconInitialTextColor = ParseColor_(j, "icon_initial_text_color");
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.iconSizeRatio = ReadFloat_(j, "icon_size_ratio");
    s.iconInitialFontRatio = ReadFloat_(j, "icon_initial_font_ratio");
    s.textFontRatio = ReadFloat_(j, "text_font_ratio");
    s.textGapRatio = ReadFloat_(j, "text_gap_ratio");
    s.iconInitialOffsetXRatio = ReadFloat_(j, "icon_initial_offset_x_ratio");
    s.iconInitialOffsetYRatio = ReadFloat_(j, "icon_initial_offset_y_ratio");
    s.iconCenterRatio = ReadFloat_(j, "icon_center_ratio");
    return s;
}

LocationPanelStyle_t ParseLocationPanelStyle_(const nlohmann::json& j)
{
    LocationPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.backgroundSprite = j.value("background_sprite", "");
    s.mutedTextColor = ParseColor_(j, "muted_text_color");
    s.bodyTextColor = ParseColor_(j, "body_text_color");
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.previewSizeRatio = ReadFloat_(j, "preview_size_ratio");
    s.textFontRatio = ReadFloat_(j, "text_font_ratio");
    s.textGapRatio = ReadFloat_(j, "text_gap_ratio");
    return s;
}

UnitStackPanelStyle_t ParseUnitStackPanelStyle_(const nlohmann::json& j)
{
    UnitStackPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.backgroundSprite = j.value("background_sprite", "");
    s.statTextColor = ParseColor_(j, "stat_text_color");
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.slotGapRatio = ReadFloat_(j, "slot_gap_ratio");
    s.iconHeightRatio = ReadFloat_(j, "icon_height_ratio");
    s.statFontRatio = ReadFloat_(j, "stat_font_ratio");
    return s;
}

InfoPanelStyle_t ParseInfoPanelStyle_(const nlohmann::json& j)
{
    InfoPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.backgroundSprite = j.value("background_sprite", "");
    s.defaultLineColor = ParseColor_(j, "default_line_color");
    s.textHeightEstimate = ReadFloat_(j, "text_height_estimate");
    s.textVerticalCenterRatio = ReadFloat_(j, "text_vertical_center_ratio");
    s.textHorizontalPadding = ReadFloat_(j, "text_horizontal_padding");
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
    s.textPadX = ReadFloat_(j, "text_pad_x");
    s.textPadY = ReadFloat_(j, "text_pad_y");
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
    s.textPadX = ReadFloat_(j, "text_pad_x");
    s.textPadY = ReadFloat_(j, "text_pad_y");
    return s;
}

WorldViewStyle_t ParseWorldViewStyle_(const nlohmann::json& j)
{
    WorldViewStyle_t s{};
    s.researchTextColor = ParseColor_(j, "research_text_color");
    s.missionYearColor = ParseColor_(j, "mission_year_color");
    s.energyTextColor = ParseColor_(j, "energy_text_color");
    s.consoleBackdropColor = ParseColor_(j, "console_backdrop_color");
    const auto& consoleLayouts = j.at("console_layouts");
    s.consoleUnit = ParseLayout_(consoleLayouts, "unit");
    s.consoleLocation = ParseLayout_(consoleLayouts, "location");
    s.consoleInfo = ParseLayout_(consoleLayouts, "info");
    s.consoleStack = ParseLayout_(consoleLayouts, "stack");
    s.consoleMinimap = ParseLayout_(consoleLayouts, "minimap");
    s.consoleCommlinks = ParseLayout_(consoleLayouts, "commlinks");
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

BombardPresentationStyle_t ParseBombardPresentationStyle_(const nlohmann::json& j)
{
    BombardPresentationStyle_t s{};
    s.overlayFill = ParseColor_(j, "overlay_fill");
    s.overlayBorder = ParseColor_(j, "overlay_border");
    s.overlayBorderWidth = ReadFloat_(j, "overlay_border_width");
    return s;
}

CameraInputStyle_t ParseCameraInputStyle_(const nlohmann::json& j)
{
    CameraInputStyle_t s{};
    s.edgeZone = ReadFloat_(j, "edge_zone");
    s.relativeMin = ReadFloat_(j, "relative_min");
    s.relativeMax = ReadFloat_(j, "relative_max");
    s.cameraScrollStep = j.at("camera_scroll_step").get<int>();
    s.initialCameraOffset = j.at("initial_camera_offset").get<int>();
    s.edgeScrollSpeed = ReadFloat_(j, "edge_scroll_speed");
    s.zoomFactor = ReadFloat_(j, "zoom_factor");
    s.minTileScale = ReadFloat_(j, "min_tile_scale");
    s.maxTileScale = ReadFloat_(j, "max_tile_scale");
    if (!(s.zoomFactor > 1.0f))
    {
        throw std::runtime_error("camera_input.zoom_factor must be greater than 1");
    }
    if (!(s.minTileScale > 0.0f) || !(s.maxTileScale > s.minTileScale))
    {
        throw std::runtime_error(
            "camera_input requires 0 < min_tile_scale < max_tile_scale");
    }
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
    s.titlePadX = ReadFloat_(j, "title_pad_x");
    s.titlePadY = ReadFloat_(j, "title_pad_y");
    s.rowStartY = ReadFloat_(j, "row_start_y");
    s.rowHeight = ReadFloat_(j, "row_height");
    s.rowPadX = ReadFloat_(j, "row_pad_x");
    s.statusPadX = ReadFloat_(j, "status_pad_x");
    s.integrityPadY = ReadFloat_(j, "integrity_pad_y");
    s.columnLabelPadY = ReadFloat_(j, "column_label_pad_y");
    s.moodPadX = ReadFloat_(j, "mood_pad_x");
    s.reputePadX = ReadFloat_(j, "repute_pad_x");
    s.councilButtonLayout = ParseLayout_(j, "council_button_layout");
    return s;
}

TalkPopupStyle_t ParseTalkPopupStyle_(const nlohmann::json& j)
{
    TalkPopupStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.captionColor = ParseColor_(j, "caption_color");
    s.labelColor = ParseColor_(j, "label_color");
    s.textColor = ParseColor_(j, "text_color");
    s.highlightColor = ParseColor_(j, "highlight_color");
    s.captionFontSize = j.at("caption_font_size").get<unsigned int>();
    s.textFontSize = j.at("text_font_size").get<unsigned int>();
    s.padX = ReadFloat_(j, "pad_x");
    s.valuePadX = ReadFloat_(j, "value_pad_x");
    s.captionPadY = ReadFloat_(j, "caption_pad_y");
    s.moodPadY = ReadFloat_(j, "mood_pad_y");
    s.reputePadY = ReadFloat_(j, "repute_pad_y");
    s.integrityPadY = ReadFloat_(j, "integrity_pad_y");
    s.speechPadY = ReadFloat_(j, "speech_pad_y");
    s.lineHeight = ReadFloat_(j, "line_height");
    s.draftPadY = ReadFloat_(j, "draft_pad_y");
    s.menuLayout = ParseLayout_(j, "menu_layout");
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
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.lineHeightRatio = ReadFloat_(j, "line_height_ratio");
    s.voteButtonLayout = ParseLayout_(j, "vote_button_layout");
    return s;
}

CurrentResearchPanelStyle_t ParseCurrentResearchPanelStyle_(const nlohmann::json& j)
{
    CurrentResearchPanelStyle_t s{};
    s.labelLayout = ParseLayout_(j, "label_layout");
    s.targetLayout = ParseLayout_(j, "target_layout");
    s.progressLayout = ParseLayout_(j, "progress_layout");
    s.iconLayout = ParseLayout_(j, "icon_layout");
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.backgroundSprite = j.value("background_sprite", "");
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
    s.commerceLayout = ParseLayout_(j, "commerce_layout");
    s.workableLayout = ParseLayout_(j, "workable_layout");
    s.buildingsLayout = ParseLayout_(j, "buildings_layout");
    s.productionLayout = ParseLayout_(j, "production_layout");
    s.buildQueueLayout = ParseLayout_(j, "build_queue_layout");
    s.baseNameLayout = ParseLayout_(j, "base_name_layout");
    s.populationLayout = ParseLayout_(j, "population_layout");
    return s;
}

BuildQueueDisplayStyle_t ParseBuildQueueDisplayStyle_(const nlohmann::json& j)
{
    BuildQueueDisplayStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.textColor = ParseColor_(j, "text_color");
    s.hurryFillColor = ParseColor_(j, "hurry_fill_color");
    s.hurryDisabledFillColor = ParseColor_(j, "hurry_disabled_fill_color");
    s.hurryBorderColor = ParseColor_(j, "hurry_border_color");
    s.hurryLabelColor = ParseColor_(j, "hurry_label_color");
    s.hurryDisabledLabelColor = ParseColor_(j, "hurry_disabled_label_color");
    s.headerFontSizeRatio = ReadFloat_(j, "header_font_size_ratio");
    s.leftPaddingRatio = ReadFloat_(j, "left_padding_ratio");
    s.hurryFontSizeRatio = ReadFloat_(j, "hurry_font_size_ratio");
    s.hurryTextPadXRatio = ReadFloat_(j, "hurry_text_pad_x_ratio");
    s.hurryTextPadYRatio = ReadFloat_(j, "hurry_text_pad_y_ratio");
    s.hurryButtonLayout = ParseLayout_(j, "hurry_button_layout");
    return s;
}

HurryProductionPopupStyle_t ParseHurryProductionPopupStyle_(const nlohmann::json& j)
{
    HurryProductionPopupStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.headerColor = ParseColor_(j, "header_color");
    s.messageColor = ParseColor_(j, "message_color");
    s.fieldFillColor = ParseColor_(j, "field_fill_color");
    s.fieldBorderColor = ParseColor_(j, "field_border_color");
    s.fieldTextColor = ParseColor_(j, "field_text_color");
    s.buttonFillColor = ParseColor_(j, "button_fill_color");
    s.buttonBorderColor = ParseColor_(j, "button_border_color");
    s.buttonLabelColor = ParseColor_(j, "button_label_color");
    s.headerFontSizeRatio = ReadFloat_(j, "header_font_size_ratio");
    s.entryFontSizeRatio = ReadFloat_(j, "entry_font_size_ratio");
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.buttonFontSizeRatio = ReadFloat_(j, "button_font_size_ratio");
    s.buttonTextPadXRatio = ReadFloat_(j, "button_text_pad_x_ratio");
    s.buttonTextPadYRatio = ReadFloat_(j, "button_text_pad_y_ratio");
    s.fieldLayout = ParseLayout_(j, "field_layout");
    s.okButtonLayout = ParseLayout_(j, "ok_button_layout");
    s.cancelButtonLayout = ParseLayout_(j, "cancel_button_layout");
    return s;
}

ResourceLinesPanelStyle_t ParseResourceLinesPanelStyle_(const nlohmann::json& j)
{
    ResourceLinesPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.backgroundSprite = j.value("background_sprite", "");
    s.textColor = ParseColor_(j, "text_color");
    s.headerFontSizeRatio = ReadFloat_(j, "header_font_size_ratio");
    s.entryFontSizeRatio = ReadFloat_(j, "entry_font_size_ratio");
    s.lineHeightRatio = ReadFloat_(j, "line_height_ratio");
    s.leftPaddingRatio = ReadFloat_(j, "left_padding_ratio");
    s.stockpileLineIndex = ReadFloat_(j, "stockpile_line_index");
    s.requiredLineIndex = ReadFloat_(j, "required_line_index");
    s.productionLineIndex = ReadFloat_(j, "production_line_index");
    s.turnsLineIndex = ReadFloat_(j, "turns_line_index");
    s.iconSizeRatio = ReadFloat_(j, "icon_size_ratio");
    s.iconGapRatio = ReadFloat_(j, "icon_gap_ratio");
    return s;
}

PopulationDisplayStyle_t ParsePopulationDisplayStyle_(const nlohmann::json& j)
{
    PopulationDisplayStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.headerTextColor = ParseColor_(j, "header_text_color");
    s.headerFontSizeRatio = ReadFloat_(j, "header_font_size_ratio");
    s.popBoxSizeRatio = ReadFloat_(j, "pop_box_size_ratio");
    s.popBoxSpacingRatio = ReadFloat_(j, "pop_box_spacing_ratio");
    s.leftPaddingRatio = ReadFloat_(j, "left_padding_ratio");
    s.popBoxFontSizeRatio = ReadFloat_(j, "pop_box_font_size_ratio");
    s.popRowYOffsetRatio = ReadFloat_(j, "pop_row_y_offset_ratio");
    s.popBoxTextXOffsetRatio = ReadFloat_(j, "pop_box_text_x_offset_ratio");
    s.popBoxTextYOffsetRatio = ReadFloat_(j, "pop_box_text_y_offset_ratio");
    s.popBoxBorderWidth = ReadFloat_(j, "pop_box_border_width");
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
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.iconSizeRatio = ReadFloat_(j, "icon_size_ratio");
    s.iconGapRatio = ReadFloat_(j, "icon_gap_ratio");
    return s;
}

BuildingsDisplayStyle_t ParseBuildingsDisplayStyle_(const nlohmann::json& j)
{
    BuildingsDisplayStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.backgroundSprite = j.value("background_sprite", "");
    s.textColor = ParseColor_(j, "text_color");
    s.grantedTextColor = ParseColor_(j, "granted_text_color");
    s.headerFontSizeRatio = ReadFloat_(j, "header_font_size_ratio");
    s.entryFontSizeRatio = ReadFloat_(j, "entry_font_size_ratio");
    s.lineHeightRatio = ReadFloat_(j, "line_height_ratio");
    s.leftPaddingRatio = ReadFloat_(j, "left_padding_ratio");
    s.headerLineOffset = ReadFloat_(j, "header_line_offset");
    s.iconSizeRatio = ReadFloat_(j, "icon_size_ratio");
    s.iconGapRatio = ReadFloat_(j, "icon_gap_ratio");
    return s;
}

BaseWorkableAreaDisplayStyle_t ParseBaseWorkableAreaDisplayStyle_(const nlohmann::json& j)
{
    BaseWorkableAreaDisplayStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.tileFontSize = j.at("tile_font_size").get<unsigned int>();
    s.workedTileTextColor = ParseColor_(j, "worked_tile_text_color");
    s.unworkedTileTextColor = ParseColor_(j, "unworked_tile_text_color");
    s.unavailableTileTextColor = ParseColor_(j, "unavailable_tile_text_color");
    return s;
}

NoticePopupStyle_t ParseNoticePopupStyle_(const nlohmann::json& j)
{
    NoticePopupStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.backgroundSprite = j.value("background_sprite", "");
    s.headerColor = ParseColor_(j, "header_color");
    s.messageColor = ParseColor_(j, "message_color");
    s.okButtonLayout = ParseLayout_(j, "ok_button_layout");
    return s;
}

ListSelectorPopupStyle_t ParseListSelectorPopupStyle_(const nlohmann::json& j)
{
    ListSelectorPopupStyle_t s{};
    s.headerFontSizeRatio = ReadFloat_(j, "header_font_size_ratio");
    s.entryFontSizeRatio = ReadFloat_(j, "entry_font_size_ratio");
    s.lineHeightRatio = ReadFloat_(j, "line_height_ratio");
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.headerLineOffset = ReadFloat_(j, "header_line_offset");
    s.borderWidth = ReadFloat_(j, "border_width");
    s.iconSizeRatio = ReadFloat_(j, "icon_size_ratio");
    s.iconGapRatio = ReadFloat_(j, "icon_gap_ratio");
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.backgroundSprite = j.value("background_sprite", "");
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
    s.scoresPanelWidthRatio = ReadFloat_(j, "scores_panel_width_ratio");
    s.categoryRowHeightRatio = ReadFloat_(j, "category_row_height_ratio");
    s.categoryNameRowRatio = ReadFloat_(j, "category_name_row_ratio");
    s.policyNameRowRatio = ReadFloat_(j, "policy_name_row_ratio");
    s.policyBonusRowRatio = ReadFloat_(j, "policy_bonus_row_ratio");
    s.horizontalPaddingRatio = ReadFloat_(j, "horizontal_padding_ratio");
    s.verticalPaddingRatio = ReadFloat_(j, "vertical_padding_ratio");
    s.headerFontSizeRatio = ReadFloat_(j, "header_font_size_ratio");
    s.categoryFontSizeRatio = ReadFloat_(j, "category_font_size_ratio");
    s.policyFontSizeRatio = ReadFloat_(j, "policy_font_size_ratio");
    s.bonusFontSizeRatio = ReadFloat_(j, "bonus_font_size_ratio");
    s.scoreFontSizeRatio = ReadFloat_(j, "score_font_size_ratio");
    s.factionBonusSectionRatio = ReadFloat_(j, "faction_bonus_section_ratio");
    s.factionNameRowRatio = ReadFloat_(j, "faction_name_row_ratio");
    s.factionBonusRowRatio = ReadFloat_(j, "faction_bonus_row_ratio");
    s.factionNameFontSizeRatio = ReadFloat_(j, "faction_name_font_size_ratio");
    s.factionBonusFontSizeRatio = ReadFloat_(j, "faction_bonus_font_size_ratio");
    return s;
}

SocialEngineeringBottomPanelStyle_t ParseSocialEngineeringBottomPanelStyle_(const nlohmann::json& j)
{
    SocialEngineeringBottomPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.valueColor = ParseColor_(j, "value_color");
    s.rowHeightRatio = ReadFloat_(j, "row_height_ratio");
    s.horizontalPaddingRatio = ReadFloat_(j, "horizontal_padding_ratio");
    s.verticalPaddingRatio = ReadFloat_(j, "vertical_padding_ratio");
    s.valueFontSizeRatio = ReadFloat_(j, "value_font_size_ratio");
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
    s.arrowHeightRatio = ReadFloat_(j, "arrow_height_ratio");
    s.labelFontSizeRatio = ReadFloat_(j, "label_font_size_ratio");
    s.nameFontSizeRatio = ReadFloat_(j, "name_font_size_ratio");
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.arrowAreaMultiplier = ReadFloat_(j, "arrow_area_multiplier");
    s.arrowFontSizeRatio = ReadFloat_(j, "arrow_font_size_ratio");
    s.arrowPadXRatio = ReadFloat_(j, "arrow_pad_x_ratio");
    s.nameLabelSpacingMultiplier = ReadFloat_(j, "name_label_spacing_multiplier");
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
    s.headerFontSizeRatio = ReadFloat_(j, "header_font_size_ratio");
    s.statFontSizeRatio = ReadFloat_(j, "stat_font_size_ratio");
    s.lineHeightRatio = ReadFloat_(j, "line_height_ratio");
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.saveButtonHeightRatio = ReadFloat_(j, "save_button_height_ratio");
    s.saveButtonTextYOffsetRatio = ReadFloat_(j, "save_button_text_y_offset_ratio");
    s.horizontalPaddingMultiplier = ReadFloat_(j, "horizontal_padding_multiplier");
    return s;
}

DesignListPanelStyle_t ParseDesignListPanelStyle_(const nlohmann::json& j)
{
    DesignListPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.emptyListTextColor = ParseColor_(j, "empty_list_text_color");
    s.textColor = ParseColor_(j, "text_color");
    s.selectedBoxFillColor = ParseColor_(j, "selected_box_fill_color");
    s.unselectedBoxFillColor = ParseColor_(j, "unselected_box_fill_color");
    s.unselectedBoxBorderColor = ParseColor_(j, "unselected_box_border_color");
    s.selectedBoxBorderColor = ParseColor_(j, "selected_box_border_color");
    s.boxWidthRatio = ReadFloat_(j, "box_width_ratio");
    s.boxPaddingRatio = ReadFloat_(j, "box_padding_ratio");
    s.fontSizeRatio = ReadFloat_(j, "font_size_ratio");
    s.labelFontRatio = ReadFloat_(j, "label_font_ratio");
    s.emptyListPaddingMultiplier = ReadFloat_(j, "empty_list_padding_multiplier");
    s.verticalPaddingMultiplier = ReadFloat_(j, "vertical_padding_multiplier");
    s.textPadRatio = ReadFloat_(j, "text_pad_ratio");
    s.textVerticalRatio = ReadFloat_(j, "text_vertical_ratio");
    return s;
}

UnitStatusPanelStyle_t ParseUnitStatusPanelStyle_(const nlohmann::json& j)
{
    UnitStatusPanelStyle_t s{};
    s.backgroundColor = ParseColor_(j, "background_color");
    s.borderColor = ParseColor_(j, "border_color");
    s.mutedTextColor = ParseColor_(j, "muted_text_color");
    s.textColor = ParseColor_(j, "text_color");
    s.headerColor = ParseColor_(j, "header_color");
    s.headerFontSizeRatio = ReadFloat_(j, "header_font_size_ratio");
    s.statFontSizeRatio = ReadFloat_(j, "stat_font_size_ratio");
    s.lineHeightRatio = ReadFloat_(j, "line_height_ratio");
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.designNameLineIndex = ReadFloat_(j, "design_name_line_index");
    s.activeCountLineIndex = ReadFloat_(j, "active_count_line_index");
    s.inProdLineIndex = ReadFloat_(j, "in_prod_line_index");
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
    s.paddingRatio = ReadFloat_(j, "padding_ratio");
    s.tabTextPadX = ReadFloat_(j, "tab_text_pad_x");
    s.tabTextPadY = ReadFloat_(j, "tab_text_pad_y");
    s.summaryTabLayout = ParseLayout_(j, "summary_tab_layout");
    s.attackTabLayout = ParseLayout_(j, "attack_tab_layout");
    s.contentLayout = ParseLayout_(j, "content_layout");
    s.attackButtonLayout = ParseLayout_(j, "attack_button_layout");
    s.attackerListLayout = ParseLayout_(j, "attacker_list_layout");
    s.attackerConfirmLayout = ParseLayout_(j, "attacker_confirm_layout");
    s.attackerCancelLayout = ParseLayout_(j, "attacker_cancel_layout");
    s.listButtonHeight = ReadFloat_(j, "list_button_height");
    s.listButtonGap = ReadFloat_(j, "list_button_gap");
    s.listHeaderHeight = ReadFloat_(j, "list_header_height");
    s.listPadX = ReadFloat_(j, "list_pad_x");
    s.listPadY = ReadFloat_(j, "list_pad_y");
    return s;
}

} // namespace

void UiStyle::Load(const std::string& filePath)
{
    std::cout << "Loading UI style from: " << filePath << "\n";

    const nlohmann::json root = JsonConfigLoader::ReadFile(filePath);
    if (!root.is_object())
    {
        throw std::runtime_error("UI style root must be a JSON object");
    }

    UiStyle style{};
    style.layouts = ParseLayoutsStyle_(root.at("layouts"));
    style.outline.width = ReadFloat_(root.at("outline"), "width");
    style.viewFactory = ParseViewFactoryStyle_(root.at("view_factory"));
    style.tileRenderer = ParseTileRendererStyle_(root.at("tile_renderer"));
    style.mapRenderer = ParseMapRendererStyle_(root.at("map_renderer"));
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
    const nlohmann::json& combatPresentation = root.at("combat_presentation");
    style.combatPresentation = ParseCombatPresentationStyle_(combatPresentation);
    style.bombardPresentation = ParseBombardPresentationStyle_(combatPresentation.at("bombard"));
    style.cameraInput = ParseCameraInputStyle_(root.at("camera_input"));
    style.unitOrderInput = ParseUnitOrderInputStyle_(root.at("unit_order_input"));
    style.commlinksPanel = ParseCommlinksPanelStyle_(root.at("commlinks_panel"));
    style.talkPopup = ParseTalkPopupStyle_(root.at("talk_popup"));
    style.councilVoteView = ParseCouncilVoteViewStyle_(root.at("council_vote_view"));
    style.currentResearchPanel = ParseCurrentResearchPanelStyle_(root.at("current_research_panel"));
    style.settingsPanel = ParseSettingsPanelStyle_(root.at("settings_panel"));
    style.baseView = ParseBaseViewStyle_(root.at("base_view"));
    style.growthDisplay = ParseResourceLinesPanelStyle_(root.at("growth_display"));
    style.commerceDisplay = ParseResourceLinesPanelStyle_(root.at("commerce_display"));
    style.productionDisplay = ParseResourceLinesPanelStyle_(root.at("production_display"));
    style.populationDisplay = ParsePopulationDisplayStyle_(root.at("population_display"));
    style.supportDisplay = ParseSupportDisplayStyle_(root.at("support_display"));
    style.buildingsDisplay = ParseBuildingsDisplayStyle_(root.at("buildings_display"));
    style.baseWorkableAreaDisplay = ParseBaseWorkableAreaDisplayStyle_(root.at("base_workable_area_display"));
    style.buildQueueDisplay = ParseBuildQueueDisplayStyle_(root.at("build_queue_display"));
    style.listSelectorPopup = ParseListSelectorPopupStyle_(root.at("list_selector_popup"));
    style.noticePopup = ParseNoticePopupStyle_(root.at("notice_popup"));
    style.hurryProductionPopup = ParseHurryProductionPopupStyle_(root.at("hurry_production_popup"));
    style.hurryProductionPopup.borderWidth = style.outline.width;
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
