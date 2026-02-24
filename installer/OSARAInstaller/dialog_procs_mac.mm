#include "installer_app.h"
#include "resource.h"
#include "../../src/translation.h"
#include <set>
#include <string>
#include <vector>

#include "swell.h"
#import <Cocoa/Cocoa.h>

// Mac-specific error dialog using native Cocoa
void mac_show_error(HWND hwnd, const char* message)
{
    // Use SWELL's MessageBox for consistency with the rest of the UI
    MessageBox(hwnd, message, translate("OSARA Installer Error"), MB_OK | MB_ICONERROR);
}

// Mac-specific confirmation dialog using native Cocoa
bool mac_confirm_cancel(HWND hwnd)
{
    bool isUninstall = g_installer &&
        g_installer->GetState().operationMode == MODE_UNINSTALL;
    const char* message = isUninstall
        ? translate("Are you sure you want to cancel the uninstallation?")
        : translate("Are you sure you want to cancel the installation?");
    const char* title = isUninstall
        ? translate("Cancel Uninstallation")
        : translate("Cancel Installation");
    int result = MessageBox(hwnd, message, title, MB_YESNO | MB_ICONQUESTION);
    return (result == IDYES);
}

// Check whether REAPER is currently running using the Cocoa workspace API.
// This avoids forking a shell process and doesn't depend on pgrep being in PATH.
bool mac_is_reaper_running()
{
    NSArray<NSRunningApplication*>* apps =
        [[NSWorkspace sharedWorkspace] runningApplications];
    for (NSRunningApplication* app in apps)
    {
        if ([app.bundleIdentifier isEqualToString:@"com.cockos.reaper"])
            return true;
    }
    return false;
}

// Mac-specific app termination
void mac_terminate_app()
{
    // On macOS, properly terminate the app when user chooses to quit
    [NSApp terminate:nil];
}

// Mac-specific folder browser using native NSOpenPanel
bool mac_browse_for_folder(HWND hwnd, std::string& selectedPath)
{
    // Use native macOS folder selection dialog
    NSOpenPanel* openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseFiles:NO];
    [openPanel setCanChooseDirectories:YES];
    [openPanel setAllowsMultipleSelection:NO];
    [openPanel setTitle:@"Select REAPER Folder"];
    [openPanel setMessage:@"Choose the REAPER folder for portable installation"];

    // Default to the user's home directory.  Portable REAPER installations
    // live wherever the user chose to put them, so starting at the home
    // directory is a better neutral starting point than /Applications (which
    // is where REAPER.app lives, not its data folder).
    [openPanel setDirectoryURL:[NSURL fileURLWithPath:NSHomeDirectory()]];

    NSModalResponse result = [openPanel runModal];
    if (result == NSModalResponseOK)
    {
        NSURL* selectedURL = [[openPanel URLs] objectAtIndex:0];
        NSString* selectedPathNS = [selectedURL path];

        // Convert NSString to C++ string
        const char* pathCStr = [selectedPathNS UTF8String];
        selectedPath = pathCStr;
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Native macOS keymap merge dialog
//
// The SWELL multi-select LISTBOX translates to an NSTableView with
// allowsMultipleSelection = YES on macOS.  This gives VoiceOver no way to
// announce "checked / unchecked" state, and the Space bar does not toggle
// items.
//
// This file implements a replacement native Cocoa dialog that uses a single
// NSTableView column whose data cell is an NSButtonCell (NSSwitchButton /
// checkbox).  Cocoa exposes NSButtonCell checkboxes to VoiceOver as
// NSAccessibilityCheckBoxRole, so VoiceOver announces each row as
// "[description], checked" or "[description], unchecked, checkbox".
// VO+Space (or plain Space, handled in MergeTableView below) toggles the row.
//
// Navigation:
//   • Arrow keys   – move focus within the table
//   • Space        – toggle focused row (MergeTableView.keyDown:)
//   • VO+Space     – VoiceOver activates the checkbox (native NSButtonCell)
//   • Tab / Shift-Tab – move between table and buttons
//   • Return       – Continue (default button)
//   • Escape       – Cancel
// ---------------------------------------------------------------------------

// ---- MergeEntry: one row in the checkbox table ----------------------------

@interface MergeEntry : NSObject
@property (nonatomic, copy)   NSString              *label;
@property (nonatomic, copy)   NSString              *uniqueId;
@property (nonatomic, assign) BOOL                   selected;
// UIDs of companion entries merged automatically when this entry is selected
// (e.g. the ACT/SCR action definition behind a KEY binding row).
@property (nonatomic, strong) NSArray<NSString *>   *impliedIds;
@end
@implementation MergeEntry
- (void)dealloc
{
    [_label release];
    [_uniqueId release];
    [_impliedIds release];
    [super dealloc];
}
@end

// ---- MergeTableView: NSTableView that routes Space bar to the data source --

@interface MergeTableView : NSTableView
@end

@implementation MergeTableView
- (void)keyDown:(NSEvent *)event
{
    // Route Space bar to the data source's setObjectValue path, which toggles
    // the checkbox for the focused row.
    if ([[event characters] isEqualToString:@" "])
    {
        NSInteger row = [self selectedRow];
        if (row >= 0 && [[self dataSource]
                respondsToSelector:@selector(tableView:setObjectValue:forTableColumn:row:)])
        {
            [[self dataSource] tableView:self
                          setObjectValue:nil   // nil signals "toggle"
                          forTableColumn:[[self tableColumns] firstObject]
                                     row:row];
        }
    }
    // Tab / Shift-Tab: advance the key-view loop instead of letting the table
    // attempt to traverse editable cells.  Use keyCode (not characters) because
    // Shift-Tab produces characters="\x19" (ASCII 25), not "\t".
    else if ([event keyCode] == 48)
    {
        BOOL shift = ([event modifierFlags] & NSEventModifierFlagShift) != 0;
        if (shift)
            [[self window] selectPreviousKeyView:self];
        else
            [[self window] selectNextKeyView:self];
    }
    else
    {
        [super keyDown:event];
    }
}
@end

// ---- MergeTableController: NSTableViewDataSource + NSTableViewDelegate -----

@interface MergeTableController : NSObject <NSTableViewDataSource, NSTableViewDelegate>
@property (nonatomic, strong) NSMutableArray<MergeEntry *> *entries;
@property (nonatomic, assign) NSTableView                  *tableView; // outlives controller
@end

@implementation MergeTableController

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tv
{
    return (NSInteger)[self.entries count];
}

// Returns the NSNumber BOOL state (drives NSButtonCell checked / unchecked).
- (id)tableView:(NSTableView *)tv
    objectValueForTableColumn:(NSTableColumn *)col
                          row:(NSInteger)row
{
    if (row < 0 || row >= (NSInteger)[self.entries count]) return @NO;
    return @(self.entries[(NSUInteger)row].selected);
}

// Sets per-row title on the prototype cell before it is displayed.
// This is what makes VoiceOver read the description alongside the state.
- (void)tableView:(NSTableView *)tv
  willDisplayCell:(id)cell
   forTableColumn:(NSTableColumn *)col
              row:(NSInteger)row
{
    if (![cell isKindOfClass:[NSButtonCell class]]) return;
    if (row < 0 || row >= (NSInteger)[self.entries count]) return;
    [(NSButtonCell *)cell setTitle:self.entries[(NSUInteger)row].label];
}

// Called on mouse click (obj = new NSNumber) or Space key (obj = nil, toggle).
- (void)tableView:(NSTableView *)tv
   setObjectValue:(id)obj
   forTableColumn:(NSTableColumn *)col
              row:(NSInteger)row
{
    if (row < 0 || row >= (NSInteger)[self.entries count]) return;
    MergeEntry *e = self.entries[(NSUInteger)row];
    e.selected = !e.selected;
    [tv reloadDataForRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)row]
                  columnIndexes:[NSIndexSet indexSetWithIndex:0]];
    // Notify VoiceOver that the focused element's value changed so it
    // announces "checked" / "unchecked" after a Space-bar toggle.
    id focused = [NSApp accessibilityFocusedUIElement];
    if (focused)
        NSAccessibilityPostNotification(focused,
            NSAccessibilityValueChangedNotification);
}

- (void)dealloc
{
    [_entries release];
    [super dealloc];
}

@end

// ---- MergeButtonCoordinator: button actions + NSWindowDelegate -------------
//
// Cancel shows a sheet-modal confirmation attached to the merge window so that
// the native panel stays open if the user decides not to cancel.

@interface MergeButtonCoordinator : NSObject <NSWindowDelegate>
// Two separate entry arrays: pre-checked safe items and unchecked conflicts.
@property (nonatomic, strong) NSMutableArray<MergeEntry *> *safeEntries;
@property (nonatomic, strong) NSMutableArray<MergeEntry *> *conflictEntries;
@property (nonatomic, assign) NSTableView                  *safeTableView;     // outlives coordinator
@property (nonatomic, assign) NSTableView                  *conflictTableView; // outlives coordinator
@property (nonatomic, assign) NSWindow                     *window;            // outlives coordinator
@end

@implementation MergeButtonCoordinator

- (void)doContinue:(id)sender
{
    [NSApp stopModalWithCode:NSModalResponseOK];
}

- (void)doBack:(id)sender
{
    // NSModalResponseStop (-1000) is repurposed as "Back" here.
    [NSApp stopModalWithCode:NSModalResponseStop];
}

- (void)doSelectAll:(id)sender
{
    for (MergeEntry *e in self.safeEntries)     e.selected = YES;
    for (MergeEntry *e in self.conflictEntries) e.selected = YES;
    [self.safeTableView     reloadData];
    [self.conflictTableView reloadData];
}

- (void)doDeselectAll:(id)sender
{
    for (MergeEntry *e in self.safeEntries)     e.selected = NO;
    for (MergeEntry *e in self.conflictEntries) e.selected = NO;
    [self.safeTableView     reloadData];
    [self.conflictTableView reloadData];
}

// Shows a sheet-modal confirmation; stops the modal only if the user confirms.
// Using a sheet avoids nesting runModalForWindow: calls (which is unsupported).
- (void)doCancel:(id)sender
{
    bool isUninstall = g_installer &&
        g_installer->GetState().operationMode == MODE_UNINSTALL;
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:(isUninstall
        ? @"Are you sure you want to cancel the uninstallation?"
        : @"Are you sure you want to cancel the installation?")];
    [alert addButtonWithTitle:@"Yes"];
    [alert addButtonWithTitle:@"No"];
    [alert setAlertStyle:NSAlertStyleWarning];
    // Sheet attaches to the merge window; the panel stays open while it's shown.
    [alert beginSheetModalForWindow:self.window
                  completionHandler:^(NSModalResponse r) {
        if (r == NSAlertFirstButtonReturn) // "Yes" – confirmed
            [NSApp stopModalWithCode:2];   // 2 = cancel confirmed
        // "No" → do nothing; the merge panel remains open
    }];
    [alert release];
}

// Red close-button: treat as Cancel (shows sheet confirmation).
- (BOOL)windowShouldClose:(NSWindow *)sender
{
    [self doCancel:sender];
    return NO; // never close directly; doCancel stops the modal if confirmed
}

- (void)dealloc
{
    [_safeEntries release];
    [_conflictEntries release];
    [super dealloc];
}

@end

// ---- mac_run_keymap_merge_panel: builds and runs the window modally --------
//
// Two stacked checkbox tables are shown:
//   • "New and Updated Entries"  (safeEntries,    all pre-checked)
//   • "Conflicts"                (conflictEntries, all unchecked)
// This lets users Tab past the safe table quickly and focus on conflicts.

static MergeDialogAction mac_run_keymap_merge_panel(
        NSMutableArray<MergeEntry *> *safeEntries,
        NSMutableArray<MergeEntry *> *conflictEntries,
        NSString *statusText)
{
    @autoreleasepool
    {
    const CGFloat W = 540, H = 560, M = 16;
    const CGFloat innerW = W - 2*M;

    NSWindow *win = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, W, H)
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [win setTitle:@"Merge OSARA Keymap"];
    [win setReleasedWhenClosed:NO]; // we release manually below; avoids double-free
    [win center];
    // Disable automatic key-view-loop recalculation so that NSScroller subviews
    // inside the scroll views are never inserted into our explicit Tab order.
    [win setAutorecalculatesKeyViewLoop:NO];

    NSView *content = [win contentView];

    // -- Instruction label (top) --
    NSTextField *instrLabel = [NSTextField wrappingLabelWithString:
        @"Add and Update entries are pre-checked. "
        "Replace entries overwrite an existing binding — "
        "uncheck any you want to keep."];
    instrLabel.frame = NSMakeRect(M, H - M - 40, innerW, 40);
    [instrLabel setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [content addSubview:instrLabel];

    // -- Status label (e.g. "3 to add, 1 conflict.") --
    NSTextField *statusLabel = [NSTextField labelWithString:
        (statusText.length ? statusText : @"")];
    statusLabel.frame = NSMakeRect(M, H - M - 40 - 6 - 18, innerW, 18);
    [statusLabel setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [content addSubview:statusLabel];

    // -- Button row (bottom) --
    const CGFloat btnH = 28, btnW = 86, btnY = M;

    MergeButtonCoordinator *coord = [[MergeButtonCoordinator alloc] init];
    coord.safeEntries     = safeEntries;
    coord.conflictEntries = conflictEntries;
    coord.window          = win;
    [win setDelegate:coord];

    NSButton *cancelBtn   = [NSButton buttonWithTitle:@"Cancel"
                                               target:coord
                                               action:@selector(doCancel:)];
    NSButton *backBtn     = [NSButton buttonWithTitle:@"Back"
                                               target:coord
                                               action:@selector(doBack:)];
    NSButton *deselectBtn = [NSButton buttonWithTitle:@"Deselect All"
                                               target:coord
                                               action:@selector(doDeselectAll:)];
    NSButton *selectBtn   = [NSButton buttonWithTitle:@"Select All"
                                               target:coord
                                               action:@selector(doSelectAll:)];
    NSButton *continueBtn = [NSButton buttonWithTitle:@"Continue"
                                               target:coord
                                               action:@selector(doContinue:)];

    cancelBtn.frame   = NSMakeRect(M,                    btnY, btnW, btnH);
    backBtn.frame     = NSMakeRect(M + btnW + 6,         btnY, btnW, btnH);
    deselectBtn.frame = NSMakeRect(W/2.0 - 92,           btnY, 90,   btnH);
    selectBtn.frame   = NSMakeRect(W/2.0 + 2,            btnY, 88,   btnH);
    continueBtn.frame = NSMakeRect(W - M - btnW,         btnY, btnW, btnH);

    [continueBtn setBezelStyle:NSBezelStyleRounded];
    [continueBtn setKeyEquivalent:@"\r"];   // Return = Continue
    [cancelBtn   setKeyEquivalent:@"\033"]; // Escape = Cancel

    for (NSButton *b in @[cancelBtn, backBtn, deselectBtn, selectBtn, continueBtn])
        [content addSubview:b];

    // -- Hint text above buttons --
    const CGFloat hintY = btnY + btnH + 4;
    const CGFloat hintH = 16;
    NSTextField *hintLabel = [NSTextField labelWithString:
        @"Space or VO+Space to check / uncheck. "
        "No checked entries = no keymap changes."];
    hintLabel.frame = NSMakeRect(M, hintY, innerW, hintH);
    [hintLabel setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [hintLabel setTextColor:[NSColor secondaryLabelColor]];
    [content addSubview:hintLabel];

    // ---- Two-table layout (bottom-up) ----
    //
    // Conflict table (bottom section — needs user attention)
    const CGFloat conflictTableBottom = hintY + hintH + 8;
    const CGFloat conflictTableH      = 190;
    const CGFloat conflictTableTop    = conflictTableBottom + conflictTableH;

    // Conflict section header label (sits directly above the conflict table)
    const CGFloat conflictHdrY   = conflictTableTop + 4;
    const CGFloat conflictHdrH   = 18;
    const CGFloat conflictHdrTop = conflictHdrY + conflictHdrH;

    // Safe table (top section — pre-checked, typically just Tab past)
    const CGFloat safeTableBottom = conflictHdrTop + 6;
    const CGFloat safeTableH      = 160;
    const CGFloat safeTableTop    = safeTableBottom + safeTableH;

    // Safe section header label (sits directly above the safe table)
    const CGFloat safeHdrY = safeTableTop + 4;
    const CGFloat safeHdrH = 18;
    // safeHdrTop = safeHdrY + safeHdrH ≈ 472, status label starts at 480 → 8px gap

    // -- Section headers --
    NSString *safeHdrText = [NSString stringWithFormat:
        @"New and Updated Entries (%ld)", (long)[safeEntries count]];
    NSTextField *safeHdrLabel = [NSTextField labelWithString:safeHdrText];
    safeHdrLabel.frame = NSMakeRect(M, safeHdrY, innerW, safeHdrH);
    [safeHdrLabel setFont:[NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]]];
    [content addSubview:safeHdrLabel];

    NSString *conflictHdrText = [NSString stringWithFormat:
        @"Conflicts (%ld) — Replaces an existing binding", (long)[conflictEntries count]];
    NSTextField *conflictHdrLabel = [NSTextField labelWithString:conflictHdrText];
    conflictHdrLabel.frame = NSMakeRect(M, conflictHdrY, innerW, conflictHdrH);
    [conflictHdrLabel setFont:[NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]]];
    [content addSubview:conflictHdrLabel];

    // ---- Helper to build one checkbox table column ----
    // Used twice (safe and conflict).  Returns the configured column; caller adds to tv.
#define MAKE_CHECKBOX_COLUMN(identifier) ({ \
    NSTableColumn *_col = [[NSTableColumn alloc] initWithIdentifier:(identifier)]; \
    [_col setWidth:innerW - 4]; \
    [_col setEditable:YES]; \
    NSButtonCell *_proto = [[NSButtonCell alloc] init]; \
    [_proto setButtonType:NSSwitchButton]; \
    [_proto setImagePosition:NSImageLeft]; \
    [_proto setControlSize:NSControlSizeRegular]; \
    [_proto setFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]]; \
    [_col setDataCell:_proto]; \
    [_proto release]; \
    _col; \
})

    // -- Safe table --
    MergeTableView *safeTV = [[MergeTableView alloc]
        initWithFrame:NSMakeRect(0, 0, innerW, safeTableH)];
    [safeTV setHeaderView:nil];
    [safeTV setUsesAlternatingRowBackgroundColors:YES];
    [safeTV setAllowsMultipleSelection:NO];
    [safeTV setFocusRingType:NSFocusRingTypeExterior];
    [safeTV setAccessibilityLabel:safeHdrText]; // matches the visible section heading
    NSTableColumn *safeCol = MAKE_CHECKBOX_COLUMN(@"safeEntry");
    [safeTV addTableColumn:safeCol];
    [safeCol release];

    MergeTableController *safeController = [[MergeTableController alloc] init];
    safeController.entries   = safeEntries;
    safeController.tableView = safeTV;
    [safeTV setDataSource:safeController];
    [safeTV setDelegate:safeController];
    coord.safeTableView = safeTV;

    NSScrollView *safeScroll = [[NSScrollView alloc]
        initWithFrame:NSMakeRect(M, safeTableBottom, innerW, safeTableH)];
    [safeScroll setDocumentView:safeTV];
    [safeScroll setHasVerticalScroller:YES];
    [[safeScroll verticalScroller] setRefusesFirstResponder:YES];
    [safeScroll setBorderType:NSBezelBorder];
    [content addSubview:safeScroll];
    [safeScroll release];
    [safeTV release]; // retained by safeScroll

    // -- Conflict table --
    MergeTableView *conflictTV = [[MergeTableView alloc]
        initWithFrame:NSMakeRect(0, 0, innerW, conflictTableH)];
    [conflictTV setHeaderView:nil];
    [conflictTV setUsesAlternatingRowBackgroundColors:YES];
    [conflictTV setAllowsMultipleSelection:NO];
    [conflictTV setFocusRingType:NSFocusRingTypeExterior];
    [conflictTV setAccessibilityLabel:conflictHdrText]; // matches the visible section heading
    NSTableColumn *conflictCol = MAKE_CHECKBOX_COLUMN(@"conflictEntry");
    [conflictTV addTableColumn:conflictCol];
    [conflictCol release];

    MergeTableController *conflictController = [[MergeTableController alloc] init];
    conflictController.entries   = conflictEntries;
    conflictController.tableView = conflictTV;
    [conflictTV setDataSource:conflictController];
    [conflictTV setDelegate:conflictController];
    coord.conflictTableView = conflictTV;

    NSScrollView *conflictScroll = [[NSScrollView alloc]
        initWithFrame:NSMakeRect(M, conflictTableBottom, innerW, conflictTableH)];
    [conflictScroll setDocumentView:conflictTV];
    [conflictScroll setHasVerticalScroller:YES];
    [[conflictScroll verticalScroller] setRefusesFirstResponder:YES];
    [conflictScroll setBorderType:NSBezelBorder];
    [content addSubview:conflictScroll];
    [conflictScroll release];
    [conflictTV release]; // retained by conflictScroll

#undef MAKE_CHECKBOX_COLUMN

    // Wire the key-view loop so Tab/Shift-Tab cycle through both tables then
    // through the buttons and back.  Must be done after all views are created.
    // safeTV → conflictTV → cancelBtn → backBtn → deselectBtn → selectBtn
    //        → continueBtn → safeTV
    safeTV.nextKeyView      = conflictTV;
    conflictTV.nextKeyView  = cancelBtn;
    cancelBtn.nextKeyView   = backBtn;
    backBtn.nextKeyView     = deselectBtn;
    deselectBtn.nextKeyView = selectBtn;
    selectBtn.nextKeyView   = continueBtn;
    continueBtn.nextKeyView = safeTV;

    // Focus the conflict table if it has entries (those need attention);
    // otherwise focus the safe table.
    if ([conflictEntries count] > 0)
        [win makeFirstResponder:conflictTV];
    else
        [win makeFirstResponder:safeTV];

    // Run the modal event loop.
    // runModalForWindow: is correct here; mac_browse_for_folder uses the same
    // pattern and SWELL's Cocoa application supports it.
    NSModalResponse resp = [NSApp runModalForWindow:win];

    [win setDelegate:nil]; // clear before releasing coord to avoid dangling pointer
    [win close];
    [safeController     release];
    [conflictController release];
    [coord release];
    [win  release];

    if (resp == NSModalResponseOK)   return MergeDialogAction::Continue;
    if (resp == 2)                   return MergeDialogAction::Cancel;
    return MergeDialogAction::Back;  // NSModalResponseStop or anything else
    } // @autoreleasepool
}

// ---- mac_schedule_merge_dialog: entry point called from dialog_procs.cpp ---
//
// Called from KeymapMergeDlgProc WM_INITDIALOG on macOS.  Schedules the
// native panel on the next run-loop tick so the SWELL ShowWindow / SetFocus
// machinery completes first.  On return, writes the user's selections to
// g_installer state and navigates forward or back.

void mac_schedule_merge_dialog(HWND hwnd,
    const std::vector<std::string>& itemIds,
    const std::vector<std::string>& descriptions,
    const std::vector<std::vector<std::string>>& impliedIds,
    const std::vector<bool>& defaultSelected,
    const std::string& statusText)
{
    // Split entries into two tables:
    //   safeEntries     — Add/Update (defaultSelected=true,  pre-checked)
    //   conflictEntries — Replace    (defaultSelected=false, unchecked)
    NSMutableArray<MergeEntry *> *safeEntries     = [NSMutableArray array];
    NSMutableArray<MergeEntry *> *conflictEntries = [NSMutableArray array];

    for (size_t i = 0; i < itemIds.size() && i < descriptions.size(); ++i)
    {
        MergeEntry *e = [[[MergeEntry alloc] init] autorelease];
        e.label    = [NSString stringWithUTF8String:descriptions[i].c_str()];
        e.uniqueId = [NSString stringWithUTF8String:itemIds[i].c_str()];
        bool isSafe = (i < defaultSelected.size() && defaultSelected[i]);
        e.selected = isSafe ? YES : NO;

        // Convert any companion (implied) entry IDs to an NSArray.
        if (i < impliedIds.size() && !impliedIds[i].empty())
        {
            NSMutableArray<NSString *> *arr =
                [NSMutableArray arrayWithCapacity:impliedIds[i].size()];
            for (const auto& s : impliedIds[i])
                [arr addObject:[NSString stringWithUTF8String:s.c_str()]];
            e.impliedIds = arr;
        }
        else
        {
            e.impliedIds = @[];
        }

        if (isSafe)
            [safeEntries addObject:e];
        else
            [conflictEntries addObject:e];
    }
    NSString *status = [NSString stringWithUTF8String:statusText.c_str()];

    dispatch_async(dispatch_get_main_queue(), ^{
        MergeDialogAction action =
            mac_run_keymap_merge_panel(safeEntries, conflictEntries, status);

        switch (action)
        {
            case MergeDialogAction::Continue:
                if (g_installer)
                {
                    // Collect checked IDs from both tables.
                    // For entries that imply companion IDs (e.g. a KEY row that
                    // covers a suppressed ACT entry), include those too.
                    // Use a set to prevent duplicates when multiple KEY entries
                    // share the same companion ACT/SCR action definition.
                    auto& selectedIds =
                        g_installer->GetState().selectedMergeEntryIds;
                    selectedIds.clear();
                    std::set<std::string> addedIds;

                    for (NSArray<MergeEntry *> *arr in
                            @[safeEntries, conflictEntries])
                    {
                        for (MergeEntry *e in arr)
                        {
                            if (!e.selected) continue;
                            std::string uid = [e.uniqueId UTF8String];
                            if (addedIds.insert(uid).second)
                                selectedIds.push_back(uid);
                            for (NSString *implied in e.impliedIds)
                            {
                                std::string impliedStr = [implied UTF8String];
                                if (addedIds.insert(impliedStr).second)
                                    selectedIds.push_back(impliedStr);
                            }
                        }
                    }
                    g_installer->PerformInstallation();
                    g_installer->ShowScreen(IDD_COMPLETION);
                }
                break;

            case MergeDialogAction::Back:
                if (g_installer)
                    g_installer->ShowPreviousScreen();
                break;

            case MergeDialogAction::Cancel:
                // The sheet-modal confirmation already ran inside doCancel:.
                // If we reach here the user confirmed; terminate.
                mac_terminate_app();
                break;
        }
    });
}
