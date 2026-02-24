#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <functional>
#include "KeymapParser.h"

namespace KeymapMerger {

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
    std::set<KeymapParser::Context> includeContexts; // Empty = include all
    std::set<KeymapParser::Context> excludeContexts;
    
    // Entry type filtering
    std::set<KeymapParser::EntryType> includeTypes; // Empty = include all
    std::set<KeymapParser::EntryType> excludeTypes;
    
    // Custom filtering
    std::function<bool(const KeymapParser::KeymapEntry*)> customFilter;
};

struct ConflictInfo {
    enum class ConflictType {
        DUPLICATE_ACTION_COMMAND_ID,
        DUPLICATE_KEY_BINDING,
        DUPLICATE_GLOBAL_SHORTCUT,
        INCOMPATIBLE_CONTEXTS
    } type;
    
    std::string description;
    KeymapParser::KeymapEntry* userEntry = nullptr;
    KeymapParser::KeymapEntry* osaraEntry = nullptr;
    std::string resolution;
    bool resolved = false;
    // Whether this entry should be pre-checked in the merge dialog.
    // Add and Update entries are pre-checked (low risk); Replace entries are
    // not (the user must explicitly choose to overwrite an existing binding).
    bool defaultSelected = false;
    // UIDs of companion entries (e.g. the ACT/SCR definition for a KEY binding)
    // that should be merged automatically when this entry is selected.
    // These entries are suppressed from the displayed list so the user sees only
    // one row for what is logically a single change.
    std::vector<std::string> impliedEntryIds;
};

struct MergeResult {
    KeymapParser::KeymapParser mergedKeymap;
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
    static MergeResult merge(const KeymapParser::KeymapParser& userKeymap, 
                           const KeymapParser::KeymapParser& osaraKeymap,
                           const MergeOptions& options = MergeOptions{});
    
    // Conflict analysis (without merging)
    static std::vector<ConflictInfo> analyzeConflicts(const KeymapParser::KeymapParser& userKeymap,
                                                     const KeymapParser::KeymapParser& osaraKeymap);
    
    // Selective merging
    static MergeResult mergeSpecificEntries(const KeymapParser::KeymapParser& userKeymap,
                                          const KeymapParser::KeymapParser& osaraKeymap,
                                          const std::vector<std::string>& entriesToMerge,
                                          const MergeOptions& options = MergeOptions{});
    
private:
    static void detectConflicts(const KeymapParser::KeymapParser& userKeymap,
                              const KeymapParser::KeymapParser& osaraKeymap,
                              std::vector<ConflictInfo>& conflicts);
    static void resolveConflicts(std::vector<ConflictInfo>& conflicts,
                               const MergeOptions& options);
    static std::string generateUniqueActionCommandId(const std::string& baseId,
                                                    const KeymapParser::KeymapParser& keymap);
};

} // namespace KeymapMerger
