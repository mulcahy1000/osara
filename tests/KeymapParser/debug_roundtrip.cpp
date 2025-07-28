#include "KeymapParser.h"
#include <iostream>

int main() {
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
    if (!success1) {
        std::cout << "First parse failed!" << std::endl;
        return 1;
    }
    
    std::cout << "First parse - entries with comments:" << std::endl;
    const auto& entries1 = parser1.getEntries();
    for (size_t i = 0; i < entries1.size(); ++i) {
        std::string comment = entries1[i]->getComment();
        std::cout << "  [" << i << "] '" << comment << "' (length: " << comment.length() << ")" << std::endl;
    }
    
    // Write to string
    std::string serialized = parser1.toString();
    std::cout << "\nSerialized keymap:" << std::endl;
    std::cout << serialized << std::endl;
    
    // Second parse
    KeymapParser::KeymapParser parser2;
    bool success2 = parser2.parseString(serialized);
    if (!success2) {
        std::cout << "Second parse failed!" << std::endl;
        return 1;
    }
    
    std::cout << "Second parse - entries with comments:" << std::endl;
    const auto& entries2 = parser2.getEntries();
    for (size_t i = 0; i < entries2.size(); ++i) {
        std::string comment = entries2[i]->getComment();
        std::cout << "  [" << i << "] '" << comment << "' (length: " << comment.length() << ")" << std::endl;
    }
    
    // Compare
    std::cout << "\nComparison:" << std::endl;
    for (size_t i = 0; i < std::min(entries1.size(), entries2.size()); ++i) {
        std::string comment1 = entries1[i]->getComment();
        std::string comment2 = entries2[i]->getComment();
        
        if (comment1 == comment2) {
            std::cout << "  [" << i << "] ✓ Match" << std::endl;
        } else {
            std::cout << "  [" << i << "] ✗ Mismatch:" << std::endl;
            std::cout << "    Original: '" << comment1 << "'" << std::endl;
            std::cout << "    Round-trip: '" << comment2 << "'" << std::endl;
        }
    }
    
    return 0;
}
