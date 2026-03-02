# Custom CodeQL Queries for NeoMutt

## The problem

CodeQL's `cpp/path-injection` query ("Uncontrolled data used in path
expression") produces false positives because of NeoMutt's Buffer Pool.

The pattern:

1. **Code1** gets a Buffer from the pool — `buf_pool_get()`
2. **Code1** writes untrusted data into the Buffer
3. **Code1** returns the Buffer — `buf_pool_release()`, which calls
   `buf_reset()` → `memset()` to zero-fill the storage
4. **Code2** calls `buf_pool_get()` and receives the *same* Buffer
5. CodeQL considers the Buffer still tainted (it isn't — step 3 cleared it)

## How the fix works

### Step 1 — Exclude the default query

`.github/codeql.yml` filters out the stock `cpp/path-injection` query so it
never runs:

```yaml
query-filters:
  - exclude:
      id: cpp/path-injection
```

### Step 2 — Run a replacement query

`TaintedPath.ql` is a copy of the upstream query (from `github/codeql`,
`cpp/ql/src/Security/CWE/CWE-022/TaintedPath.ql`) with one extra barrier
clause added to `isBarrier()`:

```ql
exists(Call call |
  call.getTarget().hasName("buf_pool_get") and
  (
    node.asExpr() = call
    or
    node.asIndirectExpr() = call
  )
)
```

This tells the taint-tracking engine: the value returned by `buf_pool_get()`
is clean — do not propagate taint through it.

The replacement query is registered as `neomutt/path-injection` and included
via the `packs` list in `.github/codeql.yml`.

## Maintenance

When the upstream `cpp/path-injection` query changes (new barriers, sources,
or sinks), `TaintedPath.ql` should be updated to match.  The upstream source
lives at:

https://github.com/github/codeql/blob/main/cpp/ql/src/Security/CWE/CWE-022/TaintedPath.ql

