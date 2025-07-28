#include "KeymapParser.h"
#include <iostream>
#include <fstream>

int main() {
    std::cout << "OSARA Keymap Parser Demo" << std::endl;
    std::cout << "========================" << std::endl << std::endl;
    
    // Create a sample OSARA keymap
    KeymapParser::KeymapParser osaraKeymap;
    
    // Add some typical OSARA key bindings
    osaraKeymap.addKeyBinding(1, 65, "OSARA_REPORTFOCUS", KeymapParser::Context::MAIN); // Shift+A
    osaraKeymap.addKeyBinding(1, 83, "OSARA_REPORTSELECTION", KeymapParser::Context::MAIN); // Shift+S
    osaraKeymap.addKeyBinding(1, 84, "OSARA_REPORTTIME", KeymapParser::Context::MAIN); // Shift+T
    
    // Add custom actions
    osaraKeymap.addCustomAction(3, KeymapParser::Context::MAIN, "OSARA_REPORTFOCUS", 
                               "OSARA: Report focus", {"40913"});
    osaraKeymap.addCustomAction(3, KeymapParser::Context::MAIN, "OSARA_REPORTSELECTION", 
                               "OSARA: Report selection", {"40914"});
    
    // Add script actions
    osaraKeymap.addScriptAction(KeymapParser::ScriptBehavior::SHOW_DIALOG, 
                               KeymapParser::Context::MAIN, "_OSARA_CONFIG_reportFocus",
                               "OSARA: Configuration for report focus", "osara_config.lua");
    
    std::cout << "Created OSARA keymap with " << osaraKeymap.getEntryCount() << " entries:" << std::endl;
    
    // Show entry counts by type
    auto counts = osaraKeymap.getEntryCountsByType();
    std::cout << "- Key bindings: " << counts[KeymapParser::EntryType::KEY_BINDING] << std::endl;
    std::cout << "- Custom actions: " << counts[KeymapParser::EntryType::CUSTOM_ACTION] << std::endl;
    std::cout << "- Script actions: " << counts[KeymapParser::EntryType::SCRIPT_ACTION] << std::endl;
    std::cout << std::endl;
    
    // Demonstrate serialization
    std::string serialized = osaraKeymap.toString();
    std::cout << "Serialized keymap:" << std::endl;
    std::cout << serialized << std::endl;
    
    // Save to file
    std::string filename = "osara_keymap_demo.ReaperKeyMap";
    if (osaraKeymap.writeToFile(filename)) {
        std::cout << "Saved keymap to: " << filename << std::endl;
    } else {
        std::cout << "Failed to save keymap to file" << std::endl;
    }
    
    // Demonstrate parsing an existing keymap
    std::cout << std::endl << "Parsing the saved keymap back..." << std::endl;
    KeymapParser::KeymapParser parsedKeymap;
    if (parsedKeymap.parseFile(filename)) {
        std::cout << "Successfully parsed keymap with " << parsedKeymap.getEntryCount() << " entries" << std::endl;
        
        // Find key bindings for a specific action
        auto bindings = parsedKeymap.findKeyBindingsForAction("OSARA_REPORTFOCUS");
        std::cout << "Found " << bindings.size() << " key bindings for OSARA_REPORTFOCUS:" << std::endl;
        for (auto* binding : bindings) {
            std::cout << "  " << binding->getKeyComboString() << std::endl;
        }
    } else {
        std::cout << "Failed to parse keymap file" << std::endl;
        if (parsedKeymap.hasErrors()) {
            std::cout << "Parse errors:" << std::endl;
            for (const auto& error : parsedKeymap.getParseErrors()) {
                std::cout << "  Line " << error.first << ": " << error.second << std::endl;
            }
        }
    }
    
    // Demonstrate conflict detection
    std::cout << std::endl << "Testing conflict detection..." << std::endl;
    KeymapParser::KeymapParser testKeymap;
    testKeymap.addKeyBinding(1, 65, "ACTION1", KeymapParser::Context::MAIN);
    testKeymap.addKeyBinding(1, 65, "ACTION2", KeymapParser::Context::MAIN); // Same key combo
    
    auto conflicts = testKeymap.findConflictingKeyBindings();
    std::cout << "Found " << conflicts.size() << " conflicting key bindings" << std::endl;
    
    // Demonstrate utilities
    std::cout << std::endl << "Utility functions:" << std::endl;
    std::cout << "Context display name for MIDI_EDITOR: " 
              << KeymapParser::KeymapUtils::contextToDisplayName(KeymapParser::Context::MIDI_EDITOR) << std::endl;
    
    std::cout << "Valid action command ID '1007': " 
              << (KeymapParser::KeymapUtils::isValidActionCommandId("1007") ? "Yes" : "No") << std::endl;
    
    std::cout << "Valid action command ID 'TEST_ACTION': " 
              << (KeymapParser::KeymapUtils::isValidActionCommandId("TEST_ACTION") ? "Yes" : "No") << std::endl;
    
    std::cout << std::endl << "Demo completed successfully!" << std::endl;
    
    return 0;
}
