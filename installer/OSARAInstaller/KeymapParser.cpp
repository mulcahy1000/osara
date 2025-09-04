    #include "KeymapParser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

namespace KeymapParser {

// ============================================================================
// KeymapEntry Base Class Implementation
// ============================================================================

bool KeymapEntry::operator==(const KeymapEntry& other) const {
    return getType() == other.getType() && 
           getUniqueId() == other.getUniqueId() &&
           getContext() == other.getContext();
}

// ============================================================================
// KeyBinding Implementation
// ============================================================================

KeyBinding::KeyBinding(int modifierValue, int keyNoteValue, const std::string& actionCommandId, Context context)
    : modifierValue(modifierValue), keyNoteValue(keyNoteValue), actionCommandId(actionCommandId), context(context) {
    determineKeyType();
}

void KeyBinding::determineKeyType() {
    // Based on Reaper specification
    if (modifierValue >= 144 && modifierValue <= 159) {
        keyType = KeyType::MIDI_NOTE;
    } else if (modifierValue >= 176 && modifierValue <= 191) {
        keyType = KeyType::MIDI_CC;
    } else if (modifierValue >= 192 && modifierValue <= 207) {
        keyType = KeyType::MIDI_PC;
    } else if (modifierValue >= 224 && modifierValue <= 239) {
        keyType = KeyType::MIDI_PITCH;
    } else if ((modifierValue >= 128 && modifierValue <= 143) ||
               (modifierValue >= 160 && modifierValue <= 175) ||
               (modifierValue >= 208 && modifierValue <= 223) ||
               (modifierValue >= 240 && modifierValue <= 254)) {
        keyType = KeyType::MIDI_RAW;
    } else if (modifierValue == 255) {
        // Check for special media keyboard or multitouch keys
        if (keyNoteValue >= 232) {
            keyType = KeyType::MEDIA_KEYBOARD;
        } else if ((keyNoteValue >= 72 && keyNoteValue <= 73) ||
                   (keyNoteValue >= 200 && keyNoteValue <= 207)) {
            keyType = KeyType::MULTITOUCH;
        } else if ((keyNoteValue >= 120 && keyNoteValue <= 125) ||
                   (keyNoteValue >= 248 && keyNoteValue <= 255)) {
            keyType = KeyType::MOUSEWHEEL;
        } else {
            keyType = KeyType::UNKNOWN_KEY_TYPE;
        }
    } else {
        keyType = KeyType::REGULAR_KEY;
    }
}

std::string KeyBinding::toString() const {
    std::ostringstream oss;
    oss << "KEY " << modifierValue << " " << keyNoteValue << " ";
    
    if (actionCommandId.empty() || std::isdigit(actionCommandId[0])) {
        oss << actionCommandId;
    } else {
        oss << "_" << actionCommandId;
    }
    
    oss << " " << static_cast<int>(context);
    
    // Add comment if present
    if (!comment.empty()) {
        oss << "\t\t" << comment;
    }
    
    return oss.str();
}

std::string KeyBinding::getDescription() const {
    return getKeyComboString() + " -> " + actionCommandId;
}

bool KeyBinding::isConflictWith(const KeymapEntry& other) const {
    if (other.getType() != EntryType::KEY_BINDING) {
        return false;
    }
    
    const KeyBinding* otherBinding = static_cast<const KeyBinding*>(&other);
    return hasSameKeyCombo(*otherBinding) && context == otherBinding->context;
}

std::string KeyBinding::getUniqueId() const {
    std::ostringstream oss;
    oss << "KEY_" << modifierValue << "_" << keyNoteValue << "_" << static_cast<int>(context);
    return oss.str();
}

std::string KeyBinding::getKeyComboString() const {
    switch (keyType) {
        case KeyType::MIDI_NOTE:
        case KeyType::MIDI_CC:
        case KeyType::MIDI_PC:
        case KeyType::MIDI_PITCH:
        case KeyType::MIDI_RAW:
            return formatMidiKey();
        case KeyType::MEDIA_KEYBOARD:
            return formatMediaKey();
        case KeyType::MULTITOUCH:
        case KeyType::MOUSEWHEEL:
            return formatMediaKey(); // Same formatting for now
        case KeyType::REGULAR_KEY:
        default:
            return formatRegularKey();
    }
}

std::string KeyBinding::getModifierString() const {
    if (keyType != KeyType::REGULAR_KEY) {
        return ""; // MIDI and special keys handle modifiers differently
    }
    
    std::vector<std::string> modifiers;
    
    // Based on Reaper specification for regular keys
    bool isOddModifier = (modifierValue % 2) == 1;
    
    if (isOddModifier) {
        // Odd modifier values include the modifier in the key value
        // This is complex - simplified for now
        if (modifierValue & 1) modifiers.push_back("Shift");
        if (modifierValue & 2) modifiers.push_back("Ctrl");
        if (modifierValue & 4) modifiers.push_back("Alt");
        if (modifierValue & 8) modifiers.push_back("Win");
    } else {
        // Even modifier values
        if (modifierValue & 4) modifiers.push_back("Shift");
        if (modifierValue & 8) modifiers.push_back("Ctrl");
        if (modifierValue & 16) modifiers.push_back("Alt");
        if (modifierValue & 32) modifiers.push_back("Win");
    }
    
    std::string result;
    for (size_t i = 0; i < modifiers.size(); ++i) {
        if (i > 0) result += "+";
        result += modifiers[i];
    }
    return result;
}

std::string KeyBinding::getKeyString() const {
    if (keyType != KeyType::REGULAR_KEY) {
        return getKeyComboString(); // For non-regular keys, the combo string is the key
    }
    
    // Special key values
    if (keyNoteValue >= 32801 && keyNoteValue <= 32815) {
        switch (keyNoteValue) {
            case 32801: return "Page Up";
            case 32802: return "Page Down";
            case 32803: return "End";
            case 32804: return "Home";
            case 32805: return "Left";
            case 32806: return "Up";
            case 32807: return "Right";
            case 32808: return "Down";
            case 32813: return "Insert";
            case 32814: return "Delete";
            case 32781: return "NumPad Enter";
            default: return "Special Key " + std::to_string(keyNoteValue);
        }
    }
    
    // ASCII keys
    if (keyNoteValue >= 32 && keyNoteValue <= 126) {
        char c = static_cast<char>(keyNoteValue);
        if (std::isalnum(c) || std::ispunct(c)) {
            return std::string(1, c);
        }
    }
    
    // Function keys and other special keys
    if (keyNoteValue >= 112 && keyNoteValue <= 123) {
        return "F" + std::to_string(keyNoteValue - 111);
    }
    
    return "Key " + std::to_string(keyNoteValue);
}

bool KeyBinding::hasSameKeyCombo(const KeyBinding& other) const {
    return modifierValue == other.modifierValue && keyNoteValue == other.keyNoteValue;
}

int KeyBinding::getMidiChannel() const {
    if (keyType == KeyType::MIDI_NOTE) {
        return modifierValue - 144 + 1; // Channels 1-16
    } else if (keyType == KeyType::MIDI_CC) {
        return modifierValue - 176 + 1;
    } else if (keyType == KeyType::MIDI_PC) {
        return modifierValue - 192 + 1;
    } else if (keyType == KeyType::MIDI_PITCH) {
        return modifierValue - 224 + 1;
    }
    return -1;
}

int KeyBinding::getMidiNote() const {
    if (keyType == KeyType::MIDI_NOTE || keyType == KeyType::MIDI_CC || keyType == KeyType::MIDI_PC) {
        return keyNoteValue % 127; // Notes wrap at 127
    }
    return -1;
}

int KeyBinding::getMidiCC() const {
    if (keyType == KeyType::MIDI_CC) {
        return keyNoteValue % 127;
    }
    return -1;
}

std::string KeyBinding::formatMidiKey() const {
    std::ostringstream oss;
    
    switch (keyType) {
        case KeyType::MIDI_NOTE:
            oss << "MIDI Chan " << getMidiChannel() << " Note " << getMidiNote();
            break;
        case KeyType::MIDI_CC:
            oss << "MIDI Chan " << getMidiChannel() << " CC " << getMidiCC();
            break;
        case KeyType::MIDI_PC:
            oss << "MIDI Chan " << getMidiChannel() << " PC " << getMidiNote();
            break;
        case KeyType::MIDI_PITCH:
            oss << "MIDI Chan " << getMidiChannel() << " Pitch";
            break;
        case KeyType::MIDI_RAW:
            oss << "MIDI Raw " << std::hex << modifierValue << " " << keyNoteValue;
            break;
        default:
            oss << "MIDI Unknown";
            break;
    }
    
    return oss.str();
}

std::string KeyBinding::formatMediaKey() const {
    if (keyType == KeyType::MEDIA_KEYBOARD) {
        // Media keyboard key names based on specification
        switch (keyNoteValue) {
            case 488: return "MediaKbd Browse-";
            case 744: return "MediaKbd Browse+";
            case 1000: return "MediaKbd Browse Refresh";
            case 2280: return "MediaKbd Mute";
            case 2536: return "MediaKbd Vol-";
            case 2792: return "MediaKbd Vol+";
            case 3560: return "MediaKbd Stop";
            case 3816: return "MediaKbd Play/Pause";
            case 4072: return "MediaKbd Mail";
            default: return "MediaKbd " + std::to_string(keyNoteValue);
        }
    } else if (keyType == KeyType::MOUSEWHEEL) {
        if (keyNoteValue == 120 || keyNoteValue == 248) {
            return "Mousewheel";
        } else if (keyNoteValue == 88 || keyNoteValue == 216) {
            return "Horizontal Mousewheel";
        }
    } else if (keyType == KeyType::MULTITOUCH) {
        if (keyNoteValue == 72 || keyNoteValue == 200) {
            return "MultiZoom";
        } else if (keyNoteValue == 24 || keyNoteValue == 152) {
            return "MultiRotate";
        }
    }
    
    return "Special " + std::to_string(keyNoteValue);
}

std::string KeyBinding::formatRegularKey() const {
    std::string modifier = getModifierString();
    std::string key = getKeyString();
    
    if (modifier.empty()) {
        return key;
    } else {
        return modifier + "+" + key;
    }
}

// ============================================================================
// GlobalKeyBinding Implementation
// ============================================================================

GlobalKeyBinding::GlobalKeyBinding(std::unique_ptr<KeyBinding> actionBinding,
                                 std::unique_ptr<KeyBinding> scopeBinding,
                                 GlobalKeyScope scope)
    : actionBinding(std::move(actionBinding)), scopeBinding(std::move(scopeBinding)), scope(scope) {
}

std::string GlobalKeyBinding::toString() const {
    return actionBinding->toString() + "\n" + scopeBinding->toString();
}

std::string GlobalKeyBinding::getDescription() const {
    std::string scopeDesc = (scope == GlobalKeyScope::GLOBAL) ? "Global" : "Global+TextFields";
    return getKeyComboString() + " -> " + actionBinding->getActionCommandId() + " (" + scopeDesc + ")";
}

Context GlobalKeyBinding::getContext() const {
    return actionBinding->getContext();
}

bool GlobalKeyBinding::isConflictWith(const KeymapEntry& other) const {
    if (other.getType() != EntryType::GLOBAL_KEY_BINDING) {
        return false;
    }
    
    const GlobalKeyBinding* otherGlobal = static_cast<const GlobalKeyBinding*>(&other);
    return actionBinding->hasSameKeyCombo(*otherGlobal->actionBinding) &&
           getContext() == otherGlobal->getContext();
}

std::string GlobalKeyBinding::getUniqueId() const {
    return "GLOBAL_" + actionBinding->getUniqueId();
}

std::string GlobalKeyBinding::getKeyComboString() const {
    return actionBinding->getKeyComboString();
}

// ============================================================================
// CustomAction Implementation
// ============================================================================

CustomAction::CustomAction(int flags, Context context, const std::string& actionCommandId,
                          const std::string& description, const std::vector<std::string>& commands)
    : flags(flags), context(context), actionCommandId(actionCommandId), 
      description(description), commandSequence(commands) {
}

std::string CustomAction::toString() const {
    std::ostringstream oss;
    
    // Helper function to escape quotes in strings
    auto escapeQuotes = [](const std::string& str) {
        std::string escaped;
        for (char c : str) {
            if (c == '"') {
                escaped += "\\\"";
            } else if (c == '\\') {
                escaped += "\\\\";
            } else {
                escaped += c;
            }
        }
        return escaped;
    };
    
    oss << "ACT " << flags << " " << static_cast<int>(context) 
        << " \"" << escapeQuotes(actionCommandId) << "\" \"" << escapeQuotes(description) << "\"";
    
    for (const auto& command : commandSequence) {
        oss << " " << command;
    }
    
    // Add comment if present
    if (!comment.empty()) {
        oss << "\t\t" << comment;
    }
    
    return oss.str();
}

bool CustomAction::isConflictWith(const KeymapEntry& other) const {
    if (other.getType() != EntryType::CUSTOM_ACTION) {
        return false;
    }
    
    const CustomAction* otherAction = static_cast<const CustomAction*>(&other);
    return actionCommandId == otherAction->actionCommandId && context == otherAction->context;
}

// ============================================================================
// ScriptAction Implementation
// ============================================================================

ScriptAction::ScriptAction(ScriptBehavior behavior, Context context, const std::string& actionCommandId,
                          const std::string& description, const std::string& scriptPath)
    : behavior(behavior), context(context), actionCommandId(actionCommandId),
      description(description), scriptPath(scriptPath) {
}

std::string ScriptAction::toString() const {
    std::ostringstream oss;
    oss << "SCR " << static_cast<int>(behavior) << " " << static_cast<int>(context) 
        << " " << actionCommandId << " \"" << description << "\" " << scriptPath;
    
    // Add comment if present
    if (!comment.empty()) {
        oss << "\t\t" << comment;
    }
    
    return oss.str();
}

bool ScriptAction::isConflictWith(const KeymapEntry& other) const {
    if (other.getType() != EntryType::SCRIPT_ACTION) {
        return false;
    }
    
    const ScriptAction* otherScript = static_cast<const ScriptAction*>(&other);
    return actionCommandId == otherScript->actionCommandId && context == otherScript->context;
}

bool ScriptAction::consolidatesUndoPoints() const {
    return (static_cast<int>(behavior) & 1) != 0;
}

bool ScriptAction::showsInActionsMenu() const {
    return (static_cast<int>(behavior) & 2) != 0;
}

std::string ScriptAction::getBehaviorDescription() const {
    switch (behavior) {
        case ScriptBehavior::SHOW_DIALOG:
            return "Show dialog if instance running";
        case ScriptBehavior::TERMINATE_ALL:
            return "Always terminate all instances";
        case ScriptBehavior::NEW_INSTANCE:
            return "Always start new instance";
        default:
            return "Unknown behavior";
    }
}

// ============================================================================
// KeymapParser Implementation
// ============================================================================

bool KeymapParser::parseFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        addParseError(0, "Could not open file: " + filePath);
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    return parseString(content);
}

bool KeymapParser::parseString(const std::string& content) {
    clear();
    
    std::istringstream stream(content);
    std::string line;
    int lineNumber = 0;
    
    while (std::getline(stream, line)) {
        ++lineNumber;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        auto entry = parseLine(line, lineNumber);
        if (entry) {
            entries.push_back(std::move(entry));
        }
    }
    
    // Post-process to detect global shortcuts
    std::vector<std::unique_ptr<KeymapEntry>> processedEntries;
    
    for (size_t i = 0; i < entries.size(); ++i) {
        if (isGlobalShortcutPair(i)) {
            auto globalBinding = parseGlobalShortcut(i);
            if (globalBinding) {
                processedEntries.push_back(std::move(globalBinding));
                ++i; // Skip the next entry as it's part of the global shortcut
            }
        } else {
            processedEntries.push_back(std::move(entries[i]));
        }
    }
    
    entries = std::move(processedEntries);
    updateActionCommandIdCounts();
    markAsClean();
    
    return !hasErrors();
}

std::unique_ptr<KeymapEntry> KeymapParser::parseLine(const std::string& line, int lineNumber) {
    // Extract comment first (everything after #, including preceding whitespace)
    std::string entryLine = line;
    std::string comment;
    
    size_t commentPos = line.find('#');
    if (commentPos != std::string::npos) {
        // Find the start of whitespace before the #
        size_t whitespaceStart = commentPos;
        while (whitespaceStart > 0 && std::isspace(line[whitespaceStart - 1])) {
            whitespaceStart--;
        }
        
        entryLine = line.substr(0, whitespaceStart);
        comment = line.substr(whitespaceStart);
    }
    
    auto tokens = tokenizeLine(entryLine);
    if (tokens.empty()) {
        return nullptr;
    }
    
    const std::string& type = tokens[0];
    std::unique_ptr<KeymapEntry> entry;
    
    if (type == "KEY") {
        entry = parseKeyLine(entryLine, lineNumber);
    } else if (type == "ACT") {
        entry = parseActLine(entryLine, lineNumber);
    } else if (type == "SCR") {
        entry = parseScrLine(entryLine, lineNumber);
    } else {
        addParseError(lineNumber, "Unknown entry type: " + type);
        return nullptr;
    }
    
    // Set comment if we found one and successfully parsed the entry
    if (entry && !comment.empty()) {
        entry->setComment(comment);
    }
    
    return entry;
}

std::unique_ptr<KeyBinding> KeymapParser::parseKeyLine(const std::string& line, int lineNumber) {
    auto tokens = tokenizeLine(line);
    if (tokens.size() < 4) {
        addParseError(lineNumber, "KEY line requires at least 4 tokens");
        return nullptr;
    }
    
    try {
        int modifierValue = std::stoi(tokens[1]);
        int keyNoteValue = std::stoi(tokens[2]);
        std::string actionCommandId = tokens[3];
        
        // Remove leading underscore if present
        if (!actionCommandId.empty() && actionCommandId[0] == '_') {
            actionCommandId = actionCommandId.substr(1);
        }
        
        Context context = Context::MAIN;
        if (tokens.size() > 4) {
            context = parseContext(std::stoi(tokens[4]));
        }
        
        return std::make_unique<KeyBinding>(modifierValue, keyNoteValue, actionCommandId, context);
        
    } catch (const std::exception& e) {
        addParseError(lineNumber, "Error parsing KEY line: " + std::string(e.what()));
        return nullptr;
    }
}

std::unique_ptr<CustomAction> KeymapParser::parseActLine(const std::string& line, int lineNumber) {
    auto tokens = tokenizeLine(line);
    if (tokens.size() < 5) {
        addParseError(lineNumber, "ACT line requires at least 5 tokens");
        return nullptr;
    }
    
    try {
        int flags = std::stoi(tokens[1]);
        Context context = parseContext(std::stoi(tokens[2]));
        std::string actionCommandId = tokens[3];
        std::string description = tokens[4];
        
        // Remove quotes from actionCommandId and description
        if (actionCommandId.size() >= 2 && actionCommandId.front() == '"' && actionCommandId.back() == '"') {
            actionCommandId = actionCommandId.substr(1, actionCommandId.size() - 2);
        }
        if (description.size() >= 2 && description.front() == '"' && description.back() == '"') {
            description = description.substr(1, description.size() - 2);
        }
        
        std::vector<std::string> commands;
        for (size_t i = 5; i < tokens.size(); ++i) {
            commands.push_back(tokens[i]);
        }
        
        return std::make_unique<CustomAction>(flags, context, actionCommandId, description, commands);
        
    } catch (const std::exception& e) {
        addParseError(lineNumber, "Error parsing ACT line: " + std::string(e.what()));
        return nullptr;
    }
}

std::unique_ptr<ScriptAction> KeymapParser::parseScrLine(const std::string& line, int lineNumber) {
    auto tokens = tokenizeLine(line);
    if (tokens.size() < 6) {
        addParseError(lineNumber, "SCR line requires at least 6 tokens");
        return nullptr;
    }
    
    try {
        ScriptBehavior behavior = static_cast<ScriptBehavior>(std::stoi(tokens[1]));
        Context context = parseContext(std::stoi(tokens[2]));
        std::string actionCommandId = tokens[3];
        std::string description = tokens[4];
        std::string scriptPath = tokens[5];
        
        // Remove quotes from description
        if (description.size() >= 2 && description.front() == '"' && description.back() == '"') {
            description = description.substr(1, description.size() - 2);
        }
        
        return std::make_unique<ScriptAction>(behavior, context, actionCommandId, description, scriptPath);
        
    } catch (const std::exception& e) {
        addParseError(lineNumber, "Error parsing SCR line: " + std::string(e.what()));
        return nullptr;
    }
}

std::vector<std::string> KeymapParser::tokenizeLine(const std::string& line) const {
    std::vector<std::string> tokens;
    size_t pos = 0;
    
    while (pos < line.length()) {
        // Skip whitespace
        while (pos < line.length() && std::isspace(line[pos])) {
            pos++;
        }
        
        if (pos >= line.length()) {
            break;
        }
        
        std::string token;
        
        if (line[pos] == '"') {
            // Handle quoted string
            pos++; // Skip opening quote
            while (pos < line.length() && line[pos] != '"') {
                if (line[pos] == '\\' && pos + 1 < line.length()) {
                    // Handle escaped characters
                    pos++; // Skip backslash
                    switch (line[pos]) {
                        case 'n': token += '\n'; break;
                        case 't': token += '\t'; break;
                        case 'r': token += '\r'; break;
                        case '\\': token += '\\'; break;
                        case '"': token += '"'; break;
                        default: token += line[pos]; break;
                    }
                } else {
                    token += line[pos];
                }
                pos++;
            }
            if (pos < line.length()) {
                pos++; // Skip closing quote
            }
        } else {
            // Handle unquoted token
            while (pos < line.length() && !std::isspace(line[pos])) {
                token += line[pos];
                pos++;
            }
        }
        
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

Context KeymapParser::parseContext(int contextValue) const {
    switch (contextValue) {
        case 0: return Context::MAIN;
        case 100: return Context::MAIN_ALT_RECORDING;
        case 32060: return Context::MIDI_EDITOR;
        case 32061: return Context::MIDI_EVENT_LIST;
        case 32062: return Context::MIDI_INLINE_EDITOR;
        case 32063: return Context::MEDIA_EXPLORER;
        case 102: return Context::GLOBAL_MAIN;
        case 103: return Context::GLOBAL_MAIN_ALT;
        default: return Context::UNKNOWN_CONTEXT;
    }
}

std::string KeymapParser::contextToString(Context context) const {
    return std::to_string(static_cast<int>(context));
}

void KeymapParser::addParseError(int lineNumber, const std::string& error) {
    parseErrors[lineNumber] = error;
}

bool KeymapParser::isGlobalShortcutPair(size_t index) const {
    if (index + 1 >= entries.size()) {
        return false;
    }
    
    auto* first = dynamic_cast<KeyBinding*>(entries[index].get());
    auto* second = dynamic_cast<KeyBinding*>(entries[index + 1].get());
    
    if (!first || !second) {
        return false;
    }
    
    // Check if they have the same key combination and the second one has a global context
    return first->hasSameKeyCombo(*second) &&
           (second->getContext() == Context::GLOBAL_MAIN || 
            second->getContext() == Context::GLOBAL_MAIN_ALT);
}

std::unique_ptr<GlobalKeyBinding> KeymapParser::parseGlobalShortcut(size_t& index) {
    if (index + 1 >= entries.size()) {
        return nullptr;
    }
    
    auto actionBinding = std::unique_ptr<KeyBinding>(
        static_cast<KeyBinding*>(entries[index].release()));
    auto scopeBinding = std::unique_ptr<KeyBinding>(
        static_cast<KeyBinding*>(entries[index + 1].release()));
    
    GlobalKeyScope scope = GlobalKeyScope::GLOBAL;
    if (scopeBinding->getActionCommandId() == "101") {
        scope = GlobalKeyScope::GLOBAL_TEXTFIELDS;
    }
    
    return std::make_unique<GlobalKeyBinding>(std::move(actionBinding), std::move(scopeBinding), scope);
}

// ============================================================================
// Query Methods
// ============================================================================

std::vector<KeymapEntry*> KeymapParser::getEntriesByType(EntryType type) const {
    std::vector<KeymapEntry*> result;
    for (const auto& entry : entries) {
        if (entry->getType() == type) {
            result.push_back(entry.get());
        }
    }
    return result;
}

std::vector<KeymapEntry*> KeymapParser::getEntriesByContext(Context context) const {
    std::vector<KeymapEntry*> result;
    for (const auto& entry : entries) {
        if (entry->getContext() == context) {
            result.push_back(entry.get());
        }
    }
    return result;
}

KeymapEntry* KeymapParser::findEntryByActionCommandId(const std::string& id) const {
    for (const auto& entry : entries) {
        if (auto* keyBinding = dynamic_cast<KeyBinding*>(entry.get())) {
            if (keyBinding->getActionCommandId() == id) {
                return entry.get();
            }
        } else if (auto* customAction = dynamic_cast<CustomAction*>(entry.get())) {
            if (customAction->getActionCommandId() == id) {
                return entry.get();
            }
        } else if (auto* scriptAction = dynamic_cast<ScriptAction*>(entry.get())) {
            if (scriptAction->getActionCommandId() == id) {
                return entry.get();
            }
        }
    }
    return nullptr;
}

std::vector<KeyBinding*> KeymapParser::findKeyBindingsForAction(const std::string& actionId) const {
    std::vector<KeyBinding*> result;
    for (const auto& entry : entries) {
        if (auto* keyBinding = dynamic_cast<KeyBinding*>(entry.get())) {
            if (keyBinding->getActionCommandId() == actionId) {
                result.push_back(keyBinding);
            }
        }
    }
    return result;
}

std::vector<KeyBinding*> KeymapParser::findConflictingKeyBindings() const {
    std::vector<KeyBinding*> conflicts;
    std::map<std::string, std::vector<KeyBinding*>> keyComboMap;
    
    // Group key bindings by key combination and context
    for (const auto& entry : entries) {
        if (auto* keyBinding = dynamic_cast<KeyBinding*>(entry.get())) {
            std::string key = keyBinding->getUniqueId();
            keyComboMap[key].push_back(keyBinding);
        }
    }
    
    // Find conflicts (multiple bindings for same key combo in same context)
    for (const auto& pair : keyComboMap) {
        if (pair.second.size() > 1) {
            conflicts.insert(conflicts.end(), pair.second.begin(), pair.second.end());
        }
    }
    
    return conflicts;
}

// ============================================================================
// Modification Methods
// ============================================================================

void KeymapParser::addEntry(std::unique_ptr<KeymapEntry> entry) {
    if (entry) {
        entries.push_back(std::move(entry));
        markAsModified();
        updateActionCommandIdCounts();
    }
}

void KeymapParser::addKeyBinding(int modifierValue, int keyNoteValue, 
                                const std::string& actionCommandId, Context context) {
    auto binding = std::make_unique<KeyBinding>(modifierValue, keyNoteValue, actionCommandId, context);
    addEntry(std::move(binding));
}

void KeymapParser::addCustomAction(int flags, Context context, const std::string& actionCommandId,
                                  const std::string& description, const std::vector<std::string>& commands) {
    auto action = std::make_unique<CustomAction>(flags, context, actionCommandId, description, commands);
    addEntry(std::move(action));
}

void KeymapParser::addScriptAction(ScriptBehavior behavior, Context context, 
                                  const std::string& actionCommandId, const std::string& description,
                                  const std::string& scriptPath) {
    auto script = std::make_unique<ScriptAction>(behavior, context, actionCommandId, description, scriptPath);
    addEntry(std::move(script));
}

bool KeymapParser::removeEntry(const std::string& uniqueId) {
    auto it = std::find_if(entries.begin(), entries.end(),
        [&uniqueId](const std::unique_ptr<KeymapEntry>& entry) {
            return entry->getUniqueId() == uniqueId;
        });
    
    if (it != entries.end()) {
        entries.erase(it);
        markAsModified();
        updateActionCommandIdCounts();
        return true;
    }
    return false;
}

bool KeymapParser::removeEntryByIndex(size_t index) {
    if (index < entries.size()) {
        entries.erase(entries.begin() + index);
        markAsModified();
        updateActionCommandIdCounts();
        return true;
    }
    return false;
}

size_t KeymapParser::removeEntriesByType(EntryType type) {
    size_t removed = 0;
    auto it = entries.begin();
    while (it != entries.end()) {
        if ((*it)->getType() == type) {
            it = entries.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    
    if (removed > 0) {
        markAsModified();
        updateActionCommandIdCounts();
    }
    return removed;
}

size_t KeymapParser::removeEntriesByContext(Context context) {
    size_t removed = 0;
    auto it = entries.begin();
    while (it != entries.end()) {
        if ((*it)->getContext() == context) {
            it = entries.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    
    if (removed > 0) {
        markAsModified();
        updateActionCommandIdCounts();
    }
    return removed;
}

size_t KeymapParser::removeEntriesMatching(std::function<bool(const KeymapEntry*)> predicate) {
    size_t removed = 0;
    auto it = entries.begin();
    while (it != entries.end()) {
        if (predicate(it->get())) {
            it = entries.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    
    if (removed > 0) {
        markAsModified();
        updateActionCommandIdCounts();
    }
    return removed;
}

bool KeymapParser::updateEntry(const std::string& uniqueId, std::unique_ptr<KeymapEntry> newEntry) {
    auto it = std::find_if(entries.begin(), entries.end(),
        [&uniqueId](const std::unique_ptr<KeymapEntry>& entry) {
            return entry->getUniqueId() == uniqueId;
        });
    
    if (it != entries.end() && newEntry) {
        newEntry->setLineNumber((*it)->getLineNumber());
        *it = std::move(newEntry);
        markAsModified();
        updateActionCommandIdCounts();
        return true;
    }
    return false;
}

bool KeymapParser::replaceKeyBinding(const std::string& actionCommandId, Context context,
                                    int newModifierValue, int newKeyNoteValue) {
    for (auto& entry : entries) {
        if (auto* keyBinding = dynamic_cast<KeyBinding*>(entry.get())) {
            if (keyBinding->getActionCommandId() == actionCommandId && 
                keyBinding->getContext() == context) {
                
                auto newBinding = std::make_unique<KeyBinding>(
                    newModifierValue, newKeyNoteValue, actionCommandId, context);
                newBinding->setLineNumber(keyBinding->getLineNumber());
                entry = std::move(newBinding);
                markAsModified();
                return true;
            }
        }
    }
    return false;
}


void KeymapParser::replaceEntriesOfType(EntryType type, const std::vector<std::unique_ptr<KeymapEntry>>& newEntries) {
    removeEntriesByType(type);
    
    for (const auto& entry : newEntries) {
        if (entry && entry->getType() == type) {
            addEntry(KeymapUtils::cloneEntry(entry.get()));
        }
    }
}

// ============================================================================
// Output Methods
// ============================================================================

bool KeymapParser::writeToFile(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    file << toString();
    file.close();
    return true;
}

std::string KeymapParser::toString() const {
    std::ostringstream oss;
    
    for (const auto& entry : entries) {
        oss << entry->toString() << "\n";
    }
    
    return oss.str();
}

// ============================================================================
// State Management
// ============================================================================

void KeymapParser::clear() {
    entries.clear();
    parseErrors.clear();
    actionCommandIdCounts.clear();
    modified = false;
}

// ============================================================================
// Validation Methods
// ============================================================================

std::vector<std::string> KeymapParser::validateEntries() const {
    std::vector<std::string> warnings;
    
    // Check for duplicate action command IDs
    std::map<std::string, std::vector<KeymapEntry*>> actionIdMap;
    for (const auto& entry : entries) {
        std::string actionId;
        
        if (auto* keyBinding = dynamic_cast<KeyBinding*>(entry.get())) {
            actionId = keyBinding->getActionCommandId();
        } else if (auto* customAction = dynamic_cast<CustomAction*>(entry.get())) {
            actionId = customAction->getActionCommandId();
        } else if (auto* scriptAction = dynamic_cast<ScriptAction*>(entry.get())) {
            actionId = scriptAction->getActionCommandId();
        }
        
        if (!actionId.empty()) {
            actionIdMap[actionId].push_back(entry.get());
        }
    }
    
    for (const auto& pair : actionIdMap) {
        if (pair.second.size() > 1) {
            warnings.push_back("Duplicate action command ID: " + pair.first);
        }
    }
    
    // Check for key binding conflicts
    auto conflicts = findConflictingKeyBindings();
    if (!conflicts.empty()) {
        warnings.push_back("Found " + std::to_string(conflicts.size()) + " conflicting key bindings");
    }
    
    return warnings;
}

// ============================================================================
// Statistics Methods
// ============================================================================

std::map<EntryType, int> KeymapParser::getEntryCountsByType() const {
    std::map<EntryType, int> counts;
    
    for (const auto& entry : entries) {
        counts[entry->getType()]++;
    }
    
    return counts;
}

std::map<Context, int> KeymapParser::getEntryCountsByContext() const {
    std::map<Context, int> counts;
    
    for (const auto& entry : entries) {
        counts[entry->getContext()]++;
    }
    
    return counts;
}

// ============================================================================
// Helper Methods
// ============================================================================

bool KeymapParser::isValidActionCommandId(const std::string& id) const {
    if (id.empty()) {
        return false;
    }
    
    // Check if it's a numeric ID
    if (std::all_of(id.begin(), id.end(), ::isdigit)) {
        return true;
    }
    
    // Check if it's a valid string ID (alphanumeric + underscore)
    return std::all_of(id.begin(), id.end(), [](char c) {
        return std::isalnum(c) || c == '_';
    });
}

bool KeymapParser::isValidContext(Context context) const {
    return context != Context::UNKNOWN_CONTEXT;
}

void KeymapParser::updateActionCommandIdCounts() {
    actionCommandIdCounts.clear();
    
    for (const auto& entry : entries) {
        std::string actionId;
        
        if (auto* keyBinding = dynamic_cast<KeyBinding*>(entry.get())) {
            actionId = keyBinding->getActionCommandId();
        } else if (auto* customAction = dynamic_cast<CustomAction*>(entry.get())) {
            actionId = customAction->getActionCommandId();
        } else if (auto* scriptAction = dynamic_cast<ScriptAction*>(entry.get())) {
            actionId = scriptAction->getActionCommandId();
        }
        
        if (!actionId.empty()) {
            actionCommandIdCounts[actionId]++;
        }
    }
}

void KeymapParser::validateUniqueIds() {
    std::set<std::string> uniqueIds;
    
    for (const auto& entry : entries) {
        const std::string& id = entry->getUniqueId();
        if (uniqueIds.find(id) != uniqueIds.end()) {
            addParseError(entry->getLineNumber(), "Duplicate unique ID: " + id);
        } else {
            uniqueIds.insert(id);
        }
    }
}

// ============================================================================
// EntryFactory Implementation
// ============================================================================

namespace EntryFactory {

std::unique_ptr<KeyBinding> createKeyBinding(int modifierValue, int keyNoteValue,
                                            const std::string& actionCommandId,
                                            Context context) {
    return std::make_unique<KeyBinding>(modifierValue, keyNoteValue, actionCommandId, context);
}

std::unique_ptr<CustomAction> createCustomAction(int flags, Context context,
                                                const std::string& actionCommandId,
                                                const std::string& description,
                                                const std::vector<std::string>& commands) {
    return std::make_unique<CustomAction>(flags, context, actionCommandId, description, commands);
}

std::unique_ptr<ScriptAction> createScriptAction(ScriptBehavior behavior, Context context,
                                                const std::string& actionCommandId,
                                                const std::string& description,
                                                const std::string& scriptPath) {
    return std::make_unique<ScriptAction>(behavior, context, actionCommandId, description, scriptPath);
}

std::unique_ptr<GlobalKeyBinding> createGlobalKeyBinding(int modifierValue, int keyNoteValue,
                                                       const std::string& actionCommandId,
                                                       Context context, GlobalKeyScope scope) {
    auto actionBinding = std::make_unique<KeyBinding>(modifierValue, keyNoteValue, actionCommandId, context);
    auto scopeBinding = std::make_unique<KeyBinding>(modifierValue, keyNoteValue, 
                                                    std::to_string(static_cast<int>(scope)),
                                                    context == Context::MAIN ? Context::GLOBAL_MAIN : Context::GLOBAL_MAIN_ALT);
    
    return std::make_unique<GlobalKeyBinding>(std::move(actionBinding), std::move(scopeBinding), scope);
}

} // namespace EntryFactory

// ============================================================================
// KeymapUtils Implementation
// ============================================================================

namespace KeymapUtils {

bool isValidKeymapFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line[0] != '#') {
            // Check if line starts with valid entry type
            if (line.substr(0, 3) == "KEY" || 
                line.substr(0, 3) == "ACT" || 
                line.substr(0, 3) == "SCR") {
                return true;
            }
        }
    }
    
    return false;
}

bool backupFile(const std::string& filePath, const std::string& backupSuffix) {
    std::ifstream src(filePath, std::ios::binary);
    if (!src.is_open()) {
        return false;
    }
    
    std::string backupPath = filePath + backupSuffix;
    std::ofstream dst(backupPath, std::ios::binary);
    if (!dst.is_open()) {
        return false;
    }
    
    dst << src.rdbuf();
    return true;
}

std::unique_ptr<KeymapEntry> cloneEntry(const KeymapEntry* entry) {
    if (!entry) {
        return nullptr;
    }
    
    switch (entry->getType()) {
        case EntryType::KEY_BINDING: {
            auto* kb = static_cast<const KeyBinding*>(entry);
            return std::make_unique<KeyBinding>(kb->getModifierValue(), kb->getKeyNoteValue(),
                                               kb->getActionCommandId(), kb->getContext());
        }
        
        case EntryType::CUSTOM_ACTION: {
            auto* ca = static_cast<const CustomAction*>(entry);
            return std::make_unique<CustomAction>(ca->getFlags(), ca->getContext(),
                                                 ca->getActionCommandId(), ca->getDescription(),
                                                 ca->getCommandSequence());
        }
        
        case EntryType::SCRIPT_ACTION: {
            auto* sa = static_cast<const ScriptAction*>(entry);
            return std::make_unique<ScriptAction>(sa->getBehavior(), sa->getContext(),
                                                 sa->getActionCommandId(), sa->getDescription(),
                                                 sa->getScriptPath());
        }
        
        case EntryType::GLOBAL_KEY_BINDING: {
            auto* gkb = static_cast<const GlobalKeyBinding*>(entry);
            auto actionClone = cloneEntry(gkb->getActionBinding());
            auto scopeClone = cloneEntry(gkb->getScopeBinding());
            
            return std::make_unique<GlobalKeyBinding>(
                std::unique_ptr<KeyBinding>(static_cast<KeyBinding*>(actionClone.release())),
                std::unique_ptr<KeyBinding>(static_cast<KeyBinding*>(scopeClone.release())),
                gkb->getScope());
        }
        
        default:
            return nullptr;
    }
}

bool areEntriesEquivalent(const KeymapEntry* entry1, const KeymapEntry* entry2) {
    if (!entry1 || !entry2) {
        return entry1 == entry2;
    }
    
    return entry1->getType() == entry2->getType() &&
           entry1->getUniqueId() == entry2->getUniqueId() &&
           entry1->getContext() == entry2->getContext();
}

std::string decodeKeyCombo(int modifierValue, int keyNoteValue) {
    KeyBinding temp(modifierValue, keyNoteValue, "", Context::MAIN);
    return temp.getKeyComboString();
}

std::pair<int, int> encodeKeyCombo(const std::string& /* keyCombo */) {
    // This is a complex reverse operation - simplified implementation
    // In a full implementation, this would parse the key combo string
    // and return the appropriate modifier and key values
    return {0, 0}; // Placeholder
}

bool isValidKeyCombo(int modifierValue, int keyNoteValue) {
    return modifierValue >= 0 && modifierValue <= 255 &&
           keyNoteValue >= 0 && keyNoteValue <= 65535;
}

std::string contextToDisplayName(Context context) {
    switch (context) {
        case Context::MAIN: return "Main";
        case Context::MAIN_ALT_RECORDING: return "Main (alt recording)";
        case Context::MIDI_EDITOR: return "MIDI Editor";
        case Context::MIDI_EVENT_LIST: return "MIDI Event List";
        case Context::MIDI_INLINE_EDITOR: return "MIDI Inline Editor";
        case Context::MEDIA_EXPLORER: return "Media Explorer";
        case Context::GLOBAL_MAIN: return "Global Main";
        case Context::GLOBAL_MAIN_ALT: return "Global Main Alt";
        default: return "Unknown";
    }
}

Context displayNameToContext(const std::string& name) {
    if (name == "Main") return Context::MAIN;
    if (name == "Main (alt recording)") return Context::MAIN_ALT_RECORDING;
    if (name == "MIDI Editor") return Context::MIDI_EDITOR;
    if (name == "MIDI Event List") return Context::MIDI_EVENT_LIST;
    if (name == "MIDI Inline Editor") return Context::MIDI_INLINE_EDITOR;
    if (name == "Media Explorer") return Context::MEDIA_EXPLORER;
    if (name == "Global Main") return Context::GLOBAL_MAIN;
    if (name == "Global Main Alt") return Context::GLOBAL_MAIN_ALT;
    return Context::UNKNOWN_CONTEXT;
}

std::vector<Context> getAllValidContexts() {
    return {
        Context::MAIN,
        Context::MAIN_ALT_RECORDING,
        Context::MIDI_EDITOR,
        Context::MIDI_EVENT_LIST,
        Context::MIDI_INLINE_EDITOR,
        Context::MEDIA_EXPLORER,
        Context::GLOBAL_MAIN,
        Context::GLOBAL_MAIN_ALT
    };
}

bool isValidActionCommandId(const std::string& id) {
    if (id.empty()) {
        return false;
    }
    
    // Check if it's a numeric ID
    if (std::all_of(id.begin(), id.end(), ::isdigit)) {
        return true;
    }
    
    // Check if it's a valid string ID (alphanumeric + underscore)
    return std::all_of(id.begin(), id.end(), [](char c) {
        return std::isalnum(c) || c == '_';
    });
}

bool isValidScriptPath(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    
    // Check for valid script extensions
    std::string ext = path.substr(path.find_last_of('.') + 1);
    return ext == "lua" || ext == "py" || ext == "eel";
}

std::vector<std::string> validateKeymap(const KeymapParser& keymap) {
    return keymap.validateEntries();
}

} // namespace KeymapUtils

} // namespace KeymapParser
