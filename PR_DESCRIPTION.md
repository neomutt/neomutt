# Unify mbox_check API: Merge mbox_check and mbox_check_stats

## Summary

This PR refactors the mailbox checking API by merging the separate `mbox_check()` and `mbox_check_stats()` functions into a single unified `mbox_check()` function with flags. This simplifies the API, improves consistency across mailbox backends, and provides better control over check behavior.

## Motivation

The previous API had two separate functions:
- `mbox_check()` - Check for new mail
- `mbox_check_stats()` - Check mailbox statistics (message counts, etc.)

This design had several issues:
- **Duplication**: Many backends had nearly identical code in both functions
- **Confusion**: Unclear when to call which function
- **Inefficiency**: Some backends (IMAP, POP3) had to perform expensive operations (reconnect, fetch headers) in both
- **Inconsistency**: Different backends handled statistics differently

## Changes

### API Refactoring

**Core API (`core/mxapi.h`):**
- Removed separate `mbox_check()` and `mbox_check_stats()` functions
- Introduced unified `mbox_check(struct Mailbox *m, MboxCheckFlags flags)` function
- Renamed and clarified flags:
  - `MUTT_MAILBOX_CHECK_NO_FLAGS` → `MBOX_CHECK_NO_FLAGS` - No special behavior
  - `MUTT_MAILBOX_CHECK_IMMEDIATE` → `MBOX_CHECK_FORCE` - Force immediate check, ignore caching
  - `MUTT_MAILBOX_CHECK_STATS` → `MBOX_CHECK_FORCE_STATS` - Force statistics update, ignore cache
  - New: `MBOX_CHECK_NO_STATS` - Skip statistics update (quick check only)
  - `MUTT_MAILBOX_CHECK_POSTPONED` → `MBOX_CHECK_POSTPONED` - Update postponed message count

**Mailbox Structure (`core/mailbox.h`):**
- Added statistics caching fields:
  - `stats_last_checked` - Timestamp of last stats update
  - `stats_valid` - Whether cached stats are valid

### Backend Implementations

All mailbox backends updated to implement the unified API:

**IMAP (`imap/imap.c`):**
- Removed separate `imap_mbox_check_stats()`
- Unified into `imap_mbox_check()` with intelligent behavior:
  - `MBOX_CHECK_FORCE` bypasses queue, forces immediate check
  - `MBOX_CHECK_NO_STATS` performs lightweight check only
  - Statistics updated based on cache validity and flags
- For open mailboxes: full check with email updates
- For closed mailboxes: STATUS command for statistics

**POP3 (`pop/pop.c`):**
- Merged check logic into single function
- POP3 is all-or-nothing (requires reconnect and header fetch)
- `MBOX_CHECK_NO_STATS` flag ignored (POP3 always gets all data)
- Respects check interval unless `MBOX_CHECK_FORCE` is set
- Aggressive caching (300s default) to avoid constant reconnects

**NNTP (`nntp/nntp.c`):**
- Unified check implementation
- For open mailboxes: full check with article processing
- For closed mailboxes: GROUP command for statistics
- Supports all flag combinations appropriately

**Maildir (`maildir/mailbox.c`, `maildir/maildir.c`):**
- Merged filesystem scanning logic
- Statistics from directory parsing (new/, cur/, tmp/)
- Efficient caching to avoid repeated filesystem scans
- `MBOX_CHECK_NO_STATS` for quick new mail check only

**Mbox (`mbox/mbox.c`):**
- Unified file parsing approach
- Statistics from file header parsing
- Caching based on file mtime
- Handles concurrent modifications

**MH (`mh/mh.c`):**
- Similar to Maildir (directory-based)
- Sequence file parsing for statistics
- Efficient directory scanning

**Notmuch (`notmuch/notmuch.c`):**
- Database-backed, stats are cheap (queries vs filesystem)
- For open mailboxes: full check with email updates
- For closed mailboxes: database queries only
- Database mtime tracking for change detection

**Compress (`compmbox/compress.c`):**
- Delegates to underlying backend's `mbox_check()`
- Handles compressed file change detection
- Passes flags through to backend

### Call Site Updates

Updated all callers throughout the codebase:

- **Browser** (`browser/dlg_browser.c`): Flag renames
- **GUI** (`gui/global.c`): `<check-stats>` command uses `MBOX_CHECK_FORCE_STATS`
- **Index** (`index/dlg_index.c`): Periodic checks use appropriate flags
- **Pager** (`pager/dlg_pager.c`): New mail checks
- **Postpone** (`postpone/postpone.c`): Postponed message tracking
- **Main** (`main.c`): Startup checks
- **MX layer** (`mx.c`, `mutt_mailbox.c`): High-level mailbox operations

### Documentation

- Updated Doxygen comments for all modified functions
- Clarified flag meanings and behavior
- Added implementation notes for each backend's strategy
- Created helper documentation (`mh_helper.txt`)

## Benefits

1. **Simpler API**: One function instead of two, clearer semantics
2. **Better Performance**: Intelligent caching reduces unnecessary checks
3. **Consistent Behavior**: All backends follow same patterns
4. **Flexibility**: Flags provide fine-grained control over check behavior
5. **Maintainability**: Less code duplication, easier to understand

## Testing

- All three build configurations tested (default, minimal, full)
- Verified against test suite
- No behavioral changes for end users

## Files Changed

- 25 files modified
- ~726 insertions, ~524 deletions
- Net reduction in code complexity

## Backward Compatibility

This is an internal API change only. No user-facing changes to configuration, commands, or behavior.
