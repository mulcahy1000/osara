#include "KeymapMerger.h"
#include <algorithm>
#include <sstream>
#include <set>

namespace KeymapMerger {

// ---------------------------------------------------------------------------
// analyzeConflicts
//
// Returns one ConflictInfo per OSARA entry that is either:
//   * New     – its unique ID is not present in the user keymap at all.
//   * Conflict – its unique ID IS present but the serialised content differs.
//
// Entries that are identical in both keymaps are silently skipped.
// ---------------------------------------------------------------------------
// Returns true when a KeyBinding entry has action command ID "0", meaning
// OSARA wants to explicitly disable that key slot.  Such entries are skipped
// entirely: they add nothing useful when the user doesn't have the key, and
// asking the user to voluntarily remove an existing binding is confusing.
static bool isKeyDisable(const KeymapParser::KeymapEntry* entry)
{
    auto* kb = dynamic_cast<const KeymapParser::KeyBinding*>(entry);
    return kb && kb->getActionCommandId() == "0";
}

// Returns a human-readable display name for a keymap entry.
// KEY lines have no description field; their comment (e.g.
// "# Main : R : OVERRIDE DEFAULT : Transport: Toggle repeat") is the only
// human-readable label.  Prefer the comment text over getDescription(), which
// for KeyBinding returns something like "Shift+R -> 40332".
static std::string entryDisplayName(const KeymapParser::KeymapEntry* entry)
{
    const std::string& comment = entry->getComment();
    if (!comment.empty())
    {
        size_t hashPos = comment.find('#');
        if (hashPos != std::string::npos)
        {
            // Skip "# " to reach the descriptive text.
            size_t textStart = hashPos + 1;
            while (textStart < comment.size() && comment[textStart] == ' ')
                ++textStart;
            if (textStart < comment.size())
                return comment.substr(textStart);
        }
    }
    // For KEY entries without a comment: REAPER assigns pure-number action IDs
    // (e.g. 40525) to built-in actions.  Those numbers are meaningless to users.
    // Return just the key combo string instead so "currently: -" is shown
    // rather than the cryptic "currently: - -> 40525".
    // Named action IDs (e.g. OSARA_REPORTRIPPLE) are kept because they are readable.
    auto* kb = dynamic_cast<const KeymapParser::KeyBinding*>(entry);
    if (kb)
    {
        const std::string& actionId = kb->getActionCommandId();
        bool isPureNumber = !actionId.empty() &&
            std::all_of(actionId.begin(), actionId.end(),
                        [](unsigned char c){ return std::isdigit(c); });
        if (isPureNumber)
            return kb->getKeyComboString();
    }
    return entry->getDescription();
}

// Strip trailing comments from a serialised entry string before comparing.
// toString() appends comments with a "\t\t" separator.  Entries that are
// semantically identical but carry different (or absent) comments must not
// be shown to the user as conflicts.
// GlobalKeyBinding serialises to two lines, so we handle each line.
static std::string stripEntryComment(const std::string& s)
{
    std::string result;
    size_t start = 0;
    bool first = true;
    while (true) {
        size_t nl = s.find('\n', start);
        std::string line = (nl == std::string::npos)
            ? s.substr(start) : s.substr(start, nl - start);
        size_t tab = line.find("\t\t");
        if (tab != std::string::npos) line.resize(tab);
        if (!first) result += '\n';
        result += line;
        first = false;
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return result;
}

std::vector<ConflictInfo>
KeymapMerger::analyzeConflicts(const KeymapParser::KeymapParser& userKeymap,
                                const KeymapParser::KeymapParser& osaraKeymap)
{
    std::vector<ConflictInfo> result;

    for (const auto& osaraEntry : osaraKeymap.getEntries())
    {
        const std::string uid = osaraEntry->getUniqueId();

        // Search user keymap for an entry with the same unique ID.
        const KeymapParser::KeymapEntry* userMatch = nullptr;
        for (const auto& ue : userKeymap.getEntries())
        {
            if (ue->getUniqueId() == uid)
            {
                userMatch = ue.get();
                break;
            }
        }

        // Skip all entries where OSARA action=0 (explicit key-disable).
        // These either add nothing useful (user doesn't have the key) or ask
        // the user to voluntarily remove their own binding, which is confusing.
        if (isKeyDisable(osaraEntry.get())) continue;

        if (!userMatch)
        {
            // New entry – not present in user keymap at all.
            ConflictInfo info;
            info.type            = ConflictInfo::ConflictType::DUPLICATE_ACTION_COMMAND_ID;
            info.osaraEntry      = const_cast<KeymapParser::KeymapEntry*>(osaraEntry.get());
            info.description     = "Add: " + entryDisplayName(osaraEntry.get());
            info.resolved        = false;
            info.defaultSelected = true; // safe to add by default
            result.push_back(std::move(info));
        }
        else if (stripEntryComment(userMatch->toString()) != stripEntryComment(osaraEntry->toString()))
        {
            // Same unique ID but different content.
            ConflictInfo info;
            info.type       = ConflictInfo::ConflictType::DUPLICATE_KEY_BINDING;
            info.userEntry  = const_cast<KeymapParser::KeymapEntry*>(userMatch);
            info.osaraEntry = const_cast<KeymapParser::KeymapEntry*>(osaraEntry.get());
            info.resolved   = false;

            const std::string osaraLabel = entryDisplayName(osaraEntry.get());
            const std::string userLabel  = entryDisplayName(userMatch);
            if (osaraLabel == userLabel)
            {
                // Same human-readable description: the difference is internal
                // (e.g. a custom action's command sequence changed).
                // Pre-check these — the user would almost always want the update.
                info.description     = "Update: " + osaraLabel;
                info.defaultSelected = true;
            }
            else
            {
                // Different descriptions: OSARA wants to bind this key/action
                // to something the user currently has set differently.
                // Leave unchecked so the user must explicitly decide.
                info.description     = "Replace: " + osaraLabel
                                     + "  (currently: " + userLabel + ")";
                info.defaultSelected = false;
            }
            result.push_back(std::move(info));
        }
        // Identical entries are silently skipped – nothing to offer the user.
    }

    // Post-processing: suppress ACT/SCR entries that are implied by a KEY entry
    // already in the result.  Custom actions in OSARA are always paired with a
    // KEY binding that assigns them a keyboard shortcut.  Showing both rows is
    // confusing to users because (a) the KEY description is more informative
    // (it includes the key combo), and (b) it looks like two separate changes
    // when it is really one.  Instead, store the ACT UID in the KEY entry's
    // impliedEntryIds so the merger can install both automatically when the
    // user selects the KEY row.

    // Build a set of all ACT/SCR UIDs currently in the result.
    std::set<std::string> actUidsInResult;
    for (const auto& info : result)
    {
        if (!info.osaraEntry) continue;
        const std::string& uid = info.osaraEntry->getUniqueId();
        // KEY entries have UIDs of the form "KEY_<mod>_<key>_<ctx>".
        if (uid.rfind("KEY_", 0) != 0)
            actUidsInResult.insert(uid);
    }

    // For each KEY entry whose action GUID also appears as a result entry,
    // link them and mark the ACT for suppression.
    std::set<std::string> suppressedActUids;
    for (auto& info : result)
    {
        if (!info.osaraEntry) continue;
        auto* kb = dynamic_cast<KeymapParser::KeyBinding*>(info.osaraEntry);
        if (!kb) continue;
        const std::string& actionId = kb->getActionCommandId();
        if (actUidsInResult.count(actionId))
        {
            info.impliedEntryIds.push_back(actionId);
            suppressedActUids.insert(actionId);
        }
    }

    // Remove suppressed ACT/SCR entries.
    result.erase(
        std::remove_if(result.begin(), result.end(),
            [&](const ConflictInfo& info)
            {
                return info.osaraEntry &&
                       suppressedActUids.count(info.osaraEntry->getUniqueId());
            }),
        result.end());

    return result;
}

// ---------------------------------------------------------------------------
// merge  (full merge – not used by the installer UI, provided for completeness)
// ---------------------------------------------------------------------------
MergeResult
KeymapMerger::merge(const KeymapParser::KeymapParser& /*userKeymap*/,
                    const KeymapParser::KeymapParser& /*osaraKeymap*/,
                    const MergeOptions& /*options*/)
{
    MergeResult result;
    result.success = false;
    result.warnings.push_back("Full merge() not implemented; use mergeSpecificEntries().");
    return result;
}

// ---------------------------------------------------------------------------
// mergeSpecificEntries
//
// Produces a merged keymap by starting from the user keymap, removing any
// conflicting entries for the chosen IDs, then appending the OSARA versions.
// ---------------------------------------------------------------------------
MergeResult
KeymapMerger::mergeSpecificEntries(const KeymapParser::KeymapParser& userKeymap,
                                   const KeymapParser::KeymapParser& osaraKeymap,
                                   const std::vector<std::string>& entriesToMerge,
                                   const MergeOptions& /*options*/)
{
    MergeResult result;
    result.success = false;

    // The merged keymap starts as the serialised user keymap rebuilt via the
    // parser so that all original entries are preserved.  We remove conflicting
    // entries before appending the selected OSARA ones.
    //
    // NOTE: KeymapParser has no copy constructor, so we re-parse from the
    // serialised string to produce a mutable working copy.
    if (!result.mergedKeymap.parseString(userKeymap.toString()))
    {
        result.warnings.push_back("Failed to re-parse user keymap during merge.");
        return result;
    }

    for (const auto& id : entriesToMerge)
    {
        // Remove any user entry that occupies the same slot (key combo / action
        // GUID).  This is a no-op for genuinely new entries.
        result.mergedKeymap.removeEntry(id);
        result.removals.push_back(id);
    }

    // Build the final merged content: user entries (minus removed conflicts)
    // followed by the selected OSARA entries, then re-parse the whole thing
    // so that result.mergedKeymap is fully populated.
    std::string mergedStr = result.mergedKeymap.toString();

    for (const auto& id : entriesToMerge)
    {
        for (const auto& entry : osaraKeymap.getEntries())
        {
            if (entry->getUniqueId() == id)
            {
                mergedStr += entry->toString() + "\n";
                result.additions.push_back(id);
                ++result.osaraEntriesAdded;
                break;
            }
        }
    }

    result.mergedKeymap.parseString(mergedStr);
    result.success = true;
    return result;
}

// ---------------------------------------------------------------------------
// Private helpers (stubs – conflict detection is inlined in analyzeConflicts)
// ---------------------------------------------------------------------------
void KeymapMerger::detectConflicts(const KeymapParser::KeymapParser& /*userKeymap*/,
                                   const KeymapParser::KeymapParser& /*osaraKeymap*/,
                                   std::vector<ConflictInfo>& /*conflicts*/) {}

void KeymapMerger::resolveConflicts(std::vector<ConflictInfo>& /*conflicts*/,
                                    const MergeOptions& /*options*/) {}

std::string
KeymapMerger::generateUniqueActionCommandId(const std::string& baseId,
                                            const KeymapParser::KeymapParser& keymap)
{
    if (!keymap.findEntryByActionCommandId(baseId))
        return baseId;

    for (int suffix = 2; suffix < 10000; ++suffix)
    {
        std::ostringstream candidate;
        candidate << baseId << "_" << suffix;
        if (!keymap.findEntryByActionCommandId(candidate.str()))
            return candidate.str();
    }
    return baseId; // fallback
}

} // namespace KeymapMerger
