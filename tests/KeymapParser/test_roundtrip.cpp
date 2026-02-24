#include "KeymapParser.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>

void testRoundTripWithComments() {
    std::cout << "Testing round-trip with comments..." << std::endl;
    
    // Create a test keymap with various entry types and comments
    std::string testKeymap = R"(ACT 1 0 "9ce484e08ce236468c6e6e9ed2916fe9" "Custom: Edit marker at cursor" 40614 41988
ACT 1 0 "a77ae1752661af4bb75a473af340ff6a" "Custom: Move to item peak and report the position" _SWS_FINDITEMPEAK 1016 _OSARA_CURSORPOS
SCR 4 0 RS1cbf05b0c4f875518496f34a5ce45adefe05cb67 "Custom: Default_6.0_theme_adjuster.lua" Cockos/Default_6.0_theme_adjuster.lua
KEY 1 189 1011 0		 # Main : - : View: Zoom out horizontal
KEY 1 187 1012 0		 # Main : = : View: Zoom in horizontal
KEY 1 32781 1013 0		 # Main : NUM ENTER : Transport: Record
KEY 1 82 1013 0		 # Main : R : OVERRIDE DEFAULT : Transport: Record
KEY 5 13 1041 0		 # Main : Shift+ENTER : Track: Cycle track folder state
KEY 1 13 1042 0		 # Main : ENTER : OVERRIDE DEFAULT : Track: Cycle folder collapsed state
KEY 9 82 1068 0		 # Main : Ctrl+R : OVERRIDE DEFAULT : Transport: Toggle repeat
KEY 1 72 1134 0		 # Main : H : Transport: Tap tempo
KEY 1 76 1135 0		 # Main : L : OVERRIDE DEFAULT : Options: Toggle locking
KEY 21 80 1155 0		 # Main : Alt+Shift+P : Options: Cycle ripple editing mode
KEY 25 71 1156 0		 # Main : Ctrl+Alt+G : OVERRIDE DEFAULT : Options: Toggle item grouping and track media/razor edit grouping
KEY 17 78 1157 0		 # Main : Alt+N : Options: Toggle snapping
KEY 5 116 14 0		 # Main : Shift+F5 : OVERRIDE DEFAULT : Track: Toggle mute for master track
KEY 5 66 16 0		 # Main : Shift+B : Track: Toggle FX bypass for master track
KEY 1 112 40007 0		 # Main : F1 : Help: About REAPER
KEY 0 123 65535 0		 # Main : { : OVERRIDE DEFAULT : No-op (no action)
KEY 0 125 65535 0		 # Main : } : OVERRIDE DEFAULT : No-op (no action)
KEY 255 3048 65535 0		 # Main : MediaKbd+Track+ : OVERRIDE DEFAULT : No-op (no action)
KEY 255 3304 65535 0		 # Main : MediaKbd+Track- : OVERRIDE DEFAULT : No-op (no action)
KEY 1 117 7 0		 # Main : F6 : OVERRIDE DEFAULT : Track: Toggle solo for selected tracks
)";
    
    // First parse
    KeymapParser::KeymapParser parser1;
    bool success1 = parser1.parseString(testKeymap);
    assert(success1 && "First parse should succeed");
    
    // Write to string
    std::string serialized = parser1.toString();
    
    // Second parse
    KeymapParser::KeymapParser parser2;
    bool success2 = parser2.parseString(serialized);
    assert(success2 && "Second parse should succeed");
    
    // Compare entry counts
    assert(parser1.getEntryCount() == parser2.getEntryCount() && 
           "Entry counts should match after round-trip");
    
    // Compare each entry
    const auto& entries1 = parser1.getEntries();
    const auto& entries2 = parser2.getEntries();
    
    for (size_t i = 0; i < entries1.size(); ++i) {
        const auto& entry1 = entries1[i];
        const auto& entry2 = entries2[i];

        // Check basic properties
        assert(entry1->getType() == entry2->getType() &&
               "Entry types should match");
        assert(entry1->getUniqueId() == entry2->getUniqueId() &&
               "Unique IDs should match");
        assert(entry1->getContext() == entry2->getContext() &&
               "Contexts should match");

        // Extract just the comment text (after the #)
        auto extractCommentText = [](const std::string& comment) -> std::string {
            size_t hashPos = comment.find('#');
            if (hashPos != std::string::npos) {
                return comment.substr(hashPos);
            }
            return comment;
        };

        std::string comment1 = entry1->getComment();
        std::string comment2 = entry2->getComment();

        // If the original entry had a comment, it must be preserved verbatim.
        // Entries that originally had no comment may gain an auto-generated one
        // on round-trip (toString() emits a description comment for uncommented
        // entries), so we only enforce equality when a comment was present.
        if (!comment1.empty()) {
            std::string commentText1 = extractCommentText(comment1);
            std::string commentText2 = extractCommentText(comment2);
            assert(commentText1 == commentText2 &&
                   "Comment content should be preserved");
        }
    }

    std::cout << "✓ Round-trip with comments test passed" << std::endl;
}

void testRoundTripWithComplexQuotedStrings() {
    std::cout << "Testing round-trip with complex quoted strings..." << std::endl;
    
    // Test keymap with complex quoted strings that might cause parsing issues
    std::string testKeymap = R"(ACT 1 0 "test_action_with_quotes" "Custom: Action with \"quotes\" inside" 40001 40002
ACT 1 0 "path_with_spaces" "Custom: Script with spaces in path" 40003
SCR 4 0 RS123abc "Custom: Script with complex path" "ReaTeam Scripts/FX/BuyOne_Insert selected FX or FX chain presets in OR copy focused FX to selected objects.lua"
SCR 4 0 RS456def "Custom: Another complex script" "X-Raym Scripts/Regions/X-Raym_Split region under cursor.eel"
KEY 1 65 test_action_with_quotes 0		 # Main : A : Custom action with quotes
KEY 1 66 path_with_spaces 0		 # Main : B : Action with spaces
)";
    
    // First parse
    KeymapParser::KeymapParser parser1;
    bool success1 = parser1.parseString(testKeymap);
    assert(success1 && "First parse should succeed");
    
    // Write to string
    std::string serialized = parser1.toString();
    
    // Second parse
    KeymapParser::KeymapParser parser2;
    bool success2 = parser2.parseString(serialized);
    assert(success2 && "Second parse should succeed");
    
    // Verify specific entries
    auto customActions1 = parser1.getEntriesByType(KeymapParser::EntryType::CUSTOM_ACTION);
    auto customActions2 = parser2.getEntriesByType(KeymapParser::EntryType::CUSTOM_ACTION);
    
    assert(customActions1.size() == customActions2.size() && 
           "Custom action counts should match");
    
    // Check that quoted strings are handled correctly
    for (size_t i = 0; i < customActions1.size(); ++i) {
        auto* ca1 = static_cast<KeymapParser::CustomAction*>(customActions1[i]);
        auto* ca2 = static_cast<KeymapParser::CustomAction*>(customActions2[i]);
        
        assert(ca1->getActionCommandId() == ca2->getActionCommandId() && 
               "Action command IDs should match");
        assert(ca1->getDescription() == ca2->getDescription() && 
               "Descriptions should match");
    }
    
    std::cout << "✓ Round-trip with complex quoted strings test passed" << std::endl;
}

void testRoundTripWithRealKeymapFiles() {
    std::cout << "Testing round-trip with real keymap files..." << std::endl;
    
    // Test both Mac and Windows keymap files
    std::vector<std::string> keymapPaths = {
        "../config/mac/reaper-kb.ini",
        "../config/windows/reaper-kb.ini"
    };
    
    for (const auto& keymapPath : keymapPaths) {
        std::cout << "  Testing " << keymapPath << "..." << std::endl;
        
        // Check if file exists
        std::ifstream testFile(keymapPath);
        if (!testFile.is_open()) {
            std::cout << "    ⚠ Skipping - file not found: " << keymapPath << std::endl;
            continue;
        }
        testFile.close();
        
        // First parse
        KeymapParser::KeymapParser parser1;
        bool success1 = parser1.parseFile(keymapPath);
        
        if (!success1) {
            std::cout << "    Parse errors:" << std::endl;
            for (const auto& error : parser1.getParseErrors()) {
                std::cout << "      Line " << error.first << ": " << error.second << std::endl;
            }
            assert(false && ("First parse should succeed for " + keymapPath).c_str());
        }
        
        size_t originalEntryCount = parser1.getEntryCount();
        std::cout << "    Original keymap has " << originalEntryCount << " entries" << std::endl;
        
        // Write to temporary file
        std::string tempPath = "test_roundtrip_temp_" + 
                              keymapPath.substr(keymapPath.find_last_of('/') + 1);
        bool writeSuccess = parser1.writeToFile(tempPath);
        assert(writeSuccess && ("Write to temp file should succeed for " + keymapPath).c_str());
        
        // Second parse
        KeymapParser::KeymapParser parser2;
        bool success2 = parser2.parseFile(tempPath);
        
        if (!success2) {
            std::cout << "    Parse errors on second parse:" << std::endl;
            for (const auto& error : parser2.getParseErrors()) {
                std::cout << "      Line " << error.first << ": " << error.second << std::endl;
            }
            assert(false && ("Second parse should succeed for " + keymapPath).c_str());
        }
        
        // Compare entry counts
        size_t newEntryCount = parser2.getEntryCount();
        std::cout << "    Round-trip keymap has " << newEntryCount << " entries" << std::endl;
        
        assert(originalEntryCount == newEntryCount && 
               ("Entry counts should match after round-trip for " + keymapPath).c_str());
        
        // Compare entry type distributions
        auto counts1 = parser1.getEntryCountsByType();
        auto counts2 = parser2.getEntryCountsByType();
        
        for (const auto& pair : counts1) {
            assert(counts2[pair.first] == pair.second && 
                   ("Entry type counts should match for " + keymapPath).c_str());
        }
        
        // Detailed comparison of a sample of entries to ensure semantic preservation
        const auto& entries1 = parser1.getEntries();
        const auto& entries2 = parser2.getEntries();
        
        // Test first 10 entries (or all if fewer than 10)
        size_t samplesToTest = std::min(static_cast<size_t>(10), entries1.size());
        auto extractCommentText = [](const std::string& comment) -> std::string {
            size_t hashPos = comment.find('#');
            if (hashPos != std::string::npos) return comment.substr(hashPos);
            return comment;
        };

        for (size_t i = 0; i < samplesToTest; ++i) {
            const auto& entry1 = entries1[i];
            const auto& entry2 = entries2[i];

            assert(entry1->getType() == entry2->getType() &&
                   ("Entry types should match at index " + std::to_string(i) + " for " + keymapPath).c_str());
            assert(entry1->getUniqueId() == entry2->getUniqueId() &&
                   ("Unique IDs should match at index " + std::to_string(i) + " for " + keymapPath).c_str());
            assert(entry1->getContext() == entry2->getContext() &&
                   ("Contexts should match at index " + std::to_string(i) + " for " + keymapPath).c_str());

            // Only check comment preservation when the original had a comment.
            // Entries without comments gain an auto-generated one on round-trip.
            std::string comment1 = entry1->getComment();
            std::string comment2 = entry2->getComment();
            if (!comment1.empty()) {
                assert(extractCommentText(comment1) == extractCommentText(comment2) &&
                       ("Comment content should be preserved at index " + std::to_string(i) + " for " + keymapPath).c_str());
            }
        }
        
        // Clean up temp file
        std::remove(tempPath.c_str());
        
        std::cout << "    ✓ Round-trip test passed for " << keymapPath << std::endl;
    }
    
    std::cout << "✓ Round-trip with real keymap files test passed" << std::endl;
}

void testCommentPreservation() {
    std::cout << "Testing comment preservation..." << std::endl;
    
    std::string testKeymap = R"(KEY 1 65 1007 0		 # Main : A : Some action
KEY 1 66 1008 0		 # Main : B : OVERRIDE DEFAULT : Another action
ACT 1 0 "TEST_ACTION" "Test Action" 40001		 # Custom action comment
SCR 4 0 RS123 "Test Script" test.lua		 # Script action comment
)";
    
    KeymapParser::KeymapParser parser;
    bool success = parser.parseString(testKeymap);
    assert(success && "Parse should succeed");
    
    const auto& entries = parser.getEntries();
    assert(entries.size() == 4 && "Should have 4 entries");
    
    // Check that all entries have comments
    for (const auto& entry : entries) {
        std::string comment = entry->getComment();
        assert(!comment.empty() && "All entries should have comments");
        assert(comment.find('#') != std::string::npos && "Comments should contain #");
    }
    
    // Check specific comments (note: comments include the whitespace before #)
    assert(entries[0]->getComment() == "\t\t # Main : A : Some action" && 
           "First comment should match");
    assert(entries[1]->getComment() == "\t\t # Main : B : OVERRIDE DEFAULT : Another action" && 
           "Second comment should match");
    assert(entries[2]->getComment() == "\t\t # Custom action comment" && 
           "Third comment should match");
    assert(entries[3]->getComment() == "\t\t # Script action comment" && 
           "Fourth comment should match");
    
    // Test that comments are included in toString output
    std::string serialized = parser.toString();
    assert(serialized.find("# Main : A : Some action") != std::string::npos && 
           "Serialized output should contain first comment");
    assert(serialized.find("# Custom action comment") != std::string::npos && 
           "Serialized output should contain custom action comment");
    
    std::cout << "✓ Comment preservation test passed" << std::endl;
}

void testEmptyAndMalformedEntries() {
    std::cout << "Testing empty and malformed entries..." << std::endl;
    
    std::string testKeymap = R"(
# This is a comment line
KEY 1 65 1007 0		 # Valid entry with comment

# Empty line above and below

KEY 1 66 1008 0		 # Another valid entry
INVALID_TYPE 1 2 3 4		 # This should be ignored
KEY incomplete line		 # This should cause an error
KEY 1 67 1009 0		 # Valid entry after error
)";
    
    KeymapParser::KeymapParser parser;
    bool success = parser.parseString(testKeymap);
    
    // Should succeed overall but have some parse errors
    assert(parser.hasErrors() && "Should have parse errors");
    
    // Should still parse the valid entries
    assert(parser.getEntryCount() == 3 && "Should have 3 valid entries");
    
    // Check that valid entries have comments
    const auto& entries = parser.getEntries();
    for (const auto& entry : entries) {
        assert(!entry->getComment().empty() && "Valid entries should have comments");
    }
    
    std::cout << "✓ Empty and malformed entries test passed" << std::endl;
}

int main() {
    std::cout << "Running KeymapParser round-trip tests..." << std::endl << std::endl;
    
    try {
        testRoundTripWithComments();
        testRoundTripWithComplexQuotedStrings();
        testRoundTripWithRealKeymapFiles();
        testCommentPreservation();
        testEmptyAndMalformedEntries();
        
        std::cout << std::endl << "✅ All round-trip tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
