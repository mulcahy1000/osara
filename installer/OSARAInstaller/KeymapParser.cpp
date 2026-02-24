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
    : m_modifierValue(modifierValue), m_keyNoteValue(keyNoteValue), m_actionCommandId(actionCommandId), m_context(context) {
    determineKeyType();
}

void KeyBinding::determineKeyType() {
    if (m_modifierValue >= 144 && m_modifierValue <= 159) {
        m_keyType = KeyType::MIDI_NOTE;
    } else if (m_modifierValue >= 176 && m_modifierValue <= 191) {
        m_keyType = KeyType::MIDI_CC;
    } else if (m_modifierValue >= 192 && m_modifierValue <= 207) {
        m_keyType = KeyType::MIDI_PC;
    } else if (m_modifierValue >= 224 && m_modifierValue <= 239) {
        m_keyType = KeyType::MIDI_PITCH;
    } else if ((m_modifierValue >= 128 && m_modifierValue <= 143) ||
               (m_modifierValue >= 160 && m_modifierValue <= 175) ||
               (m_modifierValue >= 208 && m_modifierValue <= 223) ||
               (m_modifierValue >= 240 && m_modifierValue <= 254)) {
        m_keyType = KeyType::MIDI_RAW;
    } else if (m_modifierValue == 255) {
        // Check for special media keyboard or multitouch keys
        if (m_keyNoteValue >= 232) {
            m_keyType = KeyType::MEDIA_KEYBOARD;
        } else if ((m_keyNoteValue >= 72 && m_keyNoteValue <= 73) ||
                   (m_keyNoteValue >= 200 && m_keyNoteValue <= 207)) {
            m_keyType = KeyType::MULTITOUCH;
        } else if ((m_keyNoteValue >= 120 && m_keyNoteValue <= 125) ||
                   (m_keyNoteValue >= 248 && m_keyNoteValue <= 255)) {
            m_keyType = KeyType::MOUSEWHEEL;
        } else {
            m_keyType = KeyType::UNKNOWN_KEY_TYPE;
        }
    } else {
        m_keyType = KeyType::REGULAR_KEY;
    }
}

std::string KeyBinding::toString() const {
    std::ostringstream oss;
    oss << "KEY " << m_modifierValue << " " << m_keyNoteValue << " ";
    
    if (m_actionCommandId.empty() || std::isdigit(m_actionCommandId[0])) {
        oss << m_actionCommandId;
    } else {
        oss << "_" << m_actionCommandId;
    }
    
    oss << " " << static_cast<int>(m_context);

    // Add comment: preserve the original (which already contains its leading whitespace),
    // or auto-generate a descriptive one for programmatically-created entries.
    if (!m_comment.empty()) {
        oss << m_comment;
    } else {
        oss << "\t\t # " << getDescription();
    }

    return oss.str();
}

std::string KeyBinding::getDescription() const {
    // Action command ID "0" means "explicitly disable this key slot".
    // Showing "-> 0" is cryptic; just name the key so descriptions like
    // "Remove: Tab" are readable without the confusing arrow-zero suffix.
    if (m_actionCommandId.empty() || m_actionCommandId == "0")
        return getKeyComboString();
    return getKeyComboString() + " -> " + m_actionCommandId;
}

std::string KeyBinding::getUniqueId() const {
    std::ostringstream oss;
    oss << "KEY_" << m_modifierValue << "_" << m_keyNoteValue << "_" << static_cast<int>(m_context);
    return oss.str();
}

std::string KeyBinding::getKeyComboString() const {
    switch (m_keyType) {
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
    if (m_keyType != KeyType::REGULAR_KEY) {
        return ""; // MIDI and special keys handle modifiers differently
    }

    // Modifier bits (same for both odd and even modifier values):
    //   bit 2 (0x04) = Shift
    //   bit 3 (0x08) = Ctrl  (Cmd on macOS)
    //   bit 4 (0x10) = Alt   (Option on macOS)
    //   bit 5 (0x20) = Win   (Ctrl on macOS)
    // Bit 0 is the "regular key" indicator and carries no modifier meaning.
    std::vector<std::string> modifiers;
    if (m_modifierValue & 4)  modifiers.push_back("Shift");
    if (m_modifierValue & 8)  modifiers.push_back("Ctrl");
    if (m_modifierValue & 16) modifiers.push_back("Alt");
    if (m_modifierValue & 32) modifiers.push_back("Win");

    std::string result;
    for (size_t i = 0; i < modifiers.size(); ++i) {
        if (i > 0) result += "+";
        result += modifiers[i];
    }
    return result;
}

std::string KeyBinding::getKeyString() const {
    if (m_keyType != KeyType::REGULAR_KEY) {
        return getKeyComboString(); // For non-regular keys, the combo string is the key
    }
    
    // Special key values
    if (m_keyNoteValue >= 32801 && m_keyNoteValue <= 32815) {
        switch (m_keyNoteValue) {
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
            default: return "Special Key " + std::to_string(m_keyNoteValue);
        }
    }
    
    // ASCII keys
    if (m_keyNoteValue >= 32 && m_keyNoteValue <= 126) {
        char c = static_cast<char>(m_keyNoteValue);
        if (std::isalnum(c) || std::ispunct(c)) {
            return std::string(1, c);
        }
    }
    
    // Function keys and other special keys
    if (m_keyNoteValue >= 112 && m_keyNoteValue <= 123) {
        return "F" + std::to_string(m_keyNoteValue - 111);
    }
    
    return "Key " + std::to_string(m_keyNoteValue);
}

bool KeyBinding::hasSameKeyCombo(const KeyBinding& other) const {
    return m_modifierValue == other.m_modifierValue && m_keyNoteValue == other.m_keyNoteValue;
}

int KeyBinding::getMidiChannel() const {
    if (m_keyType == KeyType::MIDI_NOTE) {
        return m_modifierValue - 144 + 1; // Channels 1-16
    } else if (m_keyType == KeyType::MIDI_CC) {
        return m_modifierValue - 176 + 1;
    } else if (m_keyType == KeyType::MIDI_PC) {
        return m_modifierValue - 192 + 1;
    } else if (m_keyType == KeyType::MIDI_PITCH) {
        return m_modifierValue - 224 + 1;
    }
    return -1;
}

int KeyBinding::getMidiNote() const {
    if (m_keyType == KeyType::MIDI_NOTE || m_keyType == KeyType::MIDI_CC || m_keyType == KeyType::MIDI_PC) {
        return m_keyNoteValue % 127; // Notes wrap at 127
    }
    return -1;
}

int KeyBinding::getMidiCC() const {
    if (m_keyType == KeyType::MIDI_CC) {
        return m_keyNoteValue % 127;
    }
    return -1;
}

std::string KeyBinding::formatMidiKey() const {
    std::ostringstream oss;
    
    switch (m_keyType) {
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
            oss << "MIDI Raw " << std::hex << m_modifierValue << " " << m_keyNoteValue;
            break;
        default:
            oss << "MIDI Unknown";
            break;
    }
    
    return oss.str();
}

std::string KeyBinding::formatMediaKey() const {
    if (m_keyType == KeyType::MEDIA_KEYBOARD) {
        switch (m_keyNoteValue) {
            case 488: return "MediaKbd Browse-";
            case 744: return "MediaKbd Browse+";
            case 1000: return "MediaKbd Browse Refresh";
            case 2280: return "MediaKbd Mute";
            case 2536: return "MediaKbd Vol-";
            case 2792: return "MediaKbd Vol+";
            case 3560: return "MediaKbd Stop";
            case 3816: return "MediaKbd Play/Pause";
            case 4072: return "MediaKbd Mail";
            default: return "MediaKbd " + std::to_string(m_keyNoteValue);
        }
    } else if (m_keyType == KeyType::MOUSEWHEEL) {
        if (m_keyNoteValue == 120 || m_keyNoteValue == 248) {
            return "Mousewheel";
        } else if (m_keyNoteValue == 88 || m_keyNoteValue == 216) {
            return "Horizontal Mousewheel";
        }
    } else if (m_keyType == KeyType::MULTITOUCH) {
        if (m_keyNoteValue == 72 || m_keyNoteValue == 200) {
            return "MultiZoom";
        } else if (m_keyNoteValue == 24 || m_keyNoteValue == 152) {
            return "MultiRotate";
        }
    }
    
    return "Special " + std::to_string(m_keyNoteValue);
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
    : m_actionBinding(std::move(actionBinding)), m_scopeBinding(std::move(scopeBinding)), m_scope(scope) {
}

std::string GlobalKeyBinding::toString() const {
    return m_actionBinding->toString() + "\n" + m_scopeBinding->toString();
}

std::string GlobalKeyBinding::getDescription() const {
    std::string scopeDesc = (m_scope == GlobalKeyScope::GLOBAL) ? "Global" : "Global+TextFields";
    return getKeyComboString() + " -> " + m_actionBinding->getActionCommandId() + " (" + scopeDesc + ")";
}

Context GlobalKeyBinding::getContext() const {
    return m_actionBinding->getContext();
}

std::string GlobalKeyBinding::getUniqueId() const {
    return "GLOBAL_" + m_actionBinding->getUniqueId();
}

std::string GlobalKeyBinding::getKeyComboString() const {
    return m_actionBinding->getKeyComboString();
}

// ============================================================================
// CustomAction Implementation
// ============================================================================

CustomAction::CustomAction(int flags, Context context, const std::string& actionCommandId,
                          const std::string& description, const std::vector<std::string>& commands)
    : m_flags(flags), m_context(context), m_actionCommandId(actionCommandId), 
      m_description(description), m_commandSequence(commands) {
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
    
    oss << "ACT " << m_flags << " " << static_cast<int>(m_context) 
        << " \"" << escapeQuotes(m_actionCommandId) << "\" \"" << escapeQuotes(m_description) << "\"";
    
    for (const auto& command : m_commandSequence) {
        oss << " " << command;
    }

    // Add comment: preserve the original, or auto-generate from the description.
    if (!m_comment.empty()) {
        oss << m_comment;
    } else {
        oss << "\t\t # " << getDescription();
    }

    return oss.str();
}

// ============================================================================
// ScriptAction Implementation
// ============================================================================

ScriptAction::ScriptAction(ScriptBehavior behavior, Context context, const std::string& actionCommandId,
                          const std::string& description, const std::string& scriptPath)
    : m_behavior(behavior), m_context(context), m_actionCommandId(actionCommandId),
      m_description(description), m_scriptPath(scriptPath) {
}

std::string ScriptAction::toString() const {
    std::ostringstream oss;
    oss << "SCR " << static_cast<int>(m_behavior) << " " << static_cast<int>(m_context)
        << " " << m_actionCommandId << " \"" << m_description << "\" " << m_scriptPath;

    // Add comment: preserve the original, or auto-generate from the description.
    if (!m_comment.empty()) {
        oss << m_comment;
    } else {
        oss << "\t\t # " << getDescription();
    }

    return oss.str();
}

bool ScriptAction::consolidatesUndoPoints() const {
    return (static_cast<int>(m_behavior) & 1) != 0;
}

bool ScriptAction::showsInActionsMenu() const {
    return (static_cast<int>(m_behavior) & 2) != 0;
}

std::string ScriptAction::getBehaviorDescription() const {
    switch (m_behavior) {
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
            m_entries.push_back(std::move(entry));
        }
    }
    
    // Post-process to detect global shortcuts
    std::vector<std::unique_ptr<KeymapEntry>> processedEntries;
    
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (isGlobalShortcutPair(i)) {
            auto globalBinding = parseGlobalShortcut(i);
            if (globalBinding) {
                processedEntries.push_back(std::move(globalBinding));
                ++i; // Skip the next entry as it's part of the global shortcut
            }
        } else {
            processedEntries.push_back(std::move(m_entries[i]));
        }
    }
    
    m_entries = std::move(processedEntries);
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
    m_parseErrors[lineNumber] = error;
}

bool KeymapParser::isGlobalShortcutPair(size_t index) const {
    if (index + 1 >= m_entries.size()) {
        return false;
    }
    
    auto* first = dynamic_cast<KeyBinding*>(m_entries[index].get());
    auto* second = dynamic_cast<KeyBinding*>(m_entries[index + 1].get());
    
    if (!first || !second) {
        return false;
    }
    
    // Check if they have the same key combination and the second one has a global context
    return first->hasSameKeyCombo(*second) &&
           (second->getContext() == Context::GLOBAL_MAIN || 
            second->getContext() == Context::GLOBAL_MAIN_ALT);
}

std::unique_ptr<GlobalKeyBinding> KeymapParser::parseGlobalShortcut(size_t& index) {
    if (index + 1 >= m_entries.size()) {
        return nullptr;
    }
    
    auto actionBinding = std::unique_ptr<KeyBinding>(
        static_cast<KeyBinding*>(m_entries[index].release()));
    auto scopeBinding = std::unique_ptr<KeyBinding>(
        static_cast<KeyBinding*>(m_entries[index + 1].release()));
    
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
    for (const auto& entry : m_entries) {
        if (entry->getType() == type) {
            result.push_back(entry.get());
        }
    }
    return result;
}

std::vector<KeymapEntry*> KeymapParser::getEntriesByContext(Context context) const {
    std::vector<KeymapEntry*> result;
    for (const auto& entry : m_entries) {
        if (entry->getContext() == context) {
            result.push_back(entry.get());
        }
    }
    return result;
}

KeymapEntry* KeymapParser::findEntryByActionCommandId(const std::string& id) const {
    for (const auto& entry : m_entries) {
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
    for (const auto& entry : m_entries) {
        if (auto* keyBinding = dynamic_cast<KeyBinding*>(entry.get())) {
            if (keyBinding->getActionCommandId() == actionId) {
                result.push_back(keyBinding);
            }
        }
    }
    return result;
}

std::vector<KeyBinding*> KeymapParser::findConflictingKeyBindings() const {
    // Collect all plain KEY_BINDING entries (not GlobalKeyBindings).
    std::vector<KeyBinding*> all;
    for (const auto& entry : m_entries) {
        if (auto* kb = dynamic_cast<KeyBinding*>(entry.get()))
            all.push_back(kb);
    }

    // Mark every binding that shares (modifier, key, context) with another.
    std::vector<KeyBinding*> result;
    for (size_t i = 0; i < all.size(); ++i) {
        for (size_t j = i + 1; j < all.size(); ++j) {
            if (all[i]->hasSameKeyCombo(*all[j]) &&
                all[i]->getContext() == all[j]->getContext()) {
                // Add both if not already present.
                if (std::find(result.begin(), result.end(), all[i]) == result.end())
                    result.push_back(all[i]);
                if (std::find(result.begin(), result.end(), all[j]) == result.end())
                    result.push_back(all[j]);
            }
        }
    }
    return result;
}

// ============================================================================
// Modification Methods
// ============================================================================

void KeymapParser::addEntry(std::unique_ptr<KeymapEntry> entry) {
    if (entry) {
        m_entries.push_back(std::move(entry));
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
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&uniqueId](const std::unique_ptr<KeymapEntry>& entry) {
            return entry->getUniqueId() == uniqueId;
        });
    
    if (it != m_entries.end()) {
        m_entries.erase(it);
        markAsModified();
        updateActionCommandIdCounts();
        return true;
    }
    return false;
}

bool KeymapParser::removeEntryByIndex(size_t index) {
    if (index < m_entries.size()) {
        m_entries.erase(m_entries.begin() + index);
        markAsModified();
        updateActionCommandIdCounts();
        return true;
    }
    return false;
}

size_t KeymapParser::removeEntriesByType(EntryType type) {
    size_t removed = 0;
    auto it = m_entries.begin();
    while (it != m_entries.end()) {
        if ((*it)->getType() == type) {
            it = m_entries.erase(it);
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
    auto it = m_entries.begin();
    while (it != m_entries.end()) {
        if ((*it)->getContext() == context) {
            it = m_entries.erase(it);
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
    auto it = m_entries.begin();
    while (it != m_entries.end()) {
        if (predicate(it->get())) {
            it = m_entries.erase(it);
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
    
    for (const auto& entry : m_entries) {
        oss << entry->toString() << "\n";
    }
    
    return oss.str();
}

// ============================================================================
// State Management
// ============================================================================

void KeymapParser::clear() {
    m_entries.clear();
    m_parseErrors.clear();
    m_actionCommandIdCounts.clear();
    m_modified = false;
}

// ============================================================================
// Statistics Methods
// ============================================================================

std::map<EntryType, int> KeymapParser::getEntryCountsByType() const {
    std::map<EntryType, int> counts;
    
    for (const auto& entry : m_entries) {
        counts[entry->getType()]++;
    }
    
    return counts;
}

std::map<Context, int> KeymapParser::getEntryCountsByContext() const {
    std::map<Context, int> counts;
    
    for (const auto& entry : m_entries) {
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
    m_actionCommandIdCounts.clear();
    
    for (const auto& entry : m_entries) {
        std::string actionId;
        
        if (auto* keyBinding = dynamic_cast<KeyBinding*>(entry.get())) {
            actionId = keyBinding->getActionCommandId();
        } else if (auto* customAction = dynamic_cast<CustomAction*>(entry.get())) {
            actionId = customAction->getActionCommandId();
        } else if (auto* scriptAction = dynamic_cast<ScriptAction*>(entry.get())) {
            actionId = scriptAction->getActionCommandId();
        }
        
        if (!actionId.empty()) {
            m_actionCommandIdCounts[actionId]++;
        }
    }
}

void KeymapParser::validateUniqueIds() {
    std::set<std::string> uniqueIds;
    
    for (const auto& entry : m_entries) {
        const std::string& id = entry->getUniqueId();
        if (uniqueIds.find(id) != uniqueIds.end()) {
            addParseError(entry->getLineNumber(), "Duplicate unique ID: " + id);
        } else {
            uniqueIds.insert(id);
        }
    }
}

// ============================================================================
// KeymapUtils Implementation
// ============================================================================

namespace KeymapUtils {

std::string contextToDisplayName(Context context) {
    switch (context) {
        case Context::MAIN:                return "Main";
        case Context::MAIN_ALT_RECORDING:  return "Main (alt recording)";
        case Context::MIDI_EDITOR:         return "MIDI Editor";
        case Context::MIDI_EVENT_LIST:     return "MIDI Event List";
        case Context::MIDI_INLINE_EDITOR:  return "MIDI Inline Editor";
        case Context::MEDIA_EXPLORER:      return "Media Explorer";
        case Context::GLOBAL_MAIN:         return "Global Main";
        case Context::GLOBAL_MAIN_ALT:     return "Global Main (alt)";
        default:                           return "Unknown";
    }
}

Context displayNameToContext(const std::string& displayName) {
    if (displayName == "Main")                   return Context::MAIN;
    if (displayName == "Main (alt recording)")   return Context::MAIN_ALT_RECORDING;
    if (displayName == "MIDI Editor")            return Context::MIDI_EDITOR;
    if (displayName == "MIDI Event List")        return Context::MIDI_EVENT_LIST;
    if (displayName == "MIDI Inline Editor")     return Context::MIDI_INLINE_EDITOR;
    if (displayName == "Media Explorer")         return Context::MEDIA_EXPLORER;
    if (displayName == "Global Main")            return Context::GLOBAL_MAIN;
    if (displayName == "Global Main (alt)")      return Context::GLOBAL_MAIN_ALT;
    return Context::UNKNOWN_CONTEXT;
}

bool isValidActionCommandId(const std::string& id) {
    if (id.empty()) return false;
    if (std::all_of(id.begin(), id.end(), ::isdigit)) return true;
    return std::all_of(id.begin(), id.end(), [](char c) {
        return std::isalnum(c) || c == '_';
    });
}

} // namespace KeymapUtils

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

} // namespace KeymapParser
