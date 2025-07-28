#include "KeymapParser.h"
#include <iostream>

int main() {
    std::string testLine = R"(ACT 1 0 "test_action_with_quotes" "Custom: Action with \"quotes\" inside" 40001 40002)";
    
    std::cout << "Original line: " << testLine << std::endl;
    
    KeymapParser::KeymapParser parser;
    bool success = parser.parseString(testLine);
    
    if (!success) {
        std::cout << "Parse failed!" << std::endl;
        for (const auto& error : parser.getParseErrors()) {
            std::cout << "Error line " << error.first << ": " << error.second << std::endl;
        }
        return 1;
    }
    
    const auto& entries = parser.getEntries();
    if (entries.empty()) {
        std::cout << "No entries parsed!" << std::endl;
        return 1;
    }
    
    auto* customAction = dynamic_cast<KeymapParser::CustomAction*>(entries[0].get());
    if (!customAction) {
        std::cout << "Entry is not a CustomAction!" << std::endl;
        return 1;
    }
    
    std::cout << "Parsed action ID: '" << customAction->getActionCommandId() << "'" << std::endl;
    std::cout << "Parsed description: '" << customAction->getDescription() << "'" << std::endl;
    
    std::string serialized = parser.toString();
    std::cout << "Serialized: " << serialized << std::endl;
    
    // Parse again
    KeymapParser::KeymapParser parser2;
    bool success2 = parser2.parseString(serialized);
    
    if (!success2) {
        std::cout << "Second parse failed!" << std::endl;
        for (const auto& error : parser2.getParseErrors()) {
            std::cout << "Error line " << error.first << ": " << error.second << std::endl;
        }
        return 1;
    }
    
    const auto& entries2 = parser2.getEntries();
    if (entries2.empty()) {
        std::cout << "No entries in second parse!" << std::endl;
        return 1;
    }
    
    auto* customAction2 = dynamic_cast<KeymapParser::CustomAction*>(entries2[0].get());
    if (!customAction2) {
        std::cout << "Second entry is not a CustomAction!" << std::endl;
        return 1;
    }
    
    std::cout << "Second parsed action ID: '" << customAction2->getActionCommandId() << "'" << std::endl;
    std::cout << "Second parsed description: '" << customAction2->getDescription() << "'" << std::endl;
    
    if (customAction->getDescription() == customAction2->getDescription()) {
        std::cout << "✓ Descriptions match!" << std::endl;
    } else {
        std::cout << "✗ Descriptions don't match!" << std::endl;
        std::cout << "Original length: " << customAction->getDescription().length() << std::endl;
        std::cout << "Second length: " << customAction2->getDescription().length() << std::endl;
    }
    
    return 0;
}
