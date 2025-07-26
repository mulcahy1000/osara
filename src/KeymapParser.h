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
    virtual EntryType getType() const = 0;
    virtual std::string toString() const = 0;
    virtual std::string getDescription() const = 0;
    virtual Context getContext() const = 0;
    virtual bool isConflictWith(const KeymapEntry& other) const = 0;
    virtual std::string getUniqueId() const = 0;
    
    // Line number in original file (for error reporting)
    int getLineNumber() const { return lineNumber; }
    void setLineNumber(int line) { lineNumber = line; }
    
    bool operator==(const KeymapEntry& other) const;

protected:
    int lineNumber = -1;
};

// Key binding entry (KEY lines)
class KeyBinding : public KeymapEntry {
private:
    int modifierValue;
    int keyNoteValue;
    std::string actionCommandId;
    Context context;
    KeyType keyType;
    
public:
    KeyBinding(int modifierValue, int keyNoteValue, const std::string& actionCommandId, 
               Context context);
    
    EntryType getType() const override { return EntryType::KEY_BINDING; }
    std::string toString() const override;
    std::string getDescription() const override;
    Context getContext() const override { return context; }
    bool isConflictWith(const KeymapEntry& other) const override;
    std::string getUniqueId() const override;
    
    // Getters
    int getModifierValue() const { return modifierValue; }
    int getKeyNoteValue() const { return keyNoteValue; }
    const std::string& getActionCommandId() const { return actionCommandId; }
    KeyType getKeyType() const { return keyType; }
    
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
};

// Global key binding (special dual-entry format)
class GlobalKeyBinding : public KeymapEntry {
private:
    std::unique_ptr<KeyBinding> actionBinding;
    std::unique_ptr<KeyBinding> scopeBinding;
    GlobalKeyScope scope;
    
public:
    GlobalKeyBinding(std::unique_ptr<KeyBinding> actionBinding,
                    std::unique_ptr<KeyBinding> scopeBinding,
                    GlobalKeyScope scope);
    
    EntryType getType() const override { return EntryType::GLOBAL_KEY_BINDING; }
    std::string toString() const override;
    std::string getDescription() const override;
    Context getContext() const override;
    bool isConflictWith(const KeymapEntry& other) const override;
    std::string getUniqueId() const override;
    
    // Getters
    const KeyBinding* getActionBinding() const { return actionBinding.get(); }
    const KeyBinding* getScopeBinding() const { return scopeBinding.get(); }
    GlobalKeyScope getScope() const { return scope; }
    std::string getKeyComboString() const;
};

// Custom action entry (ACT lines)
class CustomAction : public KeymapEntry {
private:
    int flags;
    Context context;
    std::string actionCommandId;
    std::string description;
    std::vector<std::string> commandSequence;
    
public:
    CustomAction(int flags, Context context, const std::string& actionCommandId,
                const std::string& description, const std::vector<std::string>& commands);
    
    EntryType getType() const override { return EntryType::CUSTOM_ACTION; }
    std::string toString() const override;
    std::string getDescription() const override { return description; }
    Context getContext() const override { return context; }
    bool isConflictWith(const KeymapEntry& other) const override;
    std::string getUniqueId() const override { return actionCommandId; }
    
    // Getters
    const std::string& getActionCommandId() const { return actionCommandId; }
    const std::vector<std::string>& getCommandSequence() const { return commandSequence; }
    int getFlags() const { return flags; }
    
    // Flag interpretation
    bool consolidatesUndoPoints() const { return flags & 1; }
    bool showsInActionsMenu() const { return flags & 2; }
    bool showsAsActiveIfAllComponentsActive() const { return (flags & 16) || (flags & 32); }
};

// Script action entry (SCR lines)
class ScriptAction : public KeymapEntry {
private:
    ScriptBehavior behavior;
    Context context;
    std::string actionCommandId;
    std::string description;
    std::string scriptPath;
    
public:
    ScriptAction(ScriptBehavior behavior, Context context, const std::string& actionCommandId,
                const std::string& description, const std::string& scriptPath);
    
    EntryType getType() const override { return EntryType::SCRIPT_ACTION; }
    std::string toString() const override;
    std::string getDescription() const override { return description; }
    Context getContext() const override { return context; }
    bool isConflictWith(const KeymapEntry& other) const override;
    std::string getUniqueId() const override { return actionCommandId; }
    
    // Getters
    const std::string& getActionCommandId() const { return actionCommandId; }
    const std::string& getScriptPath() const { return scriptPath; }
    ScriptBehavior getBehavior() const { return behavior; }
    
    // Behavior interpretation
    bool consolidatesUndoPoints() const;
    bool showsInActionsMenu() const;
    std::string getBehaviorDescription() const;
};

// Forward declaration for MergeOptions
struct MergeOptions;

// Main parser class with full modification support
class KeymapParser {
private:
    std::vector<std::unique_ptr<KeymapEntry>> entries;
    std::map<int, std::string> parseErrors;
    std::map<std::string, int> actionCommandIdCounts;
    bool modified = false;
    
public:
    // Default constructor
    KeymapParser() = default;
    
    // Move constructor
    KeymapParser(KeymapParser&& other) noexcept
        : entries(std::move(other.entries))
        , parseErrors(std::move(other.parseErrors))
        , actionCommandIdCounts(std::move(other.actionCommandIdCounts))
        , modified(other.modified) {
        other.modified = false;
    }
    
    // Move assignment operator
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
    
    // Delete copy constructor and copy assignment operator
    KeymapParser(const KeymapParser&) = delete;
    KeymapParser& operator=(const KeymapParser&) = delete;
    
    // === PARSING ===
    bool parseFile(const std::string& filePath);
    bool parseString(const std::string& content);
    
    // === READING/QUERYING ===
    const std::vector<std::unique_ptr<KeymapEntry>>& getEntries() const { return entries; }
    std::vector<KeymapEntry*> getEntriesByType(EntryType type) const;
    std::vector<KeymapEntry*> getEntriesByContext(Context context) const;
    KeymapEntry* findEntryByActionCommandId(const std::string& id) const;
    std::vector<KeyBinding*> findKeyBindingsForAction(const std::string& actionId) const;
    std::vector<KeyBinding*> findConflictingKeyBindings() const;
    
    // === MODIFICATION ===
    // Add entries
    void addEntry(std::unique_ptr<KeymapEntry> entry);
    void addKeyBinding(int modifierValue, int keyNoteValue, 
                      const std::string& actionCommandId, Context context);
    void addCustomAction(int flags, Context context, const std::string& actionCommandId,
                        const std::string& description, const std::vector<std::string>& commands);
    void addScriptAction(ScriptBehavior behavior, Context context, 
                        const std::string& actionCommandId, const std::string& description,
                        const std::string& scriptPath);
    
    // Remove entries
    bool removeEntry(const std::string& uniqueId);
    bool removeEntryByIndex(size_t index);
    size_t removeEntriesByType(EntryType type);
    size_t removeEntriesByContext(Context context);
    size_t removeEntriesMatching(std::function<bool(const KeymapEntry*)> predicate);
    
    // Update entries
    bool updateEntry(const std::string& uniqueId, std::unique_ptr<KeymapEntry> newEntry);
    bool replaceKeyBinding(const std::string& actionCommandId, Context context,
                          int newModifierValue, int newKeyNoteValue);
    
    // Bulk operations
    void mergeFrom(const KeymapParser& other, const MergeOptions& options);
    void replaceEntriesOfType(EntryType type, const std::vector<std::unique_ptr<KeymapEntry>>& newEntries);
    
    // === WRITING ===
    bool writeToFile(const std::string& filePath) const;
    std::string toString() const;
    
    // === STATE MANAGEMENT ===
    bool isModified() const { return modified; }
    void markAsModified() { modified = true; }
    void markAsClean() { modified = false; }
    void clear();
    
    // === VALIDATION ===
    const std::map<int, std::string>& getParseErrors() const { return parseErrors; }
    bool hasErrors() const { return !parseErrors.empty(); }
    std::vector<std::string> validateEntries() const;
    
    // === STATISTICS ===
    size_t getEntryCount() const { return entries.size(); }
    std::map<EntryType, int> getEntryCountsByType() const;
    std::map<Context, int> getEntryCountsByContext() const;
    
private:
    // Internal parsing methods
    std::unique_ptr<KeymapEntry> parseLine(const std::string& line, int lineNumber);
    std::unique_ptr<KeyBinding> parseKeyLine(const std::string& line, int lineNumber);
    std::unique_ptr<CustomAction> parseActLine(const std::string& line, int lineNumber);
    std::unique_ptr<ScriptAction> parseScrLine(const std::string& line, int lineNumber);
    
    // Global shortcut detection and parsing
    bool isGlobalShortcutPair(size_t index) const;
    std::unique_ptr<GlobalKeyBinding> parseGlobalShortcut(size_t& index);
    
    // Helper methods
    Context parseContext(int contextValue) const;
    std::string contextToString(Context context) const;
    std::vector<std::string> tokenizeLine(const std::string& line) const;
    std::string parseQuotedString(const std::string& input, size_t& pos) const;
    void addParseError(int lineNumber, const std::string& error);
    
    // Validation helpers
    bool isValidActionCommandId(const std::string& id) const;
    bool isValidContext(Context context) const;
    void updateActionCommandIdCounts();
    void validateUniqueIds();
};

// Enhanced merger with detailed control
struct MergeOptions {
    // What to preserve from user keymap
    bool preserveUserCustomActions = true;
    bool preserveUserKeyBindings = true;
    bool preserveUserScripts = true;
    bool preserveUserGlobalShortcuts = true;
    
    // Conflict resolution
    bool reportConflicts = true;
    bool autoResolveConflicts = false;
    
    // How to handle conflicts
    enum class ConflictResolution {
        PREFER_USER,        // Keep user's version
        PREFER_OSARA,       // Use OSARA's version
        RENAME_DUPLICATE,   // Rename conflicting entry
        SKIP_DUPLICATE      // Skip the conflicting entry
    } conflictResolution = ConflictResolution::PREFER_USER;
    
    // Context filtering
    std::set<Context> includeContexts; // Empty = include all
    std::set<Context> excludeContexts;
    
    // Entry type filtering
    std::set<EntryType> includeTypes; // Empty = include all
    std::set<EntryType> excludeTypes;
    
    // Custom filtering
    std::function<bool(const KeymapEntry*)> customFilter;
};

struct ConflictInfo {
    enum class ConflictType {
        DUPLICATE_ACTION_COMMAND_ID,
        DUPLICATE_KEY_BINDING,
        DUPLICATE_GLOBAL_SHORTCUT,
        INCOMPATIBLE_CONTEXTS
    } type;
    
    std::string description;
    KeymapEntry* userEntry = nullptr;
    KeymapEntry* osaraEntry = nullptr;
    std::string resolution;
    bool resolved = false;
};

struct MergeResult {
    KeymapParser mergedKeymap;
    std::vector<ConflictInfo> conflicts;
    std::vector<std::string> warnings;
    std::vector<std::string> additions;
    std::vector<std::string> removals;
    bool success = false;
    
    // Statistics
    int userEntriesPreserved = 0;
    int osaraEntriesAdded = 0;
    int conflictsResolved = 0;
    int conflictsUnresolved = 0;
};

class KeymapMerger {
public:
    // Main merge function
    static MergeResult merge(const KeymapParser& userKeymap, 
                           const KeymapParser& osaraKeymap,
                           const MergeOptions& options = MergeOptions{});
    
    // Conflict analysis (without merging)
    static std::vector<ConflictInfo> analyzeConflicts(const KeymapParser& userKeymap,
                                                     const KeymapParser& osaraKeymap);
    
    // Selective merging
    static MergeResult mergeSpecificEntries(const KeymapParser& userKeymap,
                                          const KeymapParser& osaraKeymap,
                                          const std::vector<std::string>& entriesToMerge,
                                          const MergeOptions& options = MergeOptions{});
    
private:
    static void detectConflicts(const KeymapParser& userKeymap,
                              const KeymapParser& osaraKeymap,
                              std::vector<ConflictInfo>& conflicts);
    static void resolveConflicts(std::vector<ConflictInfo>& conflicts,
                               const MergeOptions& options);
    static std::string generateUniqueActionCommandId(const std::string& baseId,
                                                    const KeymapParser& keymap);
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
    
    // Entry manipulation
    std::unique_ptr<KeymapEntry> cloneEntry(const KeymapEntry* entry);
    bool areEntriesEquivalent(const KeymapEntry* entry1, const KeymapEntry* entry2);
    
    // Key encoding/decoding
    std::string decodeKeyCombo(int modifierValue, int keyNoteValue);
    std::pair<int, int> encodeKeyCombo(const std::string& keyCombo);
    bool isValidKeyCombo(int modifierValue, int keyNoteValue);
    
    // Context utilities
    std::string contextToDisplayName(Context context);
    Context displayNameToContext(const std::string& name);
    std::vector<Context> getAllValidContexts();
    
    // Validation
    bool isValidActionCommandId(const std::string& id);
    bool isValidScriptPath(const std::string& path);
    std::vector<std::string> validateKeymap(const KeymapParser& keymap);
}

} // namespace KeymapParser
