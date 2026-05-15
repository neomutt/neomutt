# Adding a New Expando

This guide walks through adding a new expando (format-string placeholder) to a
NeoMutt config option such as `$status_format`, `$index_format`, etc.  It
assumes you are familiar with the C codebase.

A worked example at the end shows how `%{last-log}` was added to
`$status_format`.

## Architecture

An expando is a `printf`-style placeholder in a user-configurable format
string.  At parse time NeoMutt walks the format string and builds a tree of
`ExpandoNode`s; at render time each node asks a callback for its data and
formats it into the output buffer.

```diagram
                  ╭───────────────────╮
   format string  │ "...%n new...%v"  │  (from config, e.g. $status_format)
                  ╰─────────┬─────────╯
                            │  expando_parse()
                            ▼
                  ╭───────────────────╮
                  │ ExpandoNode tree  │  uses ExpandoDefinition[] table
                  │ (text, expando,   │  to map %n / %{long} → (did, uid)
                  │  padding, cond …) │
                  ╰─────────┬─────────╯
                            │  node_render()
                            ▼
                  ╭───────────────────╮
                  │ Render callbacks  │  ExpandoRenderCallback[] table
                  │ (did, uid) →      │  dispatches to get_string() /
                  │ get_string/number │  get_number() per node
                  ╰─────────┬─────────╯
                            ▼
                  ╭───────────────────╮
                  │ rendered output   │  written into a Buffer
                  ╰───────────────────╯
```

The four moving parts are:

1. **UID** — a unique enum value (`ED_xxx_yyy`) identifying the expando
   *within* its data domain.  Domains are listed in [expando/domain.h](../expando/domain.h)
   (e.g. `ED_GLOBAL`, `ED_INDEX`, `ED_EMAIL`).  Domain UIDs live next to the
   data they describe — global ones in [expando/uid.h](../expando/uid.h),
   subsystem-specific ones in headers such as `index/expando_index.h`,
   `compose/expando.h`, etc.

2. **`ExpandoDefinition` table entry** — the parser table for *one* config
   option (e.g. `StatusFormatDef`, `IndexFormatDef`).  It maps the textual
   form (`"n"` for `%n`, `"new-count"` for `%{new-count}`) to a
   `(domain, uid)` pair.  Defined in the subsystem's `config.c`.

3. **Render callback** — a `get_string_t` or `get_number_t` function that
   produces the value for a `(domain, uid)` pair.  Lives in the subsystem's
   `expando_*.c` file and is registered in an `ExpandoRenderCallback[]` array
   (e.g. `StatusRenderCallbacks`, `IndexRenderCallbacks`).

4. **Debug name** — a `DEBUG_NAME(...)` line in
   [debug/names_expando.c](../debug/names_expando.c) so the debug logger can
   stringify the new UID.

The same definition table can be referenced by several config options (see
e.g. `StatusFormatDef` shared by `$status_format`, `$ts_status_format`,
`$ts_icon_format`, `$new_mail_command`).  The same render-callback table is
in turn used to render any of them.

## Step by Step

### 1. Pick a domain and add the UID

Find the relevant `ExpandoDomain` in [expando/domain.h](../expando/domain.h).
Open the matching UID enum and add a new value, keeping the list
alphabetical (the existing files use that convention):

```c
// expando/uid.h
enum ExpandoDataGlobal
{
  ED_GLO_CONFIG_SORT = 1,
  ...
  ED_GLO_HOSTNAME,
  ED_GLO_LAST_LOG,             ///< Last log message (warning, error or perror)
  ED_GLO_PADDING_EOL,
  ...
};
```

UIDs must be non-zero (the first entry uses `= 1`).

### 2. Register the expando in the format definition

Edit the `ExpandoDefinition` array for the config option(s) the expando
belongs to.  For `$status_format` that is `StatusFormatDef` in
[index/config.c](../index/config.c):

```c
const struct ExpandoDefinition StatusFormatDef[] = {
  ...
  { "h", "hostname",   ED_GLOBAL, ED_GLO_HOSTNAME, NULL },
  { NULL,"last-log",   ED_GLOBAL, ED_GLO_LAST_LOG, NULL },
  { "l", "mailbox-size", ED_INDEX, ED_IND_MAILBOX_SIZE, NULL },
  ...
};
```

Fields, in order:

| Field        | Meaning                                                           |
|--------------|-------------------------------------------------------------------|
| `short_name` | One-letter form, e.g. `"n"` for `%n`.  `NULL` if long-form only.  |
| `long_name`  | Long form, e.g. `"new-count"` for `%{new-count}`.  May be `NULL`. |
| `did`        | Domain ID (`ED_GLOBAL`, `ED_INDEX`, …).                           |
| `uid`        | The UID you added in step 1.                                      |
| `parse`      | Custom parser, almost always `NULL` — only needed for expandos    |
|              | that take arguments (date formats, padding, hooks).               |

A `NULL` `short_name` makes the expando accessible only via the long
`%{name}` form.  Conversely, expandos with a short name *should* still get
a long name to keep format strings readable.

### 3. Implement the render callback

Add a function in the subsystem's `expando_*.c` file (for `$status_format`,
[index/expando_status.c](../index/expando_status.c)).  Use the appropriate
typedef from [expando/render.h](../expando/render.h):

```c
// String-valued expando
static void global_last_log(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  ...
  buf_strcpy(buf, message);
}

// Numeric expando — used directly by %d-style formatting and by
// conditionals such as %<n?...> ("if non-zero...")
static long index_new_count_num(const struct ExpandoNode *node, void *data,
                                MuttFormatFlags flags)
{
  const struct MenuStatusLineData *msld = data;
  ...
  return m ? m->msg_new : 0;
}
```

The `data` pointer is whatever the calling subsystem passes to
`node_render()` — for the status line that is a `struct MenuStatusLineData *`
wrapping the shared index data and the current `Menu`.  Each subsystem's
`expando_*.c` casts it to the right concrete type.

A callback may provide a string getter, a number getter, or both.  When both
are present:
- the string form is used for normal `%X` rendering,
- the number form is used for conditionals (`%<X?yes&no>`) and for any
  numeric formatting (`%4X`, padding by digits, etc.).

If only a string is provided, conditionals test "is the string empty?".

### 4. Register the callback

Add an entry to the `ExpandoRenderCallback[]` array used by the format
option(s).  For status that is `StatusRenderCallbacks` in the same file:

```c
const struct ExpandoRenderCallback StatusRenderCallbacks[] = {
  ...
  { ED_GLOBAL, ED_GLO_HOSTNAME,  global_hostname,  NULL },
  { ED_GLOBAL, ED_GLO_LAST_LOG,  global_last_log,  NULL },
  { ED_GLOBAL, ED_GLO_VERSION,   global_version,   NULL },
  ...
  { -1, -1, NULL, NULL },     // sentinel — keep this last
};
```

The `(did, uid)` pair must match what you put in the `ExpandoDefinition`
entry; otherwise `node_render()` will silently render nothing.

### 5. Add the debug name

[debug/names_expando.c](../debug/names_expando.c) provides a debug-time
`UID → string` mapping.  Add one line:

```c
DEBUG_NAME(ED_GLO_LAST_LOG);
```

inside the block for your domain.

### 6. Document it

Add a row to the expando table in [docs/config.c](../docs/config.c) under
the relevant config variable's documentation block (e.g. the `status_format`
section).  Keep the columns aligned with the other rows.

### 7. Build, test, commit

```sh
cd build
make neomutt -j$(nproc)
make test/neomutt-test -j$(nproc) && ./test/neomutt-test
```

A new expando does not strictly need a unit test, but the existing tests
exercise the parser and renderer over every defined expando, so they will
catch most mistakes (missing render callback, duplicate UID, malformed
definition, etc.).

## Special Cases

**Expandos with arguments** — e.g. `%{[fmt]}` for dates or `%{padding-soft:X}`
for padding — provide a `parse` callback in the `ExpandoDefinition` entry
instead of a `get_string`/`get_number` callback.  See `node_padding_parse()`
in [expando/node_padding.c](../expando/node_padding.c) and the
`parse_index_date*` helpers in [mutt_config.c](../mutt_config.c) for
reference implementations.  The parse callback owns the production of an
`ExpandoNode`; rendering still flows through the normal callback table, but
the node may carry extra parsed state (format string, padding char, …).

**Cross-cutting expandos** — when an expando is meaningful in several
contexts (a global like `%v`/`%{version}` is in `StatusFormatDef`,
`ComposeFormatDef`, …), add the `ExpandoDefinition` row to *each* relevant
table and the render-callback row to *each* relevant `*RenderCallbacks[]`
array.  The UID itself only needs to be defined once.

## Worked Example: `%{last-log}` in `$status_format`

This expando shows the most recent `LL_WARNING` / `LL_ERROR` / `LL_PERROR`
message from the in-memory log queue.  It has no short form.

1. **UID** — `ED_GLO_LAST_LOG` added to `enum ExpandoDataGlobal` in
   [expando/uid.h](../expando/uid.h).

2. **Definition** — added to `StatusFormatDef` in
   [index/config.c](../index/config.c):

   ```c
   { NULL, "last-log", ED_GLOBAL, ED_GLO_LAST_LOG, NULL },
   ```

3. **Callback** — added to [index/expando_status.c](../index/expando_status.c):

   ```c
   static void global_last_log(const struct ExpandoNode *node, void *data,
                               MuttFormatFlags flags, struct Buffer *buf)
   {
     const struct LogLineList lll = log_queue_get();
     const struct LogLine *ll = NULL;
     const struct LogLine *last = NULL;
     STAILQ_FOREACH(ll, &lll, entries)
     {
       if ((ll->level >= LL_PERROR) && (ll->level <= LL_WARNING))
         last = ll;
     }
     if (last && last->message)
       buf_strcpy(buf, last->message);
   }
   ```

   and registered in `StatusRenderCallbacks`:

   ```c
   { ED_GLOBAL, ED_GLO_LAST_LOG, global_last_log, NULL },
   ```

4. **Debug name** — `DEBUG_NAME(ED_GLO_LAST_LOG);` in
   [debug/names_expando.c](../debug/names_expando.c).

Total: 4 files, ~25 lines.  The expando is now usable in `$status_format`,
`$ts_status_format`, `$ts_icon_format` and `$new_mail_command` (all of
which share `StatusFormatDef`).
