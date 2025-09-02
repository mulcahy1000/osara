#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <functional>

namespace KeymapParser {

// Forward declarations
class KeymapEntry;
class KeyBinding;
class CustomAction;
class ScriptAction;
class GlobalKeyBinding;

// Enums for different entry types
enum class EntryType {
    KEY_BINDING,
    CUSTOM_ACTION,
    SCRIPT_ACTION,
    GLOBAL_KEY_BINDING,
    UNKNOWN
};

// Context IDs as specified in Reaper documentation
enum class Context : int {
    MAIN = 0,
    MAIN_ALT_RECORDING = 100,
    MIDI_EDITOR = 32060,
    MIDI_EVENT_LIST = 32061,
    MIDI_INLINE_EDITOR = 32062,
    MEDIA_EXPLORER = 32063,
    GLOBAL_MAIN = 102,
    GLOBAL_MAIN_ALT = 103,
    UNKNOWN_CONTEXT = -1
};

// Key types for different input methods
enum class KeyType {
    REGULAR_KEY,        // Standard keyboard keys
    MIDI_NOTE,          // MIDI notes (144-159)
    MIDI_CC,            // MIDI CC (176-191)  
    MIDI_PC,            // MIDI Program Change (192-207)
    MIDI_PITCH,         // MIDI Pitch Bend (224-239)
    MIDI_RAW,           // Raw MIDI (128-143, 160-175, 208-223, 240-254)
    MEDIA_KEYBOARD,     // Media keyboard keys (255 modifier)
    MULTITOUCH,         // Multitouch gestures
    MOUSEWHEEL,         // Mouse wheel events
    UNKNOWN_KEY_TYPE
};

// Global key scope for global shortcuts
enum class GlobalKeyScope {
    GLOBAL = 1,
    GLOBAL_TEXTFIELDS = 101
};

// Script behavior flags
enum class ScriptBehavior {
    SHOW_DIALOG = 4,           // Show instance dialog (default)
    TERMINATE_ALL = 260,       // Always terminate all instances
    NEW_INSTANCE = 516         // Always start new instance
};

// Base class for all keymap entries
class KeymapEntry {
public:
    virtual ~KeymapEntry() = default;
    virtual std::string getUniqueId() const = 0;
    virtual EntryType getType() const = 0;
    virtual std::string toString() const = 0;
    virtual std::string getDescription() const = 0;
    virtual Context getContext() const = 0;
    int getLineNumber() const { return m_lineNumber; }
    void setLineNumber(int line) { m_lineNumber = line; }
    virtual std::string getComment() const { return m_comment; }
    virtual void setComment(const std::string& comment) { this->m_comment = comment; }

    bool operator==(const KeymapEntry& other) const;

protected:
    int m_lineNumber = -1;
    std::string m_comment; // Optional comment for the entry
};

// Key binding entry (KEY lines)
class KeyBinding : public KeymapEntry {
public:
    KeyBinding(int modifierValue, int keyNoteValue, const std::string& actionCommandId, 
               Context context);
    
    std::string getUniqueId() const override;
    EntryType getType() const override { return EntryType::KEY_BINDING; }
    std::string toString() const override;
    std::string getDescription() const override;
    Context getContext() const override { return m_context; }
    
    // Getters
    int getModifierValue() const { return m_modifierValue; }
    int getKeyNoteValue() const { return m_keyNoteValue; }
    const std::string& getActionCommandId() const { return m_actionCommandId; }
    KeyType getKeyType() const { return m_keyType; }
    
    // Key combination description
    std::string getKeyComboString() const;
    std::string getModifierString() const;
    std::string getKeyString() const;
    
    // Check if this binding uses the same key combination
    bool hasSameKeyCombo(const KeyBinding& other) const;
    
    // MIDI-specific getters (only valid for MIDI key types)
    int getMidiChannel() const;
    int getMidiNote() const;
    int getMidiCC() const;
    
private:
    void determineKeyType();
    std::string formatMidiKey() const;
    std::string formatMediaKey() const;
    std::string formatRegularKey() const;

    int m_modifierValue;
    int m_keyNoteValue;
    std::string m_actionCommandId;
    Context m_context;
    KeyType m_keyType;
    
};

// Global key binding (special dual-entry format)
class GlobalKeyBinding : public KeymapEntry {
public:
    GlobalKeyBinding(std::unique_ptr<KeyBinding> actionBinding,
                    std::unique_ptr<KeyBinding> scopeBinding,
                    GlobalKeyScope scope);

    std::string getUniqueId() const override;
    EntryType getType() const override { return EntryType::GLOBAL_KEY_BINDING; }
    std::string toString() const override;
    std::string getDescription() const override;
    Context getContext() const override;

    // Getters
    const KeyBinding* getActionBinding() const { return m_actionBinding.get(); }
    const KeyBinding* getScopeBinding() const { return m_scopeBinding.get(); }
    GlobalKeyScope getScope() const { return m_scope; }
    std::string getKeyComboString() const;

private:
    std::unique_ptr<KeyBinding> m_actionBinding;
    std::unique_ptr<KeyBinding> m_scopeBinding;
    GlobalKeyScope m_scope;
};

// Custom action entry (ACT lines)
class CustomAction : public KeymapEntry {
public:
    CustomAction(int flags, Context context, const std::string& actionCommandId,
                const std::string& description, const std::vector<std::string>& commands);

    std::string getUniqueId() const override { return m_actionCommandId; }
    EntryType getType() const override { return EntryType::CUSTOM_ACTION; }
    std::string toString() const override;
    std::string getDescription() const override { return m_description; }
    Context getContext() const override { return m_context; }

    // Getters
    const std::string& getActionCommandId() const { return m_actionCommandId; }
    const std::vector<std::string>& getCommandSequence() const { return m_commandSequence; }
    int getFlags() const { return m_flags; }

    // Flag interpretation
    bool consolidatesUndoPoints() const { return m_flags & 1; }
    bool showsInActionsMenu() const { return m_flags & 2; }
    bool showsAsActiveIfAllComponentsActive() const { return (flags & 16) || (m_flags & 32); }

private:
    int m_flags;
    Context m_context;
    std::string m_actionCommandId;
    std::string m_description;
    std::vector<std::string> m_commandSequence;
};

// Script action entry (SCR lines)
class ScriptAction : public KeymapEntry {
public:
    ScriptAction(ScriptBehavior behavior, Context context, const std::string& actionCommandId,
                const std::string& description, const std::string& scriptPath);

    std::string getUniqueId() const override { return m_actionCommandId; }
    EntryType getType() const override { return EntryType::SCRIPT_ACTION; }
    std::string toString() const override;
    std::string getDescription() const override { return m_description; }
    Context getContext() const override { return m_context; }

    // Getters
    const std::string& getActionCommandId() const { return m_actionCommandId; }
    const std::string& getScriptPath() const { return m_scriptPath; }
    ScriptBehavior getBehavior() const { return m_behavior; }

    // Behavior interpretation
    bool consolidatesUndoPoints() const;
    bool showsInActionsMenu() const;
    std::string getBehaviorDescription() const;

private:
    ScriptBehavior m_behavior;
    Context m_context;
    std::string m_actionCommandId;
    std::string m_description;
    std::string m_scriptPath;
};

// Main parser class with full modification support
class KeymapParser {
public:
    KeymapParser() = default;
    KeymapParser(KeymapParser&& other) noexcept
        : entries(std::move(other.entries))
        , parseErrors(std::move(other.parseErrors))
        , actionCommandIdCounts(std::move(other.actionCommandIdCounts))
        , modified(other.modified) {
        other.modified = false;
    }
    KeymapParser& operator=(KeymapParser&& other) noexcept {
        if (this != &other) {
            entries = std::move(other.entries);
            parseErrors = std::move(other.parseErrors);
            actionCommandIdCounts = std::move(other.actionCommandIdCounts);
            modified = other.modified;
            other.modified = false;
        }
        return *this;
    }
    KeymapParser(const KeymapParser&) = delete;
    KeymapParser& operator=(const KeymapParser&) = delete;

    bool parseFile(const std::string& filePath);
    bool parseString(const std::string& content);

    const std::vector<std::unique_ptr<KeymapEntry>>& getEntries() const { return m_entries; }
    std::vector<KeymapEntry*> getEntriesByType(EntryType type) const;
    std::vector<KeymapEntry*> getEntriesByContext(Context context) const;
    KeymapEntry* findEntryByActionCommandId(const std::string& id) const;
    std::vector<KeyBinding*> findKeyBindingsForAction(const std::string& actionId) const;

    void addEntry(std::unique_ptr<KeymapEntry> entry);
    void addKeyBinding(int modifierValue, int keyNoteValue, 
                      const std::string& actionCommandId, Context context);
    void addCustomAction(int flags, Context context, const std::string& actionCommandId,
                        const std::string& description, const std::vector<std::string>& commands);
    void addScriptAction(ScriptBehavior behavior, Context context, 
                        const std::string& actionCommandId, const std::string& description,
                        const std::string& scriptPath);

    bool removeEntry(const std::string& uniqueId);
    bool removeEntryByIndex(size_t index);
    size_t removeEntriesByType(EntryType type);
    size_t removeEntriesByContext(Context context);
    size_t removeEntriesMatching(std::function<bool(const KeymapEntry*)> predicate);

    bool updateEntry(const std::string& uniqueId, std::unique_ptr<KeymapEntry> newEntry);
    bool replaceKeyBinding(const std::string& actionCommandId, Context context,
                          int newModifierValue, int newKeyNoteValue);

    void replaceEntriesOfType(EntryType type, const std::vector<std::unique_ptr<KeymapEntry>>& newEntries);

    bool writeToFile(const std::string& filePath) const;
    std::string toString() const;

    bool isModified() const { return m_modified; }
    void markAsModified() { m_modified = true; }
    void markAsClean() { m_modified = false; }
    void clear();

    const std::map<int, std::string>& getParseErrors() const { return m_parseErrors; }
    bool hasErrors() const { return !m_parseErrors.empty(); }
    std::vector<std::string> validateEntries() const;

    size_t getEntryCount() const { return m_entries.size(); }
    std::map<EntryType, int> getEntryCountsByType() const;
    std::map<Context, int> getEntryCountsByContext() const;

private:
    std::unique_ptr<KeymapEntry> parseLine(const std::string& line, int lineNumber);
    std::unique_ptr<KeyBinding> parseKeyLine(const std::string& line, int lineNumber);
    std::unique_ptr<CustomAction> parseActLine(const std::string& line, int lineNumber);
    std::unique_ptr<ScriptAction> parseScrLine(const std::string& line, int lineNumber);

    bool isGlobalShortcutPair(size_t index) const;
    std::unique_ptr<GlobalKeyBinding> parseGlobalShortcut(size_t& index);

    Context parseContext(int contextValue) const;
    std::string contextToString(Context context) const;
    std::vector<std::string> tokenizeLine(const std::string& line) const;
    std::string parseQuotedString(const std::string& input, size_t& pos) const;
    void addParseError(int lineNumber, const std::string& error);

    bool isValidActionCommandId(const std::string& id) const;
    bool isValidContext(Context context) const;
    void updateActionCommandIdCounts();
    void validateUniqueIds();

    std::vector<std::unique_ptr<KeymapEntry>> m_entries;
    std::map<int, std::string> m_parseErrors;
    std::map<std::string, int> m_actionCommandIdCounts;
    bool m_modified = false;
};

// Factory functions for creating entries
namespace EntryFactory {
    std::unique_ptr<KeyBinding> createKeyBinding(int modifierValue, int keyNoteValue,
                                                const std::string& actionCommandId,
                                                Context context);
    std::unique_ptr<CustomAction> createCustomAction(int flags, Context context,
                                                    const std::string& actionCommandId,
                                                    const std::string& description,
                                                    const std::vector<std::string>& commands);
    std::unique_ptr<ScriptAction> createScriptAction(ScriptBehavior behavior, Context context,
                                                    const std::string& actionCommandId,
                                                    const std::string& description,
                                                    const std::string& scriptPath);
    std::unique_ptr<GlobalKeyBinding> createGlobalKeyBinding(int modifierValue, int keyNoteValue,
                                                           const std::string& actionCommandId,
                                                           Context context, GlobalKeyScope scope);
}

// Utility functions
namespace KeymapUtils {
    // File operations
    bool isValidKeymapFile(const std::string& filePath);
    bool backupFile(const std::string& filePath, const std::string& backupSuffix = ".backup");

    std::unique_ptr<KeymapEntry> cloneEntry(const KeymapEntry* entry);

    std::string decodeKeyCombo(int modifierValue, int keyNoteValue);
    std::pair<int, int> encodeKeyCombo(const std::string& keyCombo);
    bool isValidKeyCombo(int modifierValue, int keyNoteValue);

    std::string contextToDisplayName(Context context);
    Context displayNameToContext(const std::string& name);
    std::vector<Context> getAllValidContexts();

    bool isValidActionCommandId(const std::string& id);
    bool isValidScriptPath(const std::string& path);
    std::vector<std::string> validateKeymap(const KeymapParser& keymap);
}

} // namespace KeymapParser
