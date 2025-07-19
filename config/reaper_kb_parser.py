"""
Reaper Keymap Parser

This module provides functionality to parse Reaper keymap .ini files according to the
Reaper 6.59 specification.
"""

import re
from typing import Dict, List, Union, Tuple, Optional, Any


class ReaperKeymapParser:
    """
    Parser for Reaper keymap .ini files.
    
    This class provides methods to parse Reaper-kb.ini files which contain
    ACT (Action), SCR (Script), and KEY (Key/Shortcut/MIDI-Note) entries.
    """
    
    # Section IDs and their meanings
    SECTION_IDS = {
        0: "Main",
        1: "Action stays invisible but is kept",
        100: "Main (alt recording)",
        32060: "MIDI Editor",
        32061: "MIDI Event List Editor",
        32062: "MIDI Inline Editor",
        32063: "Media Explorer"
    }
    
    def __init__(self):
        """Initialize the parser."""
        self.actions = []
        self.scripts = []
        self.keys = []
    
    def parse_file(self, file_path: str) -> Dict[str, List[Dict[str, Any]]]:
        """
        Parse a Reaper keymap .ini file.
        
        Args:
            file_path: Path to the Reaper keymap .ini file.
            
        Returns:
            A dictionary containing the parsed actions, scripts, and keys.
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as file:
                lines = file.readlines()
        except UnicodeDecodeError:
            # Try with Latin-1 encoding if UTF-8 fails
            with open(file_path, 'r', encoding='latin-1') as file:
                lines = file.readlines()
        
        # Reset collections
        self.actions = []
        self.scripts = []
        self.keys = []
        
        # Process each line
        for line in lines:
            line = line.strip()
            if not line:
                continue
                
            # Parse based on entry type
            if line.startswith('ACT '):
                self._parse_action(line)
            elif line.startswith('SCR '):
                self._parse_script(line)
            elif line.startswith('KEY '):
                self._parse_key(line)
        
        return {
            'actions': self.actions,
            'scripts': self.scripts,
            'keys': self.keys
        }
    
    def _parse_action(self, line: str) -> None:
        """
        Parse an ACT (Action) entry.
        
        Format: ACT a b "Action Command ID" "Description" ActionCommandID [ActionCommandID] ...
        
        Args:
            line: The line containing the ACT entry.
        """
        # Store the original line with comments
        original_line = line
        
        # Extract the main part of the line (before any comment)
        comment_pos = line.find('#')
        if comment_pos != -1:
            main_part = line[:comment_pos].strip()
            comment = line[comment_pos:].strip()
        else:
            main_part = line.strip()
            comment = ""
        
        # Split the line while preserving quoted strings
        parts = self._split_preserving_quotes(main_part[4:])  # Skip 'ACT '
        
        if len(parts) < 5:
            # Not enough parts for a valid ACT entry
            return
        
        settings = int(parts[0])
        section_id = int(parts[1])
        action_command_id = parts[2].strip('"')
        description = parts[3].strip('"')
        
        # Extract action command IDs (the rest of the parts)
        action_command_ids = parts[4:]
        
        # Parse settings flags
        settings_info = self._parse_act_settings(settings)
        
        # Get section name
        section_name = self.SECTION_IDS.get(section_id, f"Unknown Section ({section_id})")
        
        action = {
            'type': 'ACT',
            'settings': settings,
            'settings_info': settings_info,
            'section_id': section_id,
            'section_name': section_name,
            'action_command_id': action_command_id,
            'description': description,
            'action_command_ids': action_command_ids,
            'original_line': original_line,
            'comment': comment
        }
        
        self.actions.append(action)
    
    def _parse_act_settings(self, settings: int) -> Dict[str, bool]:
        """
        Parse the settings flags for an ACT entry.
        
        Args:
            settings: The settings value.
            
        Returns:
            A dictionary of setting flags and their values.
        """
        return {
            'consolidate_undo_points': bool(settings & 1),
            'show_in_actions_menu': bool(settings & 2),
            'show_as_active_if_all_component_actions_are': bool(settings & 16),
            'active_or_indeterminate': bool(settings & 32)
        }
    
    def _parse_script(self, line: str) -> None:
        """
        Parse a SCR (Script) entry.
        
        Format: SCR a b ActionCommandID "ScriptDescription*" Scriptname.lua|.py
        
        Args:
            line: The line containing the SCR entry.
        """
        # Store the original line with comments
        original_line = line
        
        # Extract the main part of the line (before any comment)
        comment_pos = line.find('#')
        if comment_pos != -1:
            main_part = line[:comment_pos].strip()
            comment = line[comment_pos:].strip()
        else:
            main_part = line.strip()
            comment = ""
        
        # Split the line while preserving quoted strings
        parts = self._split_preserving_quotes(main_part[4:])  # Skip 'SCR '
        
        if len(parts) < 5:
            # Not enough parts for a valid SCR entry
            return
        
        settings = int(parts[0])
        section_id = int(parts[1])
        action_command_id = parts[2]
        description = parts[3].strip('"')
        script_name = parts[4]
        
        # Parse settings flags
        settings_info = self._parse_scr_settings(settings)
        
        # Get section name
        section_name = self.SECTION_IDS.get(section_id, f"Unknown Section ({section_id})")
        
        script = {
            'type': 'SCR',
            'settings': settings,
            'settings_info': settings_info,
            'section_id': section_id,
            'section_name': section_name,
            'action_command_id': action_command_id,
            'description': description,
            'script_name': script_name,
            'original_line': original_line,
            'comment': comment
        }
        
        self.scripts.append(script)
    
    def _parse_scr_settings(self, settings: int) -> Dict[str, Union[bool, str]]:
        """
        Parse the settings flags for a SCR entry.
        
        Args:
            settings: The settings value.
            
        Returns:
            A dictionary of setting flags and their values.
        """
        # Base settings
        result = {
            'consolidate_undo_points': bool(settings & 1),
            'show_in_actions_menu': bool(settings & 2)
        }
        
        # Instance behavior
        if settings == 4:
            result['instance_behavior'] = "Show dialog (Terminate, New Instance, Abort)"
        elif settings == 260:
            result['instance_behavior'] = "Always terminate all instances"
        elif settings == 516:
            result['instance_behavior'] = "Always start a new instance"
        
        return result
    
    def _parse_key(self, line: str) -> None:
        """
        Parse a KEY (Key/Shortcut/MIDI-Note) entry.
        
        Format: KEY ModifierValue Key_Note_Value _ActionCommandID section
        
        Args:
            line: The line containing the KEY entry.
        """
        # Store the original line with comments
        original_line = line
        
        # Extract the main part of the line (before any comment)
        comment_pos = line.find('#')
        if comment_pos != -1:
            main_part = line[:comment_pos].strip()
            comment = line[comment_pos:].strip()
        else:
            main_part = line.strip()
            comment = ""
        
        parts = main_part[4:].split()  # Skip 'KEY '
        
        if len(parts) < 4:
            # Not enough parts for a valid KEY entry
            return
        
        modifier_value = int(parts[0])
        key_note_value = int(parts[1])
        action_command_id = parts[2]
        section_id = int(parts[3])
        
        # Parse modifier and key information
        key_info = self._parse_key_info(modifier_value, key_note_value)
        
        # Get section name
        section_name = self.SECTION_IDS.get(section_id, f"Unknown Section ({section_id})")
        
        # Check if this is a global shortcut entry
        is_global = False
        global_scope = None
        
        if section_id in (102, 103):
            is_global = True
            global_scope = "global" if action_command_id == "1" else "global+textfields" if action_command_id == "101" else "unknown"
        
        key = {
            'type': 'KEY',
            'modifier_value': modifier_value,
            'key_note_value': key_note_value,
            'key_info': key_info,
            'action_command_id': action_command_id,
            'section_id': section_id,
            'section_name': section_name,
            'is_global': is_global,
            'original_line': original_line,
            'comment': comment
        }
        
        if is_global:
            key['global_scope'] = global_scope
        
        self.keys.append(key)
    
    def _parse_key_info(self, modifier_value: int, key_note_value: int) -> Dict[str, Any]:
        """
        Parse the modifier and key information for a KEY entry.
        
        Args:
            modifier_value: The modifier value.
            key_note_value: The key/note value.
            
        Returns:
            A dictionary containing the parsed key information.
        """
        result = {
            'raw_modifier': modifier_value,
            'raw_key_value': key_note_value
        }
        
        # MIDI Channel modifiers (144-159, 176-191, 192-207, 224-239)
        if 144 <= modifier_value <= 159:
            channel = modifier_value - 143
            result['type'] = 'MIDI'
            result['midi_type'] = 'Note'
            result['channel'] = channel
            result['note'] = key_note_value % 128
            return result
        elif 176 <= modifier_value <= 191:
            channel = modifier_value - 175
            result['type'] = 'MIDI'
            result['midi_type'] = 'CC'
            result['channel'] = channel
            result['cc'] = key_note_value % 128
            return result
        elif 192 <= modifier_value <= 207:
            channel = modifier_value - 191
            result['type'] = 'MIDI'
            result['midi_type'] = 'PC'
            result['channel'] = channel
            result['pc'] = key_note_value % 128
            return result
        elif 224 <= modifier_value <= 239:
            channel = modifier_value - 223
            result['type'] = 'MIDI'
            result['midi_type'] = 'Pitch'
            result['channel'] = channel
            return result
        
        # MIDI XX YY YY (128-143, 160-175, 208-223, 240-254)
        if (128 <= modifier_value <= 143 or 
            160 <= modifier_value <= 175 or 
            208 <= modifier_value <= 223 or 
            240 <= modifier_value <= 254):
            result['type'] = 'MIDI'
            result['midi_type'] = 'XX YY YY'
            result['xx'] = hex(modifier_value)[2:].upper()
            # YY YY is reversed in the file
            lower_bits = (key_note_value & 0xFF)
            higher_bits = (key_note_value >> 8) & 0xFF
            result['yy_yy'] = f"{lower_bits:02X}{higher_bits:02X}"
            return result
        
        # Special modifier 255
        if modifier_value == 255:
            result['type'] = 'Special'
            
            # Determine the special type based on key_note_value
            if key_note_value in (72, 73, 74, 200, 201, 202, 203, 204, 205, 206, 207):
                result['special_type'] = 'MultiZoom'
            elif key_note_value in (24, 25, 152, 153, 154, 155, 156, 157, 158, 159):
                result['special_type'] = 'MultiRotate'
            elif key_note_value in (40, 168, 169, 170, 171, 172, 173, 174, 175):
                result['special_type'] = 'MultiHorz'
            elif key_note_value in (56, 184, 185, 186, 187, 188, 189, 190, 191):
                result['special_type'] = 'MultiVert'
            elif key_note_value in (88, 90, 216, 217, 218, 219, 220, 221, 222, 223):
                result['special_type'] = 'HorizWheel'
            elif key_note_value in (120, 121, 122, 123, 125, 248, 249, 250, 251, 252, 253, 254, 255):
                result['special_type'] = 'Mousewheel'
            elif key_note_value >= 232:
                result['special_type'] = 'MediaKeyboard'
                # Map media keyboard keys
                media_keys = {
                    232: "MediaKbd??",
                    488: "MediaKbdBrowse-",
                    744: "MediaKbdBrowse+",
                    1000: "MediaKbdBrowseRefr",
                    # ... and so on for all the media keyboard keys
                }
                result['media_key'] = media_keys.get(key_note_value, f"Unknown ({key_note_value})")
            
            return result
        
        # Regular keyboard modifiers
        result['type'] = 'Keyboard'
        
        # Parse keyboard modifiers
        modifiers = []
        if modifier_value & 1:
            modifiers.append('Ctrl')
        if modifier_value & 2:
            modifiers.append('Alt')
        if modifier_value & 4:
            modifiers.append('Shift')
        
        result['modifiers'] = modifiers
        
        # For even modifier values, key_note_value is pure ASCII
        if modifier_value % 2 == 0:
            result['key_type'] = 'ASCII'
            result['key_code'] = key_note_value
            # Try to convert to a readable character if in printable ASCII range
            if 32 <= key_note_value <= 126:
                result['key_char'] = chr(key_note_value)
        else:
            # For odd modifier values, it's more complex
            result['key_type'] = 'Special Key'
            # This would require a more extensive mapping of key codes
        
        return result
    
    def _split_preserving_quotes(self, text: str) -> List[str]:
        """
        Split a string by whitespace while preserving quoted substrings.
        
        Args:
            text: The string to split.
            
        Returns:
            A list of parts.
        """
        pattern = r'([^\s"]+)|"([^"]*)"'
        matches = re.findall(pattern, text)
        return [m[0] or m[1] for m in matches]


def parse_reaper_keymap(file_path: str) -> Dict[str, List[Dict[str, Any]]]:
    """
    Parse a Reaper keymap .ini file.
    
    Args:
        file_path: Path to the Reaper keymap .ini file.
        
    Returns:
        A dictionary containing the parsed actions, scripts, and keys.
    """
    parser = ReaperKeymapParser()
    return parser.parse_file(file_path)


if __name__ == "__main__":
    import sys
    import json
    
    if len(sys.argv) < 2:
        print("Usage: python reaper_kb_parser.py <path_to_reaper_kb_ini>")
        sys.exit(1)
    
    result = parse_reaper_keymap(sys.argv[1])
    
    # Print summary
    print(f"Parsed {len(result['actions'])} actions, {len(result['scripts'])} scripts, and {len(result['keys'])} keys.")
    
    # Output detailed results as JSON
    print(json.dumps(result, indent=2))
