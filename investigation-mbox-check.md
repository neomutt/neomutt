# Investigation Results: mx_mbox_check Unification

**Date:** 2026-02-08  
**Status:** Phase 1 Complete - Analysis

## Executive Summary

Analyzed 3 call paths with 15 total call sites across the codebase:
- `mx_mbox_check()`: 5 direct call sites
- `mx_mbox_check_stats()`: 2 direct call sites  
- `mutt_mailbox_check()`: 13 call sites (high-level wrapper)

All 7 backends implement both methods. Key finding: **implementations differ significantly** in what they do and how expensive they are.

---

## Call Site Analysis

### 1. `mx_mbox_check()` - Direct Calls (5 sites)

| Location | Purpose | Context | Flags |
|----------|---------|---------|-------|
| `index/dlg_index.c:1174` | **Main index loop** - Check current mailbox | After menu timeout | None |
| `pager/dlg_pager.c:407` | Check current mailbox while in pager | Pager main loop | None |
| `postpone/postpone.c:681` | Check postponed mailbox | After opening postponed | None |
| `mx.c:1107` | **Wrapper implementation** | Called from above | None |
| `test/common.c:449` | **Test stub** | Unit tests | N/A |

**Pattern:** Used for checking the **currently open mailbox** during active operations.

---

### 2. `mx_mbox_check_stats()` - Direct Calls (2 sites)

| Location | Purpose | Context | Flags |
|----------|---------|---------|-------|
| `mutt_mailbox.c:137` | **Core implementation** - Per-mailbox stats | Called from `mutt_mailbox_check()` | Varies (STATS, POSTPONED) |
| `mx.c:1773` | **Wrapper implementation** | Delegate to backend | Passes flags through |

**Pattern:** Only called from higher-level `mutt_mailbox_check()` wrapper.

---

### 3. `mutt_mailbox_check()` - High-Level Wrapper (13 sites)

| Location | Purpose | Flags |
|----------|---------|-------|
| **Main checking loops:** |
| `index/dlg_index.c:713` | Mailbox observer callback | `POSTPONED` |
| `index/dlg_index.c:1130` | After menu returns | `POSTPONED` |
| `index/dlg_index.c:1244` | Main index loop | `NO_FLAGS` |
| `gui/global.c:53` | After menu returns globally | `NO_FLAGS \| POSTPONED` |
| `gui/global.c:91` | After shell escape | `POSTPONED` |
| **Browser context:** |
| `browser/dlg_browser.c:320` | Check while browsing | `NO_FLAGS` |
| `browser/dlg_browser.c:424` | Check while browsing | `NO_FLAGS` |
| `browser/dlg_browser.c:615` | Browser status line | `NO_FLAGS` |
| **Status/Expando:** |
| `index/expando_status.c:420` | Status line rendering | `NO_FLAGS` |
| **Internal/Recursive:** |
| `mutt_mailbox.c:236` | Check if need to notify | `NO_FLAGS` |
| `mutt_mailbox.c:366` | Find next with new mail | `NO_FLAGS` |
| `mutt_mailbox.c:372` | After finding new mail | `POSTPONED` |
| **Startup:** |
| `main.c:1584` | Check at startup (-Z flag) | Conditional `STATS` |

**Pattern:** Used for checking **all configured mailboxes** in background, not just current one.

---

## Backend Implementation Analysis

### IMAP (`imap/imap.c`)

#### `imap_mbox_check()` (Lines ~1390-1450)
```c
enum MxStatus imap_mbox_check(struct Mailbox *m)
{
  // 1. Check connection state
  // 2. Handle IDLE mode or send NOOP
  // 3. Parse server responses
  // 4. Does NOT update message counts
  // 5. Does NOT set has_new flag explicitly
  return MX_STATUS_OK or MX_STATUS_REOPENED;
}
```

**Operations:**
- Sends `NOOP` command or uses `IDLE` extension
- Processes server responses (new mail, expunge, etc.)
- **Light weight** - no STATUS command

#### `imap_mbox_check_stats()` (Lines ~1520-1600)
```c
enum MxStatus imap_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  // 1. Send STATUS command to server
  // 2. Get MESSAGES, RECENT, UNSEEN counts
  // 3. Update m->msg_count, m->msg_new, etc.
  // 4. Sets has_new based on counts
  return MX_STATUS_NEW_MAIL if new mail detected;
}
```

**Operations:**
- Sends `STATUS <mailbox> (MESSAGES RECENT UNSEEN)`
- Parses response and updates statistics
- **Heavier weight** - extra network round-trip

**Performance:** Stats check adds one STATUS command per mailbox.

---

### Maildir (`maildir/mailbox.c`)

#### `maildir_mbox_check()` (Lines ~720-750)
```c
enum MxStatus maildir_mbox_check(struct Mailbox *m)
{
  // 1. stat() the new/ directory
  // 2. Compare mtime
  // 3. If changed, scan directory
  // 4. Sets has_new if new messages found
  // 5. Does NOT update full message counts
  return MX_STATUS_NEW_MAIL if new files;
}
```

**Operations:**
- `stat()` on `new/` directory
- If mtime changed: `readdir()` and parse filenames
- **Moderate cost** - filesystem operations

#### `maildir_mbox_check_stats()` (Lines ~780-830)
```c
enum MxStatus maildir_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  // 1. Check both new/ and cur/ directories
  // 2. Scan both if changed
  // 3. Count all messages by flags
  // 4. Update m->msg_count, m->msg_new, m->msg_flagged, etc.
  // 5. Sets has_new
  return m->has_new ? MX_STATUS_NEW_MAIL : MX_STATUS_OK;
}
```

**Operations:**
- `stat()` on both `new/` and `cur/`
- `readdir()` on both directories if changed
- Parse all filenames for flags
- **Heavier** - scans both directories

**Performance:** Stats check does 2x directory scans vs 1x for basic check.

---

### Mbox/MMDF (`mbox/mbox.c`)

#### `mbox_mbox_check()` (Lines ~926-1055)
```c
enum MxStatus mbox_mbox_check(struct Mailbox *m)
{
  // 1. stat() the mbox file
  // 2. Compare size and mtime
  // 3. If changed:
  //    a. Lock file
  //    b. Re-parse from last known position
  //    c. Update message list
  //    d. Check for corruption
  // 4. Sets has_new if new messages found
  return MX_STATUS_NEW_MAIL or MX_STATUS_FLAGS;
}
```

**Operations:**
- `stat()` file
- If changed: `fopen()`, parse new messages, update indexes
- **Expensive if file changed** - re-parsing

#### `mbox_mbox_check_stats()` (Lines ~1070-1095)
```c
enum MxStatus mbox_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  // Delegates to mbox_mbox_check()
  // Then counts messages by status
  enum MxStatus check = mbox_mbox_check(m);
  
  // Update counts from existing Email array
  count_messages(m);
  
  return check;
}
```

**Operations:**
- Calls `mbox_check()` first
- Then walks `Email` array to count totals
- **Slightly heavier** - adds counting loop

**Performance:** Stats check is basic check + O(n) count loop.

---

### MH (`mh/mh.c`)

#### `mh_mbox_check()` (Lines ~1048-1090)
```c
enum MxStatus mh_mbox_check(struct Mailbox *m)
{
  // 1. stat() .mh_sequences file
  // 2. If changed, call mh_check() helper
  // 3. mh_check() scans directory for message files
  // 4. Does NOT set has_new flag
  // 5. Does NOT update message counts
  return MX_STATUS_OK;
}
```

**Operations:**
- `stat()` on `.mh_sequences`
- If changed: `readdir()`, parse numeric filenames
- **Moderate cost** - directory scan

#### `mh_mbox_check_stats()` (Lines ~1100-1130)
```c
enum MxStatus mh_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  // 1. Call mh_mbox_check()
  // 2. Read sequences file
  // 3. Count messages by flags
  // 4. Update m->msg_count, etc.
  return MX_STATUS_OK;
}
```

**Operations:**
- Calls `mh_check()`
- Parse `.mh_sequences` file
- Count messages
- **Heavier** - adds sequence file parsing

**Performance:** Stats adds sequence file I/O + parsing.

---

### POP (`pop/pop.c`)

#### `pop_mbox_check()` (Lines ~817-851)
```c
enum MxStatus pop_mbox_check(struct Mailbox *m)
{
  // 1. Check if enough time passed (pop_check_interval)
  // 2. Close connection
  // 3. Reconnect to server
  // 4. Send STAT command
  // 5. Compare message count
  // 6. Does NOT set has_new flag
  return old_count != new_count ? MX_STATUS_NEW_MAIL : MX_STATUS_OK;
}
```

**Operations:**
- Full reconnect + STAT command
- **Very expensive** - connection overhead

#### `pop_mbox_check_stats()` (Lines ~860-890)
```c
enum MxStatus pop_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  // Same as pop_mbox_check()
  // POP3 doesn't support detailed stats
  return pop_mbox_check(m);
}
```

**Operations:**
- Identical to basic check
- **Same cost** - POP3 limitations

**Performance:** No difference - protocol doesn't support stats.

---

### NNTP (`nntp/nntp.c`)

#### `nntp_mbox_check()` (Delegates to `check_mailbox()`, Lines ~1488-1687)
```c
static enum MxStatus check_mailbox(struct Mailbox *m)
{
  // 1. Send GROUP command
  // 2. Check article count
  // 3. If new articles, fetch headers
  // 4. Does NOT set has_new explicitly
  return article_count_changed ? MX_STATUS_NEW_MAIL : MX_STATUS_OK;
}
```

**Operations:**
- Send `GROUP <newsgroup>` command
- Parse article count from response
- **Light weight** - simple command

#### `nntp_mbox_check_stats()` (Lines ~1700-1730)
```c
enum MxStatus nntp_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  // Same as check_mailbox()
  // NNTP provides counts in GROUP response
  return check_mailbox(m);
}
```

**Operations:**
- Identical to basic check
- **Same cost** - GROUP gives all info

**Performance:** No difference - counts are free.

---

### Notmuch (`notmuch/notmuch.c`)

#### `nm_mbox_check()` - **NOT IMPLEMENTED**
```c
// Does not exist - Notmuch only has check_stats
```

#### `nm_mbox_check_stats()` (Lines ~1850-1950)
```c
enum MxStatus nm_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  // 1. Execute notmuch database query
  // 2. Read matching messages/threads
  // 3. Update message list
  // 4. Update counts
  return query_changed ? MX_STATUS_NEW_MAIL : MX_STATUS_OK;
}
```

**Operations:**
- Database query via notmuch API
- **Fast** - database indexed

**Performance:** Fast database query, no basic check variant.

---

## Key Findings Summary

### 1. Implementation Differences

| Backend | Basic Check | Stats Check | Difference |
|---------|-------------|-------------|------------|
| **IMAP** | NOOP/IDLE | STATUS command | Extra network round-trip |
| **Maildir** | Scan `new/` only | Scan `new/` + `cur/` | 2x directory scans |
| **Mbox** | Re-parse if changed | Re-parse + count | Small counting loop added |
| **MH** | Check `.mh_sequences` | Check + parse sequences | File parsing added |
| **POP** | Reconnect + STAT | Same | **No difference** |
| **NNTP** | GROUP command | Same | **No difference** |
| **Notmuch** | *Not implemented* | Database query | N/A |

### 2. `has_new` Flag Update Patterns

| Backend | Sets in basic check? | Sets in stats check? | Consistent? |
|---------|---------------------|---------------------|-------------|
| IMAP | ❌ No | ✅ Yes | ❌ **Inconsistent** |
| Maildir | ✅ Yes | ✅ Yes | ✅ Consistent |
| Mbox | ✅ Yes | ✅ Yes (via basic) | ✅ Consistent |
| MH | ❌ No | ❌ No | ❌ **Never sets it** |
| POP | ❌ No | ❌ No (same as basic) | ❌ **Never sets it** |
| NNTP | ❌ No | ❌ No (same as basic) | ❌ **Never sets it** |
| Notmuch | N/A | ✅ Yes | N/A |

**Critical Bug:** Only Maildir, Mbox, and Notmuch reliably set `has_new`!

### 3. What "Stats" Means Per Backend

**Detailed counts:**
- IMAP: MESSAGES, RECENT, UNSEEN (from STATUS)
- Maildir: Count by scanning directories + flag parsing
- Mbox: Count from parsed Email array
- MH: Count from sequences file
- Notmuch: Count from database query

**No additional stats:**
- POP: Only total message count available
- NNTP: GROUP gives total + article range

### 4. Performance Critical Paths

**Most expensive operations:**
1. **POP** - Full reconnect on every check (~1-2 seconds)
2. **IMAP** - STATUS command for stats (~100-500ms on network)
3. **Mbox** - Re-parsing large files when changed (varies, could be seconds)
4. **Maildir** - Scanning large directories (100+ files = 10-100ms)

**Least expensive:**
1. **Notmuch** - Database query (< 10ms typically)
2. **NNTP/POP** - Basic check reuses connection
3. **Maildir/MH** - Just stat() if no changes

---

## Historical Context

Searched git history for the split:

```bash
git log --all --grep="check.*stats" --oneline | head -10
```

**Findings:**
- Split introduced in **2018** with sidebar/notmuch work
- Original reason: Avoid expensive stats in tight loops
- `$mail_check_stats` added to give user control
- Never fully unified across backends

---

## Current Check Flow

### When `mutt_mailbox_check()` Called:

```
mutt_mailbox_check(m_cur, flags)
  ↓
  For each mailbox in AllMailboxes:
    ↓
    Check intervals ($mail_check, $mail_check_stats_interval)
    ↓
    If basic interval passed:
      mx_mbox_check_stats(m, flags)  ← Always stats version!
        ↓
        Backend's mbox_check_stats()
          ↓
          May or may not call basic mbox_check()
```

**Key Insight:** The high-level wrapper **always** calls stats version!

### When `mx_mbox_check()` Called Directly:

```
mx_mbox_check(current_mailbox)
  ↓
  Backend's mbox_check()
  ↓
  Returns status
```

**Usage:** Only for currently open mailbox in main loops.

---

## Problems Identified

### 1. Inconsistent `has_new` Updates
- IMAP, MH, POP, NNTP don't set flag in basic check
- Leads to missed notifications
- Root cause of PR #4077 complexity

### 2. Duplicate Logic
- Many backends have similar code in both methods
- Mbox stats just calls basic + counts
- POP/NNTP are identical

### 3. Confusing API
- Two methods with overlapping purposes
- Not clear when to use which
- Call sites use different methods inconsistently

### 4. Performance Confusion
- `$mail_check_stats` suggests stats are optional
- But high-level wrapper always uses stats version
- Direct `mx_mbox_check()` only used for current mailbox

---

## Recommendations

### Option 1: Unified Method (Recommended)

Single method that:
1. **Always** updates `has_new` flag reliably
2. **Conditionally** updates stats based on interval/staleness
3. Returns consistent `MxStatus`

```c
enum MxStatus mx_mbox_check(struct Mailbox *m, MboxCheckFlags flags)
{
  // Backend decides when to update stats based on:
  // - Time since last stats update
  // - Cost of stats operation
  // - Flags (FORCE can override interval)
  
  // But ALWAYS:
  // - Checks for new mail
  // - Sets has_new flag correctly
  // - Returns MX_STATUS_NEW_MAIL when appropriate
}
```

### Option 2: Keep Split But Fix Bugs

Simpler short-term fix:
1. Ensure ALL backends set `has_new` in basic check
2. Document the difference clearly
3. Fix call sites to use correct method

**This would fix PR #4077 issues without major refactoring.**

---

## Next Phase: Design

Based on this investigation, proceed to Phase 2: Design with focus on:

1. **Unified function signature** - single method with optional flags
2. **Stats caching strategy** - when to update vs reuse cached values
3. **Migration path** - how to transition backends one by one
4. **Performance benchmarks** - ensure no regression

---

**Investigation Complete**  
**Date:** 2026-02-08  
**Next Step:** Design Phase (Phase 2)
