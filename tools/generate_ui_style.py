#!/usr/bin/env python3
"""Generate UiStyle C++ headers/sources and config/ui/style.json from a single schema."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Each section: (cpp_member, json_key, fields)
# field: (cpp_name, json_key, kind, value)
# kind: color | layout | float | uint | int

SECTIONS: list[tuple[str, str, list[tuple[str, str, str, object]]]] = [
    (
        "layouts",
        "layouts",
        [
            ("fullscreen", "fullscreen", "layout", [0.0, 0.0, 1.0, 1.0]),
            ("map", "map", "layout", [0.0, 0.0, 1.0, 0.70]),
            ("topPanel", "top_panel", "layout", [0.2, 0.05, 0.6, 0.60]),
            ("leftPanel", "left_panel", "layout", [0.0, 0.70, 0.20, 0.30]),
            ("locationPanel", "location_panel", "layout", [0.20, 0.70, 0.12, 0.30]),
            ("centerPanel", "center_panel", "layout", [0.32, 0.70, 0.46, 0.20]),
            ("bottomPanel", "bottom_panel", "layout", [0.32, 0.90, 0.46, 0.10]),
            ("rightPanel", "right_panel", "layout", [0.78, 0.70, 0.22, 0.30]),
            ("rightButton", "right_button", "layout", [0.90, 0.67, 0.10, 0.03]),
            ("popup", "popup", "layout", [0.5, 0.75, 0.25, 0.125]),
            ("popupSmall", "popup_small", "layout", [0.3, 0.3, 0.4, 0.4]),
        ],
    ),
    (
        "viewFactory",
        "view_factory",
        [
            ("fullscreenOriginX", "fullscreen_origin_x", "float", 0.0),
            ("fullscreenOriginY", "fullscreen_origin_y", "float", 0.0),
        ],
    ),
    (
        "tileRenderer",
        "tile_renderer",
        [
            ("tileBorderColor", "tile_border_color", "color", [80, 80, 80, 255]),
            ("fogTerrainColor", "fog_terrain_color", "color", [110, 110, 110, 255]),
            ("clearTerrainTextColor", "clear_terrain_text_color", "color", [255, 255, 255, 255]),
            # Elevation fill: water darker at depth, land lighter at altitude.
            ("waterLowColor", "water_low_color", "color", [8, 24, 72, 255]),
            ("waterHighColor", "water_high_color", "color", [64, 140, 210, 255]),
            ("landLowColor", "land_low_color", "color", [92, 58, 28, 255]),
            ("landHighColor", "land_high_color", "color", [210, 176, 128, 255]),
            ("forestColor", "forest_color", "color", [34, 140, 56, 255]),
            ("fungusColor", "fungus_color", "color", [220, 80, 180, 255]),
            ("minElevationMeters", "min_elevation_meters", "int", -4000),
            ("maxElevationMeters", "max_elevation_meters", "int", 4000),
            ("fogFillDimRatio", "fog_fill_dim_ratio", "float", 0.55),
            ("tileBorderWidth", "tile_border_width", "float", -1.0),
            ("tileFontSize", "tile_font_size", "uint", 14),
            ("tileTextOffsetXRatio", "tile_text_offset_x_ratio", "float", 0.1),
            ("tileTextOffsetYRatio", "tile_text_offset_y_ratio", "float", 0.3),
        ],
    ),
    (
        "worldDisplay",
        "world_display",
        [
            ("defaultTileScale", "default_tile_scale", "float", 0.1),
            ("baseNameFontSizeRatio", "base_name_font_size_ratio", "float", 0.25),
            ("baseTextOffsetRatio", "base_text_offset_ratio", "float", 0.1),
            ("baseNameWidthRatio", "base_name_width_ratio", "float", 0.8),
            ("baseNameCharWidthRatio", "base_name_char_width_ratio", "float", 0.5),
            ("sensorMarkerColor", "sensor_marker_color", "color", [40, 220, 120, 255]),
            ("sensorMarkerFontSizeRatio", "sensor_marker_font_size_ratio", "float", 0.22),
            ("sensorMarkerWidthRatio", "sensor_marker_width_ratio", "float", 0.28),
            ("sensorMarkerHeightRatio", "sensor_marker_height_ratio", "float", 0.28),
            ("sensorMarkerInsetRatio", "sensor_marker_inset_ratio", "float", 0.04),
            ("monolithMarkerColor", "monolith_marker_color", "color", [160, 120, 255, 255]),
            ("monolithMarkerFontSizeRatio", "monolith_marker_font_size_ratio", "float", 0.24),
            ("monolithMarkerWidthRatio", "monolith_marker_width_ratio", "float", 0.32),
            ("monolithMarkerHeightRatio", "monolith_marker_height_ratio", "float", 0.32),
            ("monolithMarkerInsetRatio", "monolith_marker_inset_ratio", "float", 0.04),
            ("shroudColor", "shroud_color", "color", [0, 0, 0, 255]),
            ("pathPreviewColor", "path_preview_color", "color", [255, 0, 0, 220]),
            ("pathPreviewLineThicknessRatio", "path_preview_line_thickness_ratio", "float", 0.08),
            ("baseNameColor", "base_name_color", "color", [255, 255, 0, 255]),
            ("sensorLabelColor", "sensor_label_color", "color", [0, 0, 0, 255]),
            ("monolithLabelColor", "monolith_label_color", "color", [255, 255, 255, 255]),
        ],
    ),
    (
        "unitMarker",
        "unit_marker",
        [
            ("fontSizeRatio", "font_size_ratio", "float", 0.2),
            ("widthRatio", "width_ratio", "float", 0.22),
            ("heightRatio", "height_ratio", "float", 0.22),
            ("spacingRatio", "spacing_ratio", "float", 0.03),
            ("markerColor", "marker_color", "color", [0, 220, 255, 255]),
            ("exhaustedColor", "exhausted_color", "color", [0, 110, 130, 200]),
            ("selectionBorderOffset", "selection_border_offset", "float", 1.0),
            ("selectionBorderExpansion", "selection_border_expansion", "float", 2.0),
            ("selectionBorderWidth", "selection_border_width", "float", 2.0),
            ("selectionBorderColor", "selection_border_color", "color", [255, 255, 0, 255]),
            ("hitOverlayFill", "hit_overlay_fill", "color", [255, 40, 40, 160]),
            ("hitOverlayBorder", "hit_overlay_border", "color", [255, 80, 80, 255]),
            ("hitOverlayBorderWidth", "hit_overlay_border_width", "float", 2.0),
            ("initialTextColor", "initial_text_color", "color", [0, 0, 0, 255]),
        ],
    ),
    (
        "selectedUnitPanel",
        "selected_unit_panel",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 40, 255]),
            ("borderColor", "border_color", "color", [100, 100, 160, 255]),
            ("mutedTextColor", "muted_text_color", "color", [100, 100, 120, 255]),
            ("iconColor", "icon_color", "color", [0, 220, 255, 255]),
            ("iconExhaustedColor", "icon_exhausted_color", "color", [0, 110, 130, 200]),
            ("bodyTextColor", "body_text_color", "color", [255, 255, 255, 255]),
            ("iconInitialTextColor", "icon_initial_text_color", "color", [0, 0, 0, 255]),
            ("paddingRatio", "padding_ratio", "float", 0.06),
            ("iconSizeRatio", "icon_size_ratio", "float", 0.45),
            ("iconInitialFontRatio", "icon_initial_font_ratio", "float", 0.45),
            ("textFontRatio", "text_font_ratio", "float", 0.06),
            ("textGapRatio", "text_gap_ratio", "float", 0.04),
            ("iconInitialOffsetXRatio", "icon_initial_offset_x_ratio", "float", 0.3),
            ("iconInitialOffsetYRatio", "icon_initial_offset_y_ratio", "float", 0.25),
            ("iconCenterRatio", "icon_center_ratio", "float", 0.5),
        ],
    ),
    (
        "locationPanel",
        "location_panel",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 40, 255]),
            ("borderColor", "border_color", "color", [100, 100, 160, 255]),
            ("mutedTextColor", "muted_text_color", "color", [100, 100, 120, 255]),
            ("bodyTextColor", "body_text_color", "color", [255, 255, 255, 255]),
            ("paddingRatio", "padding_ratio", "float", 0.06),
            ("previewSizeRatio", "preview_size_ratio", "float", 0.40),
            ("textFontRatio", "text_font_ratio", "float", 0.055),
            ("textGapRatio", "text_gap_ratio", "float", 0.025),
        ],
    ),
    (
        "unitStackPanel",
        "unit_stack_panel",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 40, 255]),
            ("borderColor", "border_color", "color", [100, 100, 160, 255]),
            ("statTextColor", "stat_text_color", "color", [255, 255, 255, 255]),
            ("paddingRatio", "padding_ratio", "float", 0.04),
            ("slotGapRatio", "slot_gap_ratio", "float", 0.02),
            ("iconHeightRatio", "icon_height_ratio", "float", 0.55),
            ("statFontRatio", "stat_font_ratio", "float", 0.22),
        ],
    ),
    (
        "infoPanel",
        "info_panel",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 40, 255]),
            ("borderColor", "border_color", "color", [100, 100, 160, 255]),
            ("defaultLineColor", "default_line_color", "color", [255, 255, 255, 255]),
            ("textHeightEstimate", "text_height_estimate", "float", 20.0),
            ("textVerticalCenterRatio", "text_vertical_center_ratio", "float", 0.5),
            ("textHorizontalPadding", "text_horizontal_padding", "float", 10.0),
            ("fontSize", "font_size", "uint", 18),
        ],
    ),
    (
        "endTurnButton",
        "end_turn_button",
        [
            ("idleFillColor", "idle_fill_color", "color", [40, 40, 70, 255]),
            ("readyFillColor", "ready_fill_color", "color", [40, 120, 50, 255]),
            ("borderColor", "border_color", "color", [100, 100, 160, 255]),
            ("readyBorderColor", "ready_border_color", "color", [120, 220, 100, 255]),
            ("labelColor", "label_color", "color", [255, 255, 255, 255]),
            ("fontSize", "font_size", "uint", 16),
            ("textPadX", "text_pad_x", "float", 10.0),
            ("textPadY", "text_pad_y", "float", 8.0),
            ("layout", "layout", "layout", [0.0, 0.0, 1.0, 0.08]),
        ],
    ),
    (
        "commlinksButton",
        "commlinks_button",
        [
            ("fillColor", "fill_color", "color", [40, 40, 70, 255]),
            ("borderColor", "border_color", "color", [100, 100, 160, 255]),
            ("labelColor", "label_color", "color", [255, 255, 255, 255]),
            ("fontSize", "font_size", "uint", 16),
            ("textPadX", "text_pad_x", "float", 10.0),
            ("textPadY", "text_pad_y", "float", 8.0),
        ],
    ),
    (
        "worldView",
        "world_view",
        [
            ("researchTextColor", "research_text_color", "color", [100, 200, 255, 255]),
            ("missionYearColor", "mission_year_color", "color", [255, 255, 255, 255]),
            ("energyTextColor", "energy_text_color", "color", [255, 255, 0, 255]),
        ],
    ),
    (
        "combatView",
        "combat_view",
        [
            ("sideNameColor", "side_name_color", "color", [255, 255, 255, 255]),
            ("roundHeaderColor", "round_header_color", "color", [255, 255, 255, 255]),
            ("idleLabelColor", "idle_label_color", "color", [255, 255, 255, 255]),
            ("hpLineColor", "hp_line_color", "color", [255, 255, 0, 255]),
            ("rollsLineColor", "rolls_line_color", "color", [180, 180, 220, 255]),
            ("hitLineColor", "hit_line_color", "color", [255, 0, 0, 255]),
        ],
    ),
    (
        "combatPresentation",
        "combat_presentation",
        [
            ("defaultDamageFlashMs", "default_damage_flash_ms", "int", 500),
            ("defaultInterRoundDelayMs", "default_inter_round_delay_ms", "int", 250),
        ],
    ),
    (
        "cameraInput",
        "camera_input",
        [
            ("edgeZone", "edge_zone", "float", 0.05),
            ("relativeMin", "relative_min", "float", 0.0),
            ("relativeMax", "relative_max", "float", 1.0),
            ("cameraScrollStep", "camera_scroll_step", "int", 1),
            ("initialCameraOffset", "initial_camera_offset", "int", 0),
            ("edgeScrollSpeed", "edge_scroll_speed", "float", 0.02),
        ],
    ),
    (
        "unitOrderInput",
        "unit_order_input",
        [
            ("holdThresholdMs", "hold_threshold_ms", "int", 500),
        ],
    ),
    (
        "commlinksPanel",
        "commlinks_panel",
        [
            ("backgroundColor", "background_color", "color", [30, 30, 50, 255]),
            ("borderColor", "border_color", "color", [80, 80, 120, 255]),
            ("titleColor", "title_color", "color", [255, 255, 255, 255]),
            ("factionNameColor", "faction_name_color", "color", [255, 255, 0, 255]),
            ("statusColor", "status_color", "color", [255, 255, 255, 255]),
            ("titleFontSize", "title_font_size", "uint", 18),
            ("rowFontSize", "row_font_size", "uint", 16),
            ("titlePadX", "title_pad_x", "float", 0.05),
            ("titlePadY", "title_pad_y", "float", 0.05),
            ("rowStartY", "row_start_y", "float", 0.20),
            ("rowHeight", "row_height", "float", 0.08),
            ("rowPadX", "row_pad_x", "float", 0.05),
            ("statusPadX", "status_pad_x", "float", 0.55),
        ],
    ),
    (
        "currentResearchPanel",
        "current_research_panel",
        [
            ("labelLayout", "label_layout", "layout", [0.0, 0.0, 1.0, 0.35]),
            ("targetLayout", "target_layout", "layout", [0.0, 0.35, 1.0, 0.4]),
            ("progressLayout", "progress_layout", "layout", [0.0, 0.75, 1.0, 0.25]),
            ("backgroundColor", "background_color", "color", [30, 30, 50, 255]),
            ("borderColor", "border_color", "color", [80, 80, 120, 255]),
            ("labelColor", "label_color", "color", [255, 255, 255, 255]),
            ("targetColor", "target_color", "color", [255, 255, 0, 255]),
            ("progressColor", "progress_color", "color", [0, 255, 0, 255]),
            ("labelFontSize", "label_font_size", "uint", 14),
            ("targetFontSize", "target_font_size", "uint", 16),
            ("progressFontSize", "progress_font_size", "uint", 13),
        ],
    ),
    (
        "settingsPanel",
        "settings_panel",
        [
            ("backgroundColor", "background_color", "color", [30, 30, 50, 255]),
            ("borderColor", "border_color", "color", [80, 80, 120, 255]),
            ("titleColor", "title_color", "color", [255, 255, 255, 255]),
            ("rowColor", "row_color", "color", [255, 255, 0, 255]),
            ("titleFontSize", "title_font_size", "uint", 18),
            ("rowFontSize", "row_font_size", "uint", 16),
            ("titleLayout", "title_layout", "layout", [0.05, 0.05, 0.9, 0.15]),
            ("rowLayout", "row_layout", "layout", [0.05, 0.25, 0.9, 0.2]),
        ],
    ),
    (
        "baseView",
        "base_view",
        [
            ("growthLayout", "growth_layout", "layout", [0.0, 0.0, 0.2, 1.0]),
            ("workableLayout", "workable_layout", "layout", [0.2, 0.0, 0.6, 1.0]),
            ("buildingsLayout", "buildings_layout", "layout", [0.8, 0.0, 0.2, 1.0]),
            ("productionLayout", "production_layout", "layout", [0.0, 0.0, 0.666667, 1.0]),
            ("buildQueueLayout", "build_queue_layout", "layout", [0.666667, 0.0, 0.333333, 1.0]),
            ("baseNameLayout", "base_name_layout", "layout", [0.0, 0.0, 1.0, 0.333333]),
            ("populationLayout", "population_layout", "layout", [0.0, 0.333333, 1.0, 0.666667]),
        ],
    ),
    (
        "growthDisplay",
        "growth_display",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 20, 255]),
            ("textColor", "text_color", "color", [255, 255, 255, 255]),
            ("headerFontSizeRatio", "header_font_size_ratio", "float", 0.04),
            ("entryFontSizeRatio", "entry_font_size_ratio", "float", 0.03),
            ("lineHeightRatio", "line_height_ratio", "float", 0.05),
            ("leftPaddingRatio", "left_padding_ratio", "float", 0.02),
            ("stockpileLineIndex", "stockpile_line_index", "float", 1.0),
            ("requiredLineIndex", "required_line_index", "float", 2.0),
            ("productionLineIndex", "production_line_index", "float", 3.0),
        ],
    ),
    (
        "productionDisplay",
        "production_display",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 20, 255]),
            ("textColor", "text_color", "color", [255, 255, 255, 255]),
            ("headerFontSizeRatio", "header_font_size_ratio", "float", 0.04),
            ("entryFontSizeRatio", "entry_font_size_ratio", "float", 0.03),
            ("lineHeightRatio", "line_height_ratio", "float", 0.05),
            ("leftPaddingRatio", "left_padding_ratio", "float", 0.02),
            ("stockpileLineIndex", "stockpile_line_index", "float", 1.0),
            ("requiredLineIndex", "required_line_index", "float", 2.0),
            ("productionLineIndex", "production_line_index", "float", 3.0),
        ],
    ),
    (
        "populationDisplay",
        "population_display",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 20, 255]),
            ("headerTextColor", "header_text_color", "color", [255, 255, 255, 255]),
            ("headerFontSizeRatio", "header_font_size_ratio", "float", 0.04),
            ("popBoxSizeRatio", "pop_box_size_ratio", "float", 0.6),
            ("popBoxSpacingRatio", "pop_box_spacing_ratio", "float", 0.02),
            ("leftPaddingRatio", "left_padding_ratio", "float", 0.02),
            ("popBoxFontSizeRatio", "pop_box_font_size_ratio", "float", 0.6),
            ("popRowYOffsetRatio", "pop_row_y_offset_ratio", "float", 0.02),
            ("popBoxTextXOffsetRatio", "pop_box_text_x_offset_ratio", "float", 0.35),
            ("popBoxTextYOffsetRatio", "pop_box_text_y_offset_ratio", "float", 0.2),
            ("popBoxBorderWidth", "pop_box_border_width", "float", 2.0),
            ("popBoxFillColor", "pop_box_fill_color", "color", [0, 0, 255, 255]),
            ("popBoxBorderColor", "pop_box_border_color", "color", [255, 255, 255, 255]),
            ("popLetterColor", "pop_letter_color", "color", [255, 255, 255, 255]),
        ],
    ),
    (
        "supportDisplay",
        "support_display",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 40, 255]),
            ("borderColor", "border_color", "color", [100, 100, 160, 255]),
            ("paddingRatio", "padding_ratio", "float", 0.03),
            ("iconSizeRatio", "icon_size_ratio", "float", 0.10),
            ("iconGapRatio", "icon_gap_ratio", "float", 0.015),
        ],
    ),
    (
        "baseWorkableAreaDisplay",
        "base_workable_area_display",
        [
            ("gridDimension", "grid_dimension", "int", 5),
            ("gridCenterOffset", "grid_center_offset", "float", 2.0),
            ("backgroundColor", "background_color", "color", [20, 20, 20, 255]),
            ("tileBorderColor", "tile_border_color", "color", [80, 80, 80, 255]),
            ("tileBorderWidth", "tile_border_width", "float", -1.0),
            ("baseLabelFontSize", "base_label_font_size", "uint", 14),
            ("tileFontSize", "tile_font_size", "uint", 12),
            ("tileTextOffsetXRatio", "tile_text_offset_x_ratio", "float", 0.05),
            ("tileTextOffsetYRatio", "tile_text_offset_y_ratio", "float", 0.35),
            ("baseLabelColor", "base_label_color", "color", [255, 255, 0, 255]),
            ("workedTileTextColor", "worked_tile_text_color", "color", [0, 255, 0, 255]),
            ("unworkedTileTextColor", "unworked_tile_text_color", "color", [255, 255, 255, 255]),
        ],
    ),
    (
        "popTypeSelectorPopup",
        "pop_type_selector_popup",
        [
            ("headerFontSizeRatio", "header_font_size_ratio", "float", 0.04),
            ("entryFontSizeRatio", "entry_font_size_ratio", "float", 0.03),
            ("lineHeightRatio", "line_height_ratio", "float", 0.05),
            ("paddingRatio", "padding_ratio", "float", 0.02),
            ("headerLineOffset", "header_line_offset", "float", 2.0),
            ("backgroundColor", "background_color", "color", [20, 20, 40, 230]),
            ("borderColor", "border_color", "color", [255, 255, 0, 255]),
            ("headerColor", "header_color", "color", [255, 255, 0, 255]),
            ("hintColor", "hint_color", "color", [255, 255, 255, 255]),
            ("entryColor", "entry_color", "color", [255, 255, 255, 255]),
        ],
    ),
    (
        "productionSelectorPopup",
        "production_selector_popup",
        [
            ("headerFontSizeRatio", "header_font_size_ratio", "float", 0.04),
            ("entryFontSizeRatio", "entry_font_size_ratio", "float", 0.03),
            ("lineHeightRatio", "line_height_ratio", "float", 0.05),
            ("paddingRatio", "padding_ratio", "float", 0.02),
            ("headerLineOffset", "header_line_offset", "float", 2.0),
            ("backgroundColor", "background_color", "color", [20, 20, 40, 230]),
            ("borderColor", "border_color", "color", [255, 255, 0, 255]),
            ("headerColor", "header_color", "color", [255, 255, 0, 255]),
            ("hintColor", "hint_color", "color", [255, 255, 255, 255]),
            ("entryColor", "entry_color", "color", [255, 255, 255, 255]),
        ],
    ),
    (
        "socialEngineeringDisplay",
        "social_engineering_display",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 30, 255]),
            ("borderColor", "border_color", "color", [80, 80, 120, 255]),
            ("activePolicyColor", "active_policy_color", "color", [255, 220, 80, 255]),
            ("inactivePolicyColor", "inactive_policy_color", "color", [200, 200, 200, 255]),
            ("categoryColor", "category_color", "color", [160, 180, 255, 255]),
            ("scoreHeaderColor", "score_header_color", "color", [160, 180, 255, 255]),
            ("hiddenPolicySlotColor", "hidden_policy_slot_color", "color", [50, 50, 65, 255]),
            ("scoreValueColor", "score_value_color", "color", [255, 255, 255, 255]),
            ("factionBonusBodyColor", "faction_bonus_body_color", "color", [255, 255, 255, 255]),
            ("scoresPanelWidthRatio", "scores_panel_width_ratio", "float", 0.2),
            ("categoryRowHeightRatio", "category_row_height_ratio", "float", 0.25),
            ("categoryNameRowRatio", "category_name_row_ratio", "float", 0.28),
            ("policyNameRowRatio", "policy_name_row_ratio", "float", 0.36),
            ("policyBonusRowRatio", "policy_bonus_row_ratio", "float", 0.36),
            ("horizontalPaddingRatio", "horizontal_padding_ratio", "float", 0.01),
            ("verticalPaddingRatio", "vertical_padding_ratio", "float", 0.02),
            ("headerFontSizeRatio", "header_font_size_ratio", "float", 0.045),
            ("categoryFontSizeRatio", "category_font_size_ratio", "float", 0.04),
            ("policyFontSizeRatio", "policy_font_size_ratio", "float", 0.032),
            ("bonusFontSizeRatio", "bonus_font_size_ratio", "float", 0.026),
            ("scoreFontSizeRatio", "score_font_size_ratio", "float", 0.028),
            ("factionBonusSectionRatio", "faction_bonus_section_ratio", "float", 0.18),
            ("factionNameRowRatio", "faction_name_row_ratio", "float", 0.45),
            ("factionBonusRowRatio", "faction_bonus_row_ratio", "float", 0.55),
            ("factionNameFontSizeRatio", "faction_name_font_size_ratio", "float", 0.034),
            ("factionBonusFontSizeRatio", "faction_bonus_font_size_ratio", "float", 0.024),
        ],
    ),
    (
        "socialEngineeringBottomPanel",
        "social_engineering_bottom_panel",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 30, 255]),
            ("borderColor", "border_color", "color", [80, 80, 120, 255]),
            ("valueColor", "value_color", "color", [255, 255, 255, 255]),
            ("rowHeightRatio", "row_height_ratio", "float", 0.5),
            ("horizontalPaddingRatio", "horizontal_padding_ratio", "float", 0.02),
            ("verticalPaddingRatio", "vertical_padding_ratio", "float", 0.08),
            ("valueFontSizeRatio", "value_font_size_ratio", "float", 0.07),
        ],
    ),
    (
        "unitDesignerView",
        "unit_designer_view",
        [
            ("leftSlotColumnLayout", "left_slot_column_layout", "layout", [0.0, 0.0, 0.2, 1.0]),
            ("designStatsLayout", "design_stats_layout", "layout", [0.2, 0.0, 0.6, 1.0]),
            ("rightSlotColumnLayout", "right_slot_column_layout", "layout", [0.8, 0.0, 0.2, 1.0]),
        ],
    ),
    (
        "slotColumnPanel",
        "slot_column_panel",
        [
            ("visibleSlots", "visible_slots", "int", 3),
            ("arrowHeightRatio", "arrow_height_ratio", "float", 0.08),
            ("labelFontSizeRatio", "label_font_size_ratio", "float", 0.08),
            ("nameFontSizeRatio", "name_font_size_ratio", "float", 0.07),
            ("paddingRatio", "padding_ratio", "float", 0.04),
            ("arrowAreaMultiplier", "arrow_area_multiplier", "float", 2.0),
            ("arrowFontSizeRatio", "arrow_font_size_ratio", "float", 0.6),
            ("arrowPadXRatio", "arrow_pad_x_ratio", "float", 0.4),
            ("nameLabelSpacingMultiplier", "name_label_spacing_multiplier", "float", 1.4),
            ("arrowFillColor", "arrow_fill_color", "color", [30, 30, 40, 255]),
            ("arrowBorderColor", "arrow_border_color", "color", [60, 60, 80, 255]),
            ("disabledArrowColor", "disabled_arrow_color", "color", [60, 60, 60, 255]),
            ("enabledArrowColor", "enabled_arrow_color", "color", [255, 255, 255, 255]),
            ("slotFillColor", "slot_fill_color", "color", [30, 30, 35, 255]),
            ("slotBorderColor", "slot_border_color", "color", [80, 80, 100, 255]),
            ("labelTextColor", "label_text_color", "color", [150, 150, 170, 255]),
            ("emptyNameColor", "empty_name_color", "color", [80, 80, 80, 255]),
            ("filledNameColor", "filled_name_color", "color", [255, 255, 255, 255]),
        ],
    ),
    (
        "componentSlotDisplay",
        "component_slot_display",
        [
            ("backgroundColor", "background_color", "color", [30, 30, 35, 255]),
            ("borderColor", "border_color", "color", [80, 80, 100, 255]),
            ("labelTextColor", "label_text_color", "color", [150, 150, 170, 255]),
            ("emptyNameColor", "empty_name_color", "color", [80, 80, 80, 255]),
            ("filledNameColor", "filled_name_color", "color", [255, 255, 255, 255]),
            ("labelFontSizeRatio", "label_font_size_ratio", "float", 0.08),
            ("nameFontSizeRatio", "name_font_size_ratio", "float", 0.07),
            ("paddingRatio", "padding_ratio", "float", 0.04),
            ("nameLabelSpacingMultiplier", "name_label_spacing_multiplier", "float", 1.4),
        ],
    ),
    (
        "componentSelectorPopup",
        "component_selector_popup",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 40, 255]),
            ("borderColor", "border_color", "color", [100, 100, 180, 255]),
            ("titleColor", "title_color", "color", [255, 255, 0, 255]),
            ("entryColor", "entry_color", "color", [255, 255, 255, 255]),
            ("borderWidth", "border_width", "float", 2.0),
            ("titleFontSizeRatio", "title_font_size_ratio", "float", 0.05),
            ("entryFontSizeRatio", "entry_font_size_ratio", "float", 0.04),
            ("entryHeightRatio", "entry_height_ratio", "float", 0.07),
            ("paddingRatio", "padding_ratio", "float", 0.03),
            ("titleHeightMultiplier", "title_height_multiplier", "float", 2.0),
        ],
    ),
    (
        "designStatsDisplay",
        "design_stats_display",
        [
            ("backgroundColor", "background_color", "color", [15, 15, 25, 255]),
            ("borderColor", "border_color", "color", [60, 60, 110, 255]),
            ("incompleteTextColor", "incomplete_text_color", "color", [120, 120, 120, 255]),
            ("statTextColor", "stat_text_color", "color", [255, 255, 255, 255]),
            ("saveButtonFillColor", "save_button_fill_color", "color", [30, 60, 30, 255]),
            ("headerColor", "header_color", "color", [255, 255, 0, 255]),
            ("saveButtonBorderColor", "save_button_border_color", "color", [0, 255, 0, 255]),
            ("saveButtonTextColor", "save_button_text_color", "color", [0, 255, 0, 255]),
            ("headerFontSizeRatio", "header_font_size_ratio", "float", 0.04),
            ("statFontSizeRatio", "stat_font_size_ratio", "float", 0.032),
            ("lineHeightRatio", "line_height_ratio", "float", 0.055),
            ("paddingRatio", "padding_ratio", "float", 0.02),
            ("saveButtonHeightRatio", "save_button_height_ratio", "float", 0.07),
            ("saveButtonTextYOffsetRatio", "save_button_text_y_offset_ratio", "float", 0.2),
            ("horizontalPaddingMultiplier", "horizontal_padding_multiplier", "float", 2.0),
        ],
    ),
    (
        "designListPanel",
        "design_list_panel",
        [
            ("backgroundColor", "background_color", "color", [10, 10, 15, 255]),
            ("borderColor", "border_color", "color", [60, 60, 80, 255]),
            ("emptyListTextColor", "empty_list_text_color", "color", [100, 100, 100, 255]),
            ("selectedBoxFillColor", "selected_box_fill_color", "color", [50, 50, 90, 255]),
            ("unselectedBoxFillColor", "unselected_box_fill_color", "color", [25, 25, 40, 255]),
            ("unselectedBoxBorderColor", "unselected_box_border_color", "color", [80, 80, 110, 255]),
            ("selectedBoxBorderColor", "selected_box_border_color", "color", [255, 255, 0, 255]),
            ("boxWidthRatio", "box_width_ratio", "float", 0.15),
            ("boxPaddingRatio", "box_padding_ratio", "float", 0.005),
            ("fontSizeRatio", "font_size_ratio", "float", 0.07),
            ("labelFontRatio", "label_font_ratio", "float", 0.05),
            ("emptyListPaddingMultiplier", "empty_list_padding_multiplier", "float", 4.0),
            ("verticalPaddingMultiplier", "vertical_padding_multiplier", "float", 2.0),
            ("textPadRatio", "text_pad_ratio", "float", 0.05),
            ("textVerticalRatio", "text_vertical_ratio", "float", 0.35),
        ],
    ),
    (
        "unitStatusPanel",
        "unit_status_panel",
        [
            ("backgroundColor", "background_color", "color", [20, 20, 20, 255]),
            ("borderColor", "border_color", "color", [80, 80, 80, 255]),
            ("mutedTextColor", "muted_text_color", "color", [100, 100, 100, 255]),
            ("headerColor", "header_color", "color", [255, 255, 0, 255]),
            ("headerFontSizeRatio", "header_font_size_ratio", "float", 0.08),
            ("statFontSizeRatio", "stat_font_size_ratio", "float", 0.07),
            ("lineHeightRatio", "line_height_ratio", "float", 0.10),
            ("paddingRatio", "padding_ratio", "float", 0.04),
            ("designNameLineIndex", "design_name_line_index", "float", 1.0),
            ("activeCountLineIndex", "active_count_line_index", "float", 2.0),
            ("inProdLineIndex", "in_prod_line_index", "float", 3.0),
        ],
    ),
]


def cpp_type(kind: str) -> str:
    return {
        "color": "Color_t",
        "layout": "RatioLayout_t",
        "float": "float",
        "uint": "unsigned int",
        "int": "int",
    }[kind]


def struct_name(member: str) -> str:
    return member[0].upper() + member[1:] + "Style"


def write_json() -> None:
    out: dict = {}
    for member, json_key, fields in SECTIONS:
        section = {}
        for _cpp, jk, kind, value in fields:
            section[jk] = value
        out[json_key] = section
    path = ROOT / "config" / "ui" / "style.json"
    path.write_text(json.dumps(out, indent=2) + "\n")
    print(f"Wrote {path}")


def write_header() -> None:
    lines = [
        "#pragma once",
        "",
        '#include "graphics/Graphics.h"',
        '#include "ui/UIElement.h"',
        "",
        "#include <string>",
        "",
        "namespace ac",
        "{",
        "",
    ]
    for member, _jk, fields in SECTIONS:
        lines.append(f"struct {struct_name(member)}")
        lines.append("{")
        for cpp, _jk, kind, _v in fields:
            lines.append(f"    {cpp_type(kind)} {cpp}{{}};")
        lines.append("};")
        lines.append("")

    lines += [
        "class UiStyle",
        "{",
        "public:",
        "    // Loads config/ui/style.json (or an absolute/relative path to that file).",
        "    static void Load(const std::string& filePath);",
        "    static const UiStyle& Get();",
        "    static bool IsLoaded();",
        "",
    ]
    for member, _jk, _fields in SECTIONS:
        lines.append(f"    {struct_name(member)} {member};")
    lines += [
        "};",
        "",
        "inline const UiStyle& Style()",
        "{",
        "    return UiStyle::Get();",
        "}",
        "",
        "} // namespace ac",
        "",
    ]
    path = ROOT / "include" / "ui" / "style" / "UiStyle.h"
    path.write_text("\n".join(lines))
    print(f"Wrote {path}")


def write_parser() -> None:
    lines = [
        '#include "ui/style/UiStyle.h"',
        "",
        "#include <nlohmann/json.hpp>",
        "",
        "#include <fstream>",
        "#include <iostream>",
        "#include <stdexcept>",
        "",
        "namespace ac",
        "{",
        "",
        "namespace",
        "{",
        "",
        "UiStyle g_style{};",
        "bool g_loaded = false;",
        "",
        "Color_t ParseColor_(const nlohmann::json& j, const char* key)",
        "{",
        "    const auto& arr = j.at(key);",
        "    if (!arr.is_array() || arr.size() < 3)",
        "    {",
        '        throw std::runtime_error(std::string("Expected RGBA array for \'") + key + "\'");',
        "    }",
        "    const uint8_t a = arr.size() >= 4 ? arr.at(3).get<uint8_t>() : 255;",
        "    return Color_t{",
        "        arr.at(0).get<uint8_t>(),",
        "        arr.at(1).get<uint8_t>(),",
        "        arr.at(2).get<uint8_t>(),",
        "        a};",
        "}",
        "",
        "RatioLayout_t ParseLayout_(const nlohmann::json& j, const char* key)",
        "{",
        "    const auto& arr = j.at(key);",
        "    if (!arr.is_array() || arr.size() != 4)",
        "    {",
        '        throw std::runtime_error(std::string("Expected [x,y,w,h] layout for \'") + key + "\'");',
        "    }",
        "    return RatioLayout_t{",
        "        arr.at(0).get<float>(),",
        "        arr.at(1).get<float>(),",
        "        arr.at(2).get<float>(),",
        "        arr.at(3).get<float>()};",
        "}",
        "",
    ]

    for member, json_key, fields in SECTIONS:
        sn = struct_name(member)
        lines.append(f"{sn} Parse{sn}_(const nlohmann::json& j)")
        lines.append("{")
        lines.append(f"    {sn} s{{}};")
        for cpp, jk, kind, _v in fields:
            if kind == "color":
                lines.append(f'    s.{cpp} = ParseColor_(j, "{jk}");')
            elif kind == "layout":
                lines.append(f'    s.{cpp} = ParseLayout_(j, "{jk}");')
            elif kind == "float":
                lines.append(f'    s.{cpp} = j.at("{jk}").get<float>();')
            elif kind == "uint":
                lines.append(f'    s.{cpp} = j.at("{jk}").get<unsigned int>();')
            elif kind == "int":
                lines.append(f'    s.{cpp} = j.at("{jk}").get<int>();')
        lines.append("    return s;")
        lines.append("}")
        lines.append("")

    lines += [
        "} // namespace",
        "",
        "void UiStyle::Load(const std::string& filePath)",
        "{",
        '    std::cout << "Loading UI style from: " << filePath << "\\n";',
        "",
        "    std::ifstream file(filePath);",
        "    if (!file.is_open())",
        "    {",
        '        throw std::runtime_error("Could not open UI style file: " + filePath);',
        "    }",
        "",
        "    nlohmann::json root;",
        "    file >> root;",
        "    if (!root.is_object())",
        "    {",
        '        throw std::runtime_error("UI style root must be a JSON object");',
        "    }",
        "",
        "    UiStyle style{};",
    ]
    for member, json_key, _fields in SECTIONS:
        sn = struct_name(member)
        lines.append(f'    style.{member} = Parse{sn}_(root.at("{json_key}"));')

    lines += [
        "",
        "    g_style = style;",
        "    g_loaded = true;",
        '    std::cout << "Loaded UI style\\n";',
        "}",
        "",
        "const UiStyle& UiStyle::Get()",
        "{",
        "    if (!g_loaded)",
        "    {",
        '        throw std::runtime_error("UiStyle::Get called before UiStyle::Load");',
        "    }",
        "    return g_style;",
        "}",
        "",
        "bool UiStyle::IsLoaded()",
        "{",
        "    return g_loaded;",
        "}",
        "",
        "} // namespace ac",
        "",
    ]
    path = ROOT / "src" / "ui" / "style" / "UiStyle.cpp"
    path.write_text("\n".join(lines))
    print(f"Wrote {path}")


def main() -> None:
    write_json()
    write_header()
    write_parser()


if __name__ == "__main__":
    main()
