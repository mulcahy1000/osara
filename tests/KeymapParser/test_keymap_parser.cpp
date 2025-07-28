#include "KeymapParser.h"
#include <iostream>
#include <cassert>

void testBasicParsing() {
    std::cout << "Testing basic parsing..." << std::endl;
    
    KeymapParser::KeymapParser parser;
    
    // Test parsing a simple keymap string
    std::string testKeymap = R"(
KEY 1 65 1007 0
ACT 3 0 "OSARA_REPORTFOCUS" "OSARA: Report focus" 40913
SCR 4 0 _OSARA_CONFIG_reportFocus "OSARA: Configuration for report focus*" osara_config.lua
KEY 5 66 _OSARA_REPORTFOCUS 0
)";
    
    bool success = parser.parseString(testKeymap);
    assert(success && "Failed to parse test keymap");
    
    // Check that we parsed the expected number of entries
    assert(parser.getEntryCount() == 4 && "Expected 4 entries");
    
    // Check entry types
    auto counts = parser.getEntryCountsByType();
    assert(counts[KeymapParser::EntryType::KEY_BINDING] == 2 && "Expected 2 key bindings");
    assert(counts[KeymapParser::EntryType::CUSTOM_ACTION] == 1 && "Expected 1 custom action");
    assert(counts[KeymapParser::EntryType::SCRIPT_ACTION] == 1 && "Expected 1 script action");
    
    std::cout << "✓ Basic parsing test passed" << std::endl;
}

void testKeyBindingDetails() {
    std::cout << "Testing key binding details..." << std::endl;
    
    KeymapParser::KeymapParser parser;
    std::string testKeymap = "KEY 1 65 1007 0\n";
    
    parser.parseString(testKeymap);
    
    auto keyBindings = parser.getEntriesByType(KeymapParser::EntryType::KEY_BINDING);
    assert(keyBindings.size() == 1 && "Expected 1 key binding");
    
    auto* kb = static_cast<KeymapParser::KeyBinding*>(keyBindings[0]);
    assert(kb->getModifierValue() == 1 && "Expected modifier value 1");
    assert(kb->getKeyNoteValue() == 65 && "Expected key note value 65");
    assert(kb->getActionCommandId() == "1007" && "Expected action command ID 1007");
    assert(kb->getContext() == KeymapParser::Context::MAIN && "Expected main context");
    
    std::cout << "✓ Key binding details test passed" << std::endl;
}

void testMidiKeyBinding() {
    std::cout << "Testing MIDI key binding..." << std::endl;
    
    KeymapParser::KeymapParser parser;
    std::string testKeymap = "KEY 144 60 1007 0\n"; // MIDI Note C4 on channel 1
    
    parser.parseString(testKeymap);
    
    auto keyBindings = parser.getEntriesByType(KeymapParser::EntryType::KEY_BINDING);
    assert(keyBindings.size() == 1 && "Expected 1 key binding");
    
    auto* kb = static_cast<KeymapParser::KeyBinding*>(keyBindings[0]);
    assert(kb->getKeyType() == KeymapParser::KeyType::MIDI_NOTE && "Expected MIDI note type");
    assert(kb->getMidiChannel() == 1 && "Expected MIDI channel 1");
    assert(kb->getMidiNote() == 60 && "Expected MIDI note 60");
    
    std::string keyCombo = kb->getKeyComboString();
    assert(keyCombo.find("MIDI Chan 1 Note 60") != std::string::npos && "Expected MIDI description");
    
    std::cout << "✓ MIDI key binding test passed" << std::endl;
}

void testCustomAction() {
    std::cout << "Testing custom action..." << std::endl;
    
    KeymapParser::KeymapParser parser;
    std::string testKeymap = R"(ACT 3 0 "OSARA_REPORTFOCUS" "OSARA: Report focus" 40913)";
    
    parser.parseString(testKeymap);
    
    auto customActions = parser.getEntriesByType(KeymapParser::EntryType::CUSTOM_ACTION);
    assert(customActions.size() == 1 && "Expected 1 custom action");
    
    auto* ca = static_cast<KeymapParser::CustomAction*>(customActions[0]);
    assert(ca->getFlags() == 3 && "Expected flags 3");
    assert(ca->getContext() == KeymapParser::Context::MAIN && "Expected main context");
    assert(ca->getActionCommandId() == "OSARA_REPORTFOCUS" && "Expected action command ID");
    assert(ca->getDescription() == "OSARA: Report focus" && "Expected description");
    assert(ca->getCommandSequence().size() == 1 && "Expected 1 command");
    assert(ca->getCommandSequence()[0] == "40913" && "Expected command 40913");
    
    std::cout << "✓ Custom action test passed" << std::endl;
}

void testScriptAction() {
    std::cout << "Testing script action..." << std::endl;
    
    KeymapParser::KeymapParser parser;
    std::string testKeymap = R"(SCR 4 0 _OSARA_CONFIG_reportFocus "OSARA: Configuration for report focus*" osara_config.lua)";
    
    parser.parseString(testKeymap);
    
    auto scriptActions = parser.getEntriesByType(KeymapParser::EntryType::SCRIPT_ACTION);
    assert(scriptActions.size() == 1 && "Expected 1 script action");
    
    auto* sa = static_cast<KeymapParser::ScriptAction*>(scriptActions[0]);
    assert(sa->getBehavior() == KeymapParser::ScriptBehavior::SHOW_DIALOG && "Expected show dialog behavior");
    assert(sa->getContext() == KeymapParser::Context::MAIN && "Expected main context");
    assert(sa->getActionCommandId() == "_OSARA_CONFIG_reportFocus" && "Expected action command ID");
    assert(sa->getDescription() == "OSARA: Configuration for report focus*" && "Expected description");
    assert(sa->getScriptPath() == "osara_config.lua" && "Expected script path");
    
    std::cout << "✓ Script action test passed" << std::endl;
}

void testModification() {
    std::cout << "Testing keymap modification..." << std::endl;
    
    KeymapParser::KeymapParser parser;
    
    // Add a key binding
    parser.addKeyBinding(1, 65, "1007", KeymapParser::Context::MAIN);
    assert(parser.getEntryCount() == 1 && "Expected 1 entry after adding");
    assert(parser.isModified() && "Parser should be marked as modified");
    
    // Add a custom action
    parser.addCustomAction(3, KeymapParser::Context::MAIN, "TEST_ACTION", "Test Action", {"40913"});
    assert(parser.getEntryCount() == 2 && "Expected 2 entries after adding");
    
    // Remove an entry
    bool removed = parser.removeEntry("KEY_1_65_0");
    assert(removed && "Should have removed the key binding");
    assert(parser.getEntryCount() == 1 && "Expected 1 entry after removal");
    
    std::cout << "✓ Modification test passed" << std::endl;
}

void testSerialization() {
    std::cout << "Testing serialization..." << std::endl;
    
    KeymapParser::KeymapParser parser;
    parser.addKeyBinding(1, 65, "1007", KeymapParser::Context::MAIN);
    parser.addCustomAction(3, KeymapParser::Context::MAIN, "TEST_ACTION", "Test Action", {"40913"});
    
    std::string serialized = parser.toString();
    assert(!serialized.empty() && "Serialized string should not be empty");
    
    // Parse it back
    KeymapParser::KeymapParser parser2;
    bool success = parser2.parseString(serialized);
    assert(success && "Should be able to parse serialized keymap");
    assert(parser2.getEntryCount() == 2 && "Should have same number of entries");
    
    std::cout << "✓ Serialization test passed" << std::endl;
}

void testConflictDetection() {
    std::cout << "Testing conflict detection..." << std::endl;
    
    KeymapParser::KeymapParser parser;
    
    // Add two key bindings with the same key combination
    parser.addKeyBinding(1, 65, "1007", KeymapParser::Context::MAIN);
    parser.addKeyBinding(1, 65, "1008", KeymapParser::Context::MAIN);
    
    auto conflicts = parser.findConflictingKeyBindings();
    assert(conflicts.size() == 2 && "Should detect 2 conflicting key bindings");
    
    std::cout << "✓ Conflict detection test passed" << std::endl;
}

void testUtilities() {
    std::cout << "Testing utilities..." << std::endl;
    
    // Test context conversion
    std::string displayName = KeymapParser::KeymapUtils::contextToDisplayName(KeymapParser::Context::MIDI_EDITOR);
    assert(displayName == "MIDI Editor" && "Expected MIDI Editor display name");
    
    KeymapParser::Context context = KeymapParser::KeymapUtils::displayNameToContext("MIDI Editor");
    assert(context == KeymapParser::Context::MIDI_EDITOR && "Expected MIDI Editor context");
    
    // Test action command ID validation
    assert(KeymapParser::KeymapUtils::isValidActionCommandId("1007") && "Numeric ID should be valid");
    assert(KeymapParser::KeymapUtils::isValidActionCommandId("TEST_ACTION") && "String ID should be valid");
    assert(!KeymapParser::KeymapUtils::isValidActionCommandId("") && "Empty ID should be invalid");
    
    std::cout << "✓ Utilities test passed" << std::endl;
}

int main() {
    std::cout << "Running KeymapParser tests..." << std::endl << std::endl;
    
    try {
        testBasicParsing();
        testKeyBindingDetails();
        testMidiKeyBinding();
        testCustomAction();
        testScriptAction();
        testModification();
        testSerialization();
        testConflictDetection();
        testUtilities();
        
        std::cout << std::endl << "✅ All tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
