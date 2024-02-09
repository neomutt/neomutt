## Expando Types

Empty, Text, Expando, Padding, Conditional, Conditional Date, Container.

Strip down `ExpandoNode` to:

```c
struct ExpandoNode
{
  enum ExpandoNodeType type;
  struct ExpandoNode  *next;

  void *ndata;
  void (*ndata_free)(void **ptr);
};
```

Each node type has its own ndata.

### Empty

No private data :-)

### Text

```c
struct ExpandoNodeText
{
  const char *start;
  const char *end;
};
```

### Padding

```c
struct ExpandoNodePad
{
  enum ExpandoPadType pad_type;
  char                pad_char;
}:
```

### Conditional

```c
struct ExpandoNodeConditional
{
  struct ExpandoNode *condition;
  struct ExpandoNode *if_true_tree;
  struct ExpandoNode *if_false_tree;
};
```

### Conditional Date

```c
struct ExpandoNodeConditionalDate
{
  int    count;
  char   period;
  time_t multiplier;
};
```

### Container

```c
struct ExpandoNodeContainer
{
  struct ExpandoNode *children;
};
```

## Custom Nodes

If an `struct ExpandoDefinition` has a custom parser, it may return a custom `ExpandoNode`, with a custom child.

### Custom

```c
struct ExpandoNode
{
  ENT_CUSTOM;
};
```

```c
struct ExpandoNodeMyType
{
  const char *data1;
  int         value2;
};
```

### Tree/Subject Splitting

`$index_format`s `%s` expando contains two parts: tree characters and the subject.

If we introduce a custom parser, we could split this in two using a Container node.

- `Container { Expando-for-tree-chars, Expando-for-subject }`

## Expando Node

Upgrade `ExpandoNode` to have an API.

We want to be able to render the string, test for "true-ness" and possibly generate a new format string.

```c
struct ExpandoNode
{
  enum ExpandoNodeType type;
  struct ExpandoNode  *next;

  void *ndata;
  void (*ndata_free)(void **ptr);

  /**
   * render - Render an Expando
   * @param[in]  node      ExpandoNode to format
   * @param[in]  max_cols  Number of screen columns
   * @param[in]  flags     Flags, see #MuttFormatFlags
   * @param[out] buf       Buffer for the result
   * @retval num Number of bytes written to buf
   *
   * Each callback function implements some expandos, e.g.
   *
   * | Expando | Description
   * | :------ | :----------
   * | \%t     | Title
   */
  int (*render)(const struct ExpandoNode *node, int max_cols, MuttFormatFlags flags, struct Buffer *buf);

  /**
   * is_true - Test a conditional Expando
   * @param[in]  node      ExpandoNode to test
   * @retval true Expando condition is true
   */
  bool (*is_true)(const struct ExpandoNode *node);

  /**
   * generate - Create a new format string
   * @param[in]  node ExpandoNode to formay
   * @param[out] buf  Buffer for the result
   * @retval num Number of bytes written to buf
   */
  int (*generate)(const struct ExpandoNode *node, struct Buffer *buf);
};
```

- `render()`
  Allow the expando code to render nodes without knowing what type they are.

- `is_true()`
  Many expandos can be used conditionally.
  e.g. Numbers that are greater than zero; Strings that are non-empty
  A dedicated test function allows us to delegate the tricky cases, like conditional dates.

- `generate()` - In the future
  Generate a format string from the Node tree.
  Many programs allow you to tweak the order and size of table columns.

## Colouring

Currently, the colours are magic control codes that are inserted into the text.
The Expando code needs to recognise them and ignore them when calculating widths.

Only Expando Nodes are coloured and only `$index_format`s `%s` requires two colours.
If `%s` is split into two pieces, then colouring can be done more easily.

If the render function can return three strings, the expando code can make smarter decisions.

- Render `%X` -\> `[colour]` + `John Smith` + `[end-colour]`

The prefix/suffix parts can be considered zero-width.
The text can be truncated if it needs to be, leading to:

- `[colour]` + `John Sm` + `[end-colour]`

## Resizing Nodes

There are over 200 expando callbacks.
Most of them need to do some form of formatting.

The render function knows how much space is available, so it makes sense for it to do the formatting.

### Simplify the callbacks

```c
int index_L(const struct ExpandoNode *node, char *buf, int buf_len, int cols_len, void *data, MuttFormatFlags flags)
{
  assert(node->type == ENT_EXPANDO);

  const struct HdrFormatInfo *hfi = data;
  const struct Email *e = hfi->email;

  char fmt[128] = { 0 };
  char tmp[128] = { 0 };

  make_from(e->env, tmp, sizeof(tmp), true, flags);
  render_string(fmt, sizeof(fmt), tmp, flags, MT_COLOR_INDEX_AUTHOR,
                MT_COLOR_INDEX, node, NO_TREE);
  return snprintf(buf, buf_len, "%s", fmt);
}
```

might become:

```c
int index_L(const struct ExpandoNode *node, MuttFormatFlags flags, struct Buffer *buf)
{
  assert(node->type == ENT_EXPANDO);

  const struct HdrFormatInfo *hfi = node->ndata;
  const struct Email *e = hfi->email;

  return make_from(e->env, buf, true, flags);
}
```

### Resizing all the Nodes

Many Windows in NeoMutt are dynamically sized.
They can be MINIMISE, FIXED or MAXIMISE.
Each requests an amount of space and the reflow functions allocate it fairly.

The Expandos could be a simpler version of the Windows.

The first pass would be to collect all the strings into Nodes.

- `%a%b%* %c` -> `apple` + `banana` + Pad-space + `cherry`

We have some Padding, so we'll need to check the available space.
There's enough room for `%a`, but not all of `%b`, so we get:

- `apple` + `bana` + Pad-space + `cherry`

which renders to:

- `applebanacherry`

With colours that might be:

- `[+a]apple[-a][+b]bana[-b][+c]cherry[-c]`

## g0mb4's notes

I would use the following 'base' `ExpandoNode`:

```c
struct ExpandoNode
{
  enum ExpandoNodeType type;
  struct ExpandoNode  *next;

  const char *start;
  const char *end;

  void *ndata;
  void (*ndata_free)(void **ptr);
};
```

*Almost* all Expando need to store some kind of textual data. Pad could use UTF-8 not just ASCII this way and the parses allows this.

So you want to change the current way of hooking expando callbacks. We would store the callback pointer instead of the ```did```, ```uid```.
