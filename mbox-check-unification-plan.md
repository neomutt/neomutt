# Plan: Unify mx_mbox_check() and mx_mbox_check_stats()

## Problem Statement

NeoMutt currently has two separate methods for checking mailboxes:

1. **`mx_mbox_check()`** - Basic check for new mail
   - Lighter weight, faster
   - Updates mailbox state but not always statistics
   - Returns `MX_STATUS_NEW_MAIL`, `MX_STATUS_REOPENED`, etc.
   - **Problem:** Doesn't consistently set `Mailbox->has_new` flag across backends

2. **`mx_mbox_check_stats()`** - Full statistics check
   - More expensive operation
   - Updates total/new/flagged/unread counts
   - Controlled by `$mail_check_stats` and `$mail_check_stats_interval`
   - Returns same `MxStatus` enum

**Issues with Current Split:**
- Backends implement logic inconsistently
- `has_new` flag not reliably updated by basic check
- Duplicate code in many backends
- Configuration complexity (`$mail_check` vs `$mail_check_stats_interval`)
- Source of bugs (as seen in PR #4077)

## Goals

1. **Single unified method** that always returns complete mailbox state
2. **Simplify backend implementations** - one check function instead of two
3. **Maintain performance** - don't make every check expensive
4. **Preserve user-facing behavior** - backward compatible
5. **Fix `has_new` inconsistency** - always reliable across all backends

## Investigation Tasks

### Phase 1: Understand Current State

- [ ] Map all call sites of `mx_mbox_check()` and `mx_mbox_check_stats()`
- [ ] Analyze each backend's implementation differences
- [ ] Measure performance difference between basic check vs stats check per backend
- [ ] Identify what "stats" actually means for each backend
- [ ] Document current `has_new` flag update patterns
- [ ] Review historical reason for the split (git blame, old commits)

### Phase 2: Design Unified API

- [ ] Design new unified function signature
- [ ] Determine how to handle performance (flags? caching? lazy evaluation?)
- [ ] Plan configuration variable migration strategy
- [ ] Design notification event strategy
- [ ] Create backward compatibility shim if needed

### Phase 3: Backend Migration

- [ ] Refactor IMAP backend (most complex)
- [ ] Refactor Maildir backend
- [ ] Refactor Mbox/MMDF backend
- [ ] Refactor MH backend
- [ ] Refactor POP backend
- [ ] Refactor NNTP backend
- [ ] Refactor Notmuch backend (currently only has check_stats)

### Phase 4: Integration & Testing

- [ ] Update all call sites to use new unified method
- [ ] Remove old wrapper functions
- [ ] Update configuration documentation
- [ ] Add/update unit tests
- [ ] Performance regression testing
- [ ] Memory leak testing (Valgrind)
- [ ] Test with real-world mailboxes

## Design Options

### Option A: Single Method with Flags (Recommended)

Keep flags but simplify to just one meaningful distinction:

```c
typedef uint8_t MboxCheckFlags;
#define MBOX_CHECK_NO_FLAGS     0
#define MBOX_CHECK_FORCE        (1 << 0)  // Bypass interval checks
#define MBOX_CHECK_STATS_ONLY   (1 << 1)  // Only update counts, don't fetch new mail

/**
 * mx_mbox_check - Check mailbox for changes and update statistics
 * @param m     Mailbox to check
 * @param flags Check behavior flags
 * @retval enum MxStatus
 */
enum MxStatus mx_mbox_check(struct Mailbox *m, MboxCheckFlags flags);
```

**Backend Implementation:**
```c
enum MxStatus maildir_mbox_check(struct Mailbox *m, MboxCheckFlags flags)
{
  // Always update counts (cheap for maildir - just count files)
  update_message_counts(m);
  
  if (!(flags & MBOX_CHECK_STATS_ONLY))
  {
    // Check for new mail and parse new messages
    check_for_new_mail(m);
  }
  
  return m->has_new ? MX_STATUS_NEW_MAIL : MX_STATUS_OK;
}
```

**Pros:**
- Clean API
- Backends can optimize based on flags
- Easy migration path

**Cons:**
- Still has flags (but simpler than current)

### Option B: Always Do Full Check

Simplest approach - every check updates everything:

```c
enum MxStatus mx_mbox_check(struct Mailbox *m);
```

**Pros:**
- Simplest API
- No confusion about what gets updated
- Always consistent state

**Cons:**
- Performance concerns for large IMAP mailboxes
- May require caching/throttling at higher level

### Option C: Lazy Stats with Caching

Check always looks for new mail, but stats are cached:

```c
enum MxStatus mx_mbox_check(struct Mailbox *m);

// Stats cached in Mailbox struct, updated as needed
// Call sites can access m->msg_count, m->msg_unread, etc.
```

**Pros:**
- Fast checks
- Stats available when needed
- Natural caching

**Cons:**
- More complex state management
- Cache invalidation issues

## Recommended Approach

**Hybrid of Options A and C:**

1. **Unified function with minimal flags**
   ```c
   enum MxStatus mx_mbox_check(struct Mailbox *m, MboxCheckFlags flags);
   ```

2. **Smart caching at mailbox level**
   - Track last stats update time in `Mailbox` struct
   - Backends update stats when "stale" or when new mail detected
   - Configuration: `$mail_check_stats_interval` still controls staleness

3. **Consistent behavior**
   - ALWAYS set `has_new` flag when new mail detected
   - ALWAYS return `MX_STATUS_NEW_MAIL` when appropriate
   - Stats updated based on interval/staleness, not caller

## Migration Strategy

### Step 1: Add Unified Implementation (Parallel)

Add new unified check function without removing old ones:

```c
// New unified function
enum MxStatus mx_mbox_check_unified(struct Mailbox *m, MboxCheckFlags flags);

// Old functions become wrappers
enum MxStatus mx_mbox_check(struct Mailbox *m)
{
  return mx_mbox_check_unified(m, MBOX_CHECK_NO_FLAGS);
}

enum MxStatus mx_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  MboxCheckFlags new_flags = MBOX_CHECK_NO_FLAGS;
  if (flags & MUTT_MAILBOX_CHECK_FORCE)
    new_flags |= MBOX_CHECK_FORCE;
  return mx_mbox_check_unified(m, new_flags);
}
```

### Step 2: Migrate Backends One-by-One

For each backend:
1. Implement new unified `mbox_check_unified()` function
2. Update `MxOps` to point to new function
3. Test thoroughly
4. Keep old functions as deprecated stubs initially

### Step 3: Update Call Sites

Gradually migrate call sites to use new unified function:
```c
// Old:
mutt_mailbox_check(m_cur, MUTT_MAILBOX_CHECK_STATS);

// New:
mx_mbox_check(m, MBOX_CHECK_FORCE);
```

### Step 4: Remove Old API

Once all backends and call sites migrated:
1. Remove old wrapper functions
2. Remove old `MxOps` function pointers
3. Update documentation
4. Update configuration variable docs

## Performance Considerations

### Concerns:

1. **IMAP**: Sending STATUS command on every check might be expensive
   - **Solution**: Cache results, respect `$imap_poll_timeout`
   
2. **Large mbox files**: Re-parsing entire file to get counts
   - **Solution**: Maintain running counts, only re-parse on size/mtime change
   
3. **Network protocols**: Extra round-trips
   - **Solution**: Backend-specific optimizations (pipelining, caching)

### Benchmarking Plan:

- [ ] Measure current performance (basic check vs stats check)
- [ ] Implement unified version
- [ ] Compare performance in common scenarios
- [ ] Optimize backends if regression detected

## Testing Strategy

### Unit Tests:

- [ ] Test each backend's unified check function
- [ ] Test flag combinations
- [ ] Test `has_new` flag consistency
- [ ] Test stats caching logic

### Integration Tests:

- [ ] Multiple mailbox types in same session
- [ ] Rapid repeated checks (performance)
- [ ] Long-running sessions (memory leaks)
- [ ] Concurrent checks (thread safety if applicable)

### Real-World Testing:

- [ ] Large IMAP mailbox (10k+ messages)
- [ ] Multiple Maildir folders (100+ folders)
- [ ] Slow network connection (IMAP/POP)
- [ ] Mixed mailbox types

## Backward Compatibility

### Configuration Variables:

**Keep:**
- `$mail_check` - still controls basic check interval
- `$mail_check_stats_interval` - controls stats update interval (interpreted internally)
- `$mail_check_recent` - still relevant

**Potentially Deprecate:**
- `$mail_check_stats` - stats always available, but interval still respected

### User-Visible Changes:

**Minimal impact expected:**
- Notifications should work the same or better
- Status line formatting unchanged
- Sidebar display unchanged

**Possible improvements:**
- More consistent new mail detection
- Stats always available when needed

## Documentation Updates

- [ ] Update `docs/manual.xml.head` - mail checking section
- [ ] Update `mail-checking-summary.md` with new architecture
- [ ] Update Doxygen comments in `core/mxapi.h`
- [ ] Update ChangeLog entry
- [ ] Update NEWS file for release notes

## Timeline Estimate

- **Investigation**: 1-2 weeks
- **Design & Prototyping**: 1 week  
- **Backend Migration**: 2-3 weeks (7 backends)
- **Call Site Updates**: 1 week
- **Testing & Bug Fixes**: 2 weeks
- **Documentation**: 1 week

**Total: ~8-10 weeks** for complete implementation and testing

## Success Criteria

- [ ] Single `mx_mbox_check()` function used everywhere
- [ ] All backends implement unified check
- [ ] `has_new` flag reliably set across all backends
- [ ] No performance regression in common use cases
- [ ] All existing tests pass
- [ ] Memory leak free (Valgrind clean)
- [ ] Documentation updated

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Performance regression | High | Benchmark early, optimize per-backend, add caching |
| Breaking existing workflows | Medium | Extensive testing, beta release, maintain old behavior |
| Backend-specific bugs | High | Migrate one backend at a time, thorough testing |
| User confusion | Low | Keep config vars, update docs, clear ChangeLog |
| Merge conflicts | Medium | Work in feature branch, communicate with team |

## Open Questions

1. Should we completely remove `$mail_check_stats` or keep for backward compat?
2. How aggressive should stats caching be?
3. Should we add telemetry to measure performance impact?
4. What's the migration path for third-party patches?

## Next Steps

1. **Gather feedback** on this plan from @flatcap and NeoMutt team
2. **Start investigation** phase - map call sites and backends
3. **Create feature branch** for development
4. **Prototype** unified implementation for one simple backend (Maildir)
5. **Benchmark** prototype vs current implementation
6. **Iterate** on design based on prototype learnings

---

**Plan created:** 2026-02-08  
**Status:** Proposal / RFC  
**Requires:** Team review and approval before implementation
