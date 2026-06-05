#!/usr/bin/env python3
"""
Generate turn_stages.json from TurnStages.h enum definition.
This script parses the C++ enum and generates the JSON configuration,
preserving any modder customizations from the existing JSON.
"""

import re
import json
from pathlib import Path
from typing import Dict, List, Any


def parse_cpp_enum(header_path: Path) -> List[Dict[str, Any]]:
    """Parse the TurnStage enum from the C++ header file."""
    with open(header_path, 'r') as f:
        content = f.read()
    
    # Extract the enum block
    enum_match = re.search(
        r'enum class TurnStage\s*\{([^}]+)\}',
        content,
        re.DOTALL
    )
    
    if not enum_match:
        raise ValueError("Could not find TurnStage enum in header file")
    
    enum_body = enum_match.group(1)
    
    # Parse enum values
    stages = []
    for line in enum_body.split(','):
        line = line.strip()
        if not line or line == 'Count':
            continue
        
        stage_id = line.strip()
        # Convert CamelCase to snake_case for JSON IDs
        json_id = re.sub(r'(?<!^)(?=[A-Z])', '_', stage_id).lower()
        
        # Convert to readable name
        name = re.sub(r'(?<!^)(?=[A-Z])', ' ', stage_id)
        
        stages.append({
            'cpp_enum': stage_id,
            'id': json_id,
            'name': name
        })
    
    return stages


def build_stage_hierarchy(stages: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """
    Build a hierarchical stage structure from the flat enum list.
    Uses underscore prefixes to determine parent-child relationships.
    """
    root_stages = []
    stage_map = {s['id']: s for s in stages}
    
    for stage in stages:
        cpp_name = stage['cpp_enum']
        json_id = stage['id']
        
        # Check if this is a sub-stage (has underscore in cpp name)
        if '_' in cpp_name:
            # Find parent by removing the last underscore segment
            parts = cpp_name.split('_')
            parent_cpp = '_'.join(parts[:-1])
            parent_id = re.sub(r'(?<!^)(?=[A-Z])', '_', parent_cpp).lower()
            
            if parent_id in stage_map:
                parent = stage_map[parent_id]
                if 'sub_stages' not in parent:
                    parent['sub_stages'] = []
                parent['sub_stages'].append(stage)
        else:
            root_stages.append(stage)
    
    return root_stages


def preserve_customizations(
    new_stages: List[Dict[str, Any]],
    existing_json: Dict[str, Any]
) -> List[Dict[str, Any]]:
    """
    Preserve modder customizations from existing JSON.
    Preserves: hooks, order changes, repeat_for_each_faction, phase
    """
    existing_stages = existing_json.get('turn_processing', {}).get('stages', [])
    existing_map = {s['id']: s for s in existing_stages}
    
    def merge_stage(new_stage: Dict[str, Any], existing_stage: Dict[str, Any] = None) -> Dict[str, Any]:
        """Merge customizations from existing stage into new stage."""
        result = {
            'id': new_stage['id'],
            'name': new_stage['name'],
            'description': existing_stage.get('description', '') if existing_stage else '',
            'hooks': {
                'pre': [],
                'post': [],
                'replace': []
            }
        }
        
        if existing_stage:
            # Preserve customizations
            if 'phase' in existing_stage:
                result['phase'] = existing_stage['phase']
            if 'order' in existing_stage:
                result['order'] = existing_stage['order']
            if 'repeat_for_each_faction' in existing_stage:
                result['repeat_for_each_faction'] = existing_stage['repeat_for_each_faction']
            if 'hooks' in existing_stage:
                result['hooks'] = existing_stage['hooks']
        
        # Handle sub-stages recursively
        if 'sub_stages' in new_stage:
            result['sub_stages'] = []
            if existing_stage and 'sub_stages' in existing_stage:
                existing_sub_map = {s['id']: s for s in existing_stage['sub_stages']}
                for sub_stage in new_stage['sub_stages']:
                    existing_sub = existing_sub_map.get(sub_stage['id'])
                    result['sub_stages'].append(merge_stage(sub_stage, existing_sub))
            else:
                for sub_stage in new_stage['sub_stages']:
                    result['sub_stages'].append(merge_stage(sub_stage))
        
        return result
    
    merged_stages = []
    for new_stage in new_stages:
        existing_stage = existing_map.get(new_stage['id'])
        merged_stages.append(merge_stage(new_stage, existing_stage))
    
    return merged_stages


def generate_json(
    header_path: Path,
    output_path: Path,
    preserve_existing: bool = True
):
    """Generate the turn_stages.json file from the C++ enum."""
    # Parse the enum
    stages = parse_cpp_enum(header_path)
    
    # Build hierarchy
    hierarchical_stages = build_stage_hierarchy(stages)
    
    # Load existing JSON if preserving customizations
    existing_json = {}
    if preserve_existing and output_path.exists():
        with open(output_path, 'r') as f:
            existing_json = json.load(f)
    
    # Preserve customizations
    final_stages = preserve_customizations(hierarchical_stages, existing_json)
    
    # Build final JSON structure
    output_json = {
        'turn_processing': {
            'description': 'Defines the stages and order of turn processing in the game loop',
            'hooks': {
                'description': 'Mod hook definitions that can be attached to stages',
                'hook_types': {
                    'pre': 'Execute custom logic before the stage runs',
                    'post': 'Execute custom logic after the stage runs',
                    'replace': 'Replace the entire stage implementation',
                    'extend': 'Run custom logic in addition to the standard implementation'
                }
            },
            'stages': final_stages
        }
    }
    
    # Write output
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        json.dump(output_json, f, indent=2)
    
    print(f"Generated {output_path}")


if __name__ == '__main__':
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    header_path = project_root / 'src' / 'game' / 'TurnStages.h'
    output_path = project_root / 'config' / 'turn_stages.json'
    
    generate_json(header_path, output_path, preserve_existing=True)
