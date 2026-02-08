# Design Document: Unified mx_mbox_check() API

**Date:** 2026-02-08  
**Status:** Design Phase  
**Decision:** Proceed with full unification (Option A)

## Design Goals

Based on investigation findings, the unified API must:

1. ✅ **Reliability:** Always update `has_new` flag correctly across all backends
2. ✅ **Performance:** No regression in common use cases
3. ✅ **Clarity:** Single, obvious method to check mailboxes
4. ✅ **Flexibility:** Backends can optimize based on mailbox type
5. ✅ **Compatibility:** Smooth migration path from current API

## Proposed Unified API

### Core Function Signature

```c
/**
 * mx_mbox_check - Check mailbox for new mail and update statistics
 * @param m     Mailbox to check
 * @param flags Check behavior flags
 * @retval enum MxStatus indicating mailbox state
 *
 * This function checks the mailbox for new mail and optionally updates
 * message statistics. The behavior is controlled by flags and internal
 * caching based on check intervals.
 *
 * Guarantees:
 * - Always checks for new mail
 * - Always sets m->has_new correctly when new mail detected
 * - Always returns MX_STATUS_NEW_MAIL when appropriate
 * - Updates statistics based on staleness and flags
 * 
 * Statistics are cached and only updated when:
 * - $mail_check_stats_interval has elapsed since last update
 * - MBOX_CHECK_FORCE_STATS flag is set
 * - New mail is detected (always refresh stats on new mail)
 */
enum MxStatus mx_mbox_check(struct Mailbox *m, MboxCheckFlags flags);
```

### Flag System

```c
/**
 * MboxCheckFlags - Flags controlling mailbox check behavior
 */
typedef uint8_t MboxCheckFlags;

#define MBOX_CHECK_NO_FLAGS      0           ///< Standard check operation
#define MBOX_CHECK_FORCE         (1 << 0)    ///< Bypass $mail_check interval
#define MBOX_CHECK_FORCE_STATS   (1 << 1)    ///< Force stats update regardless of interval
#define MBOX_CHECK_NO_STATS      (1 << 2)    ///< Skip stats update (for performance)
#define MBOX_CHECK_POSTPONED     (1 << 3)    ///< Also update postponed count

// Convenience combinations
#define MBOX_CHECK_FULL          (MBOX_CHECK_FORCE | MBOX_CHECK_FORCE_STATS)
```

**Flag Semantics:**

- **NO_FLAGS**: Normal operation - check for new mail, update stats if interval elapsed
- **FORCE**: Bypass `$mail_check` interval, always check (for user-initiated actions)
- **FORCE_STATS**: Override `$mail_check_stats_interval`, always update counts
- **NO_STATS**: Skip stats completely for performance (rare, specific use cases)
- **POSTPONED**: Also update postponed message count (existing behavior)

### Return Values

```c
enum MxStatus
{
  MX_STATUS_ERROR = -1,     ///< An error occurred
  MX_STATUS_OK,             ///< No changes detected
  MX_STATUS_NEW_MAIL,       ///< New mail received (m->has_new will be true)
  MX_STATUS_LOCKED,         ///< Couldn't lock the Mailbox
  MX_STATUS_REOPENED,       ///< Mailbox was reopened (e.g., IMAP)
  MX_STATUS_FLAGS,          ///< Nondestructive flags change (IMAP)
};
```

**Guaranteed Invariants:**
- If return is `MX_STATUS_NEW_MAIL`, then `m->has_new == true`
- If `m->has_new == true` after call, return must be `MX_STATUS_NEW_MAIL` or `MX_STATUS_REOPENED`
- Stats fields valid if `m->stats_last_checked` is recent

---

## Mailbox Statistics Caching

### New Fields in `struct Mailbox`

```c
struct Mailbox {
  // ... existing fields ...
  
  time_t stats_last_checked;   ///< Timestamp of last statistics update
  bool stats_valid;             ///< Are cached statistics current?
  
  // Existing stat fields (now explicitly cached):
  int msg_count;                ///< Total messages
  int msg_new;                  ///< New messages  
  int msg_unread;               ///< Unread messages
  int msg_flagged;              ///< Flagged messages
  // ... etc
};
```

### Stats Update Policy

```c
/**
 * should_update_stats - Determine if statistics should be updated
 * @param m     Mailbox
 * @param flags Check flags
 * @retval true  Update stats now
 * @retval false Use cached stats
 */
static bool should_update_stats(struct Mailbox *m, MboxCheckFlags flags)
{
  // Always skip if explicitly requested
  if (flags & MBOX_CHECK_NO_STATS)
    return false;
  
  // Always update if forced
  if (flags & MBOX_CHECK_FORCE_STATS)
    return true;
  
  // Update if cache is invalid
  if (!m->stats_valid)
    return true;
  
  // Update if interval has elapsed
  const short c_interval = cs_subset_number(NeoMutt->sub, "mail_check_stats_interval");
  time_t now = mutt_date_now();
  if ((now - m->stats_last_checked) >= c_interval)
    return true;
  
  // Update if we just found new mail (keep counts fresh)
  if (m->has_new)
    return true;
  
  return false;  // Use cached stats
}
```

---

## Backend Implementation Pattern

### Standard Template

All backends should follow this pattern:

```c
/**
 * <backend>_mbox_check - Check mailbox for changes
 * @param m     Mailbox to check
 * @param flags Check flags
 * @retval enum MxStatus
 */
enum MxStatus <backend>_mbox_check(struct Mailbox *m, MboxCheckFlags flags)
{
  enum MxStatus rc = MX_STATUS_OK;
  bool new_mail_detected = false;
  
  // STEP 1: Check for new mail (ALWAYS)
  // This is the lightweight operation - check if anything changed
  new_mail_detected = <backend>_check_for_new(m);
  
  if (new_mail_detected)
  {
    rc = MX_STATUS_NEW_MAIL;
    m->has_new = true;  // CRITICAL: Always set this flag
  }
  
  // STEP 2: Update statistics (CONDITIONAL)
  // Only update if interval elapsed or forced
  if (should_update_stats(m, flags))
  {
    <backend>_update_stats(m);
    m->stats_last_checked = mutt_date_now();
    m->stats_valid = true;
  }
  
  return rc;
}
```

### Backend-Specific Optimizations

Each backend can optimize based on its characteristics:

#### IMAP
```c
enum MxStatus imap_mbox_check(struct Mailbox *m, MboxCheckFlags flags)
{
  // Lightweight: NOOP or IDLE
  bool new_mail = imap_check_idle_or_noop(m);
  
  if (new_mail)
  {
    m->has_new = true;
    rc = MX_STATUS_NEW_MAIL;
  }
  
  // Heavier: STATUS command (network round-trip)
  if (should_update_stats(m, flags))
  {
    imap_send_status_command(m);  // Get MESSAGES, UNSEEN, RECENT
    m->stats_last_checked = mutt_date_now();
    m->stats_valid = true;
  }
  
  return rc;
}
```

#### Maildir
```c
enum MxStatus maildir_mbox_check(struct Mailbox *m, MboxCheckFlags flags)
{
  // Check new/ directory only
  bool new_mail = maildir_check_new_dir(m);
  
  if (new_mail)
  {
    m->has_new = true;
    rc = MX_STATUS_NEW_MAIL;
  }
  
  // Full scan of both new/ and cur/ for stats
  if (should_update_stats(m, flags))
  {
    maildir_scan_both_dirs(m);  // More expensive
    m->stats_last_checked = mutt_date_now();
    m->stats_valid = true;
  }
  
  return rc;
}
```

#### POP (no difference between basic and stats)
```c
enum MxStatus pop_mbox_check(struct Mailbox *m, MboxCheckFlags flags)
{
  // POP3 doesn't have separate "stats" - STAT gives count
  // So we always do full check
  
  if (!pop_check_interval_elapsed(m))
    return MX_STATUS_OK;
  
  pop_reconnect_and_stat(m);
  
  if (message_count_changed(m))
  {
    m->has_new = true;
    m->stats_last_checked = mutt_date_now();
    m->stats_valid = true;
    return MX_STATUS_NEW_MAIL;
  }
  
  return MX_STATUS_OK;
}
```

---

## Migration Strategy

### Phase 1: Add Parallel Implementation (Weeks 1-2)

Create new unified function **without removing old ones:**

```c
// New unified implementation
enum MxStatus mx_mbox_check_unified(struct Mailbox *m, MboxCheckFlags flags);

// Backend ops structure gets new field
struct MxOps {
  // ... existing fields ...
  
  // OLD (deprecated but still present)
  enum MxStatus (*mbox_check)(struct Mailbox *m);
  enum MxStatus (*mbox_check_stats)(struct Mailbox *m, uint8_t flags);
  
  // NEW (unified)
  enum MxStatus (*mbox_check_unified)(struct Mailbox *m, MboxCheckFlags flags);
};
```

Wrapper maintains compatibility:
```c
enum MxStatus mx_mbox_check(struct Mailbox *m)
{
  // Prefer new unified version
  if (m->mx_ops->mbox_check_unified)
    return m->mx_ops->mbox_check_unified(m, MBOX_CHECK_NO_FLAGS);
  
  // Fall back to old version
  return m->mx_ops->mbox_check(m);
}

enum MxStatus mx_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  // Prefer new unified version
  if (m->mx_ops->mbox_check_unified)
  {
    MboxCheckFlags new_flags = MBOX_CHECK_NO_FLAGS;
    if (flags & MUTT_MAILBOX_CHECK_STATS)
      new_flags |= MBOX_CHECK_FORCE_STATS;
    if (flags & MUTT_MAILBOX_CHECK_POSTPONED)
      new_flags |= MBOX_CHECK_POSTPONED;
    
    return m->mx_ops->mbox_check_unified(m, new_flags);
  }
  
  // Fall back to old version
  return m->mx_ops->mbox_check_stats(m, flags);
}
```

### Phase 2: Migrate Backends One-by-One (Weeks 3-6)

**Priority Order:**
1. **Maildir** (simplest, good testing ground)
2. **Mbox** (similar to Maildir)
3. **Notmuch** (only has stats version currently)
4. **IMAP** (most complex, highest impact)
5. **MH** (similar to Maildir)
6. **POP** (simple, no difference in operations)
7. **NNTP** (similar to POP)

**Per-Backend Process:**
1. Implement `<backend>_mbox_check_unified()`
2. Update `MxOps` structure to point to new function
3. Add unit tests
4. Test with real mailbox
5. Commit with clear message
6. Mark old functions as deprecated (comments)

### Phase 3: Update Call Sites (Weeks 7-8)

Gradually update call sites to use new API:

```c
// OLD:
mutt_mailbox_check(m, MUTT_MAILBOX_CHECK_STATS);

// NEW:
mx_mbox_check(m, MBOX_CHECK_FORCE_STATS);
```

```c
// OLD:
enum MxStatus check = mx_mbox_check(shared->mailbox);

// NEW:  (no change needed if wrapper maintained)
enum MxStatus check = mx_mbox_check(shared->mailbox);
```

### Phase 4: Cleanup (Weeks 9-10)

Once all backends and call sites migrated:

1. Remove old `mbox_check` and `mbox_check_stats` from `MxOps`
2. Remove wrapper compatibility layer
3. Rename `mx_mbox_check_unified()` to `mx_mbox_check()`
4. Update documentation
5. Update ChangeLog
6. Final testing round

---

## Performance Optimization Strategy

### Caching Rules

**Per-Backend Cache Behavior:**

| Backend | New Mail Check Cost | Stats Update Cost | Cache Strategy |
|---------|---------------------|-------------------|----------------|
| IMAP | Low (NOOP/IDLE) | High (STATUS) | **Aggressive caching** - 60s default |
| Maildir | Low (stat new/) | Medium (scan both) | **Moderate caching** - 30s default |
| Mbox | Medium (stat+parse if changed) | Low (just count) | **Light caching** - 15s default |
| MH | Low (stat .mh_sequences) | Medium (parse sequences) | **Moderate caching** - 30s default |
| POP | High (reconnect) | Same | **Very aggressive** - 300s default |
| NNTP | Low (GROUP) | Same | **No caching needed** - free |
| Notmuch | Low (db query) | Same | **No caching needed** - fast |

### Adaptive Intervals

Consider per-backend default intervals:

```c
/**
 * get_stats_interval - Get appropriate stats interval for mailbox type
 * @param m Mailbox
 * @retval num Seconds to cache stats
 */
static int get_stats_interval(struct Mailbox *m)
{
  // User override always wins
  const short c_interval = cs_subset_number(NeoMutt->sub, "mail_check_stats_interval");
  if (c_interval > 0)
    return c_interval;
  
  // Otherwise use backend-appropriate default
  switch (m->type)
  {
    case MUTT_IMAP:
      return 60;   // Avoid excessive STATUS commands
    case MUTT_POP:
      return 300;  // Avoid reconnects
    case MUTT_MAILDIR:
    case MUTT_MH:
      return 30;   // Moderate caching
    case MUTT_MBOX:
    case MUTT_MMDF:
      return 15;   // Quick updates
    case MUTT_NNTP:
    case MUTT_NOTMUCH:
      return 0;    // No caching needed (stats are free)
    default:
      return 60;   // Conservative default
  }
}
```

### Benchmarking Plan

**Metrics to Track:**

1. **Average check time** per backend (microseconds)
2. **Network round-trips** (IMAP/POP/NNTP)
3. **Filesystem operations** (stat, readdir, fopen)
4. **Memory allocation** (any leaks?)

**Test Scenarios:**

1. **Rapid checks** - 100 checks in 10 seconds (test caching)
2. **Cold start** - First check after launch
3. **Large mailbox** - 10k+ messages
4. **Slow network** - Simulate 200ms latency for IMAP
5. **Mixed types** - 10 mailboxes of different types

**Acceptance Criteria:**
- No regression > 10% in any scenario
- IMAP/POP show improvement (fewer redundant ops)
- Memory usage flat (no leaks)

---

## Configuration Changes

### Keep Existing Variables

```
$mail_check = 5              # Unchanged - basic check interval
$mail_check_stats_interval = 60  # Unchanged - stats cache interval
$mail_check_recent = yes     # Unchanged - what counts as "new"
```

### Add New Variable (Optional)

```
$mail_check_adaptive = no    # Use backend-specific intervals?
```

### Deprecate Variable

```
$mail_check_stats = yes      # Deprecated - stats always available now
                             # But keep for compatibility (just ignored)
```

**Migration Message:**
```
Warning: $mail_check_stats is deprecated and ignored.
Statistics are now always available and cached intelligently.
Use $mail_check_stats_interval to control update frequency.
```

---

## Backward Compatibility

### Old API Compatibility

Maintain wrappers during transition:

```c
// DEPRECATED: Use mx_mbox_check() with MBOX_CHECK_FORCE_STATS instead
enum MxStatus mx_mbox_check_stats(struct Mailbox *m, uint8_t flags)
  __attribute__((deprecated("Use mx_mbox_check() with appropriate flags")));
```

### Old Call Sites

All existing call sites continue to work:
- Wrappers translate old flags to new flags
- Semantics preserved
- Gradual migration possible

### User-Visible Changes

**What stays the same:**
- Notification behavior
- Status line display
- Sidebar behavior
- Keyboard shortcuts

**What improves:**
- More reliable new mail detection
- Consistent behavior across backends
- Better performance (smarter caching)

---

## Testing Strategy

### Unit Tests

```c
// test/mx/test_mbox_check_unified.c

void test_maildir_check_basic(void)
{
  struct Mailbox *m = mailbox_new();
  // ... setup maildir ...
  
  enum MxStatus rc = mx_mbox_check(m, MBOX_CHECK_NO_FLAGS);
  TEST_CHECK(rc == MX_STATUS_OK || rc == MX_STATUS_NEW_MAIL);
  
  if (rc == MX_STATUS_NEW_MAIL)
    TEST_CHECK(m->has_new == true);  // Critical invariant
  
  mailbox_free(&m);
}

void test_imap_stats_caching(void)
{
  struct Mailbox *m = mailbox_new();
  // ... setup IMAP ...
  
  // First check updates stats
  mx_mbox_check(m, MBOX_CHECK_FORCE_STATS);
  time_t first_check = m->stats_last_checked;
  
  // Second check within interval - should use cache
  mx_mbox_check(m, MBOX_CHECK_NO_FLAGS);
  TEST_CHECK(m->stats_last_checked == first_check);  // No update
  
  // Force stats - should update
  mx_mbox_check(m, MBOX_CHECK_FORCE_STATS);
  TEST_CHECK(m->stats_last_checked > first_check);  // Updated
  
  mailbox_free(&m);
}
```

### Integration Tests

1. **Multi-backend test** - Check 10 mailboxes of different types
2. **Long-running test** - 1000 checks over 10 minutes
3. **Concurrent test** - Multiple mailboxes checked simultaneously
4. **Network failure test** - IMAP with simulated disconnects

### Performance Tests

```bash
# Benchmark script
./test/benchmark_mbox_check.sh

# Outputs:
# Backend | Avg Check (μs) | Avg Stats (μs) | Cached (μs) | Network Ops
# --------|----------------|----------------|-------------|-------------
# IMAP    | 1234          | 5678           | 123         | 1 STATUS
# Maildir | 567           | 891            | 567         | 0
# ...
```

---

## Documentation Updates

### Code Documentation

1. Update `core/mxapi.h` - New function signatures and flags
2. Update `mx.h` - Wrapper deprecation notices
3. Add migration guide in `docs/dev/`
4. Update Doxygen comments

### User Documentation

1. Update `docs/manual.xml.head`:
   - Mail checking section
   - Configuration variables
   - Performance tuning

2. Update `docs/neomuttrc.man.head`:
   - Variable descriptions
   - Deprecated variables

3. Update `mail-checking-summary.md`:
   - New unified architecture
   - Backend implementation details

### ChangeLog Entry

```
* Unified mailbox checking API
  - Merged mx_mbox_check() and mx_mbox_check_stats() into single function
  - Fixed has_new flag inconsistency across all backends
  - Improved performance through intelligent statistics caching
  - Deprecated $mail_check_stats (now ignored, always enabled)
  - Added MBOX_CHECK_* flags for fine-grained control
  - See docs/dev/mbox-check-migration.md for details
```

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Performance regression | Medium | High | Extensive benchmarking, per-backend optimization |
| Breaking user workflows | Low | Medium | Maintain backward compatibility, gradual rollout |
| Backend-specific bugs | High | Medium | One backend at a time, thorough testing |
| IMAP reliability issues | Medium | High | Extra testing, maintain old behavior initially |
| Memory leaks | Low | High | Valgrind on all backends, long-running tests |
| Config confusion | Medium | Low | Clear deprecation warnings, documentation |

---

## Success Metrics

### Technical Goals

- [ ] All 7 backends implement unified check function
- [ ] `has_new` flag set correctly 100% of the time
- [ ] No performance regression in benchmarks
- [ ] Zero memory leaks (Valgrind clean)
- [ ] All unit tests pass
- [ ] All integration tests pass

### User Experience Goals

- [ ] More reliable new mail notifications
- [ ] No user-visible behavior changes (except improvements)
- [ ] Clear migration documentation
- [ ] Positive feedback from beta testers

### Code Quality Goals

- [ ] Remove ~200 lines of duplicate code
- [ ] Simpler backend implementations
- [ ] Single clear API for mailbox checking
- [ ] Comprehensive test coverage (>80%)

---

## Timeline

### Detailed Schedule

**Weeks 1-2: Foundation**
- Implement helper functions (should_update_stats, etc.)
- Add stats caching fields to Mailbox struct
- Create MboxCheckFlags enum and constants
- Write initial unit tests

**Weeks 3-4: First Backends (Maildir, Mbox)**
- Implement Maildir unified check
- Implement Mbox unified check  
- Test and benchmark
- Document pattern for other backends

**Weeks 5-6: Complex Backends (IMAP, Notmuch)**
- Implement IMAP unified check (most critical)
- Implement Notmuch unified check
- Extra testing for IMAP (network issues)
- Performance optimization

**Weeks 7-8: Remaining Backends (MH, POP, NNTP)**
- Implement MH unified check
- Implement POP unified check
- Implement NNTP unified check
- Full backend test suite

**Weeks 9-10: Integration & Cleanup**
- Update all call sites
- Remove old API
- Final performance benchmarks
- Documentation updates
- Beta testing

**Total: 10 weeks**

---

## Next Steps

1. ✅ **Get design approval** - Review with NeoMutt maintainers
2. **Create feature branch** - `feature/unify-mbox-check`
3. **Start Week 1 implementation** - Foundation work
4. **Set up benchmarking framework** - Before any changes
5. **Implement first backend** - Maildir as proof of concept

---

**Design Complete**  
**Status:** Ready for Review  
**Next:** Awaiting approval to begin implementation
