#include "KeymapParser.h"
#include <iostream>

int main() {
    std::string testKeymap = R"(KEY 1 65 1007 0		 # Main : A : Some action)";
    
    std::cout << "Original line: '" << testKeymap << "'" << std::endl;
    
    KeymapParser::KeymapParser parser;
    bool success = parser.parseString(testKeymap);
    
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
    
    std::cout << "Number of entries: " << entries.size() << std::endl;
    
    const auto& entry = entries[0];
    std::string comment = entry->getComment();
    
    std::cout << "Comment: '" << comment << "'" << std::endl;
    std::cout << "Comment length: " << comment.length() << std::endl;
    std::cout << "Expected: ' # Main : A : Some action'" << std::endl;
    std::cout << "Expected length: " << std::string(" # Main : A : Some action").length() << std::endl;
    
    if (comment == " # Main : A : Some action") {
        std::cout << "✓ Comments match!" << std::endl;
    } else {
        std::cout << "✗ Comments don't match!" << std::endl;
        
        // Print character by character comparison
        std::string expected = " # Main : A : Some action";
        std::cout << "Character comparison:" << std::endl;
        size_t maxLen = std::max(comment.length(), expected.length());
        for (size_t i = 0; i < maxLen; ++i) {
            char actual = (i < comment.length()) ? comment[i] : '?';
            char exp = (i < expected.length()) ? expected[i] : '?';
            std::cout << "  [" << i << "] actual: '" << actual << "' (" << (int)actual << ") expected: '" << exp << "' (" << (int)exp << ")" << std::endl;
        }
    }
    
    return 0;
}
