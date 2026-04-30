/**
 * @file
 * Keymap handling
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page key_keymap Keymap handling
 *
 * Keymap handling
 */

#include "config.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "keymap.h"
#include "module_data.h"

/**
 * KeyNames - Key name lookup table
 */
static struct Mapping KeyNames[] = {
  // clang-format off
  { "<PageUp>",      KEY_PPAGE },
  { "<PageDown>",    KEY_NPAGE },
  { "<Up>",          KEY_UP },
  { "<Down>",        KEY_DOWN },
  { "<Right>",       KEY_RIGHT },
  { "<Left>",        KEY_LEFT },
  { "<Delete>",      KEY_DC },
  { "<BackSpace>",   KEY_BACKSPACE },
  { "<Insert>",      KEY_IC },
  { "<Home>",        KEY_HOME },
  { "<End>",         KEY_END },
  { "<Enter>",       '\n' },
  { "<Return>",      '\r' },
#ifdef KEY_ENTER
  { "<KeypadEnter>", KEY_ENTER },
#else
  { "<KeypadEnter>", '\n' },
#endif
  { "<Esc>",         '\033' }, // Escape
  { "<Tab>",         '\t' },
  { "<Space>",       ' ' },
  { "<Hash>",        '#' },
  { "<Semicolon>",   ';' },
  { "<DoubleQuote>", '"' },
  { "<Quote>",       '\'' },
  { "<Backslash>",   '\\' },
  { "<Backtick>",    '`' },
  { "<Dollar>",      '$' },
  { "<Less>",        '<' },
  { "<Greater>",     '>' },
#ifdef KEY_BTAB
  { "<BackTab>",     KEY_BTAB },
#endif
#ifdef KEY_NEXT
  { "<Next>",        KEY_NEXT },
#endif
  /* extensions supported by ncurses.  values are filled in during initialization */

  /* CTRL+key */
  { "<C-Up>",    -1 },
  { "<C-Down>",  -1 },
  { "<C-Left>",  -1 },
  { "<C-Right>", -1 },
  { "<C-Home>",  -1 },
  { "<C-End>",   -1 },
  { "<C-Next>",  -1 },
  { "<C-Prev>",  -1 },

  /* SHIFT+key */
  { "<S-Up>",    -1 },
  { "<S-Down>",  -1 },
  { "<S-Left>",  -1 },
  { "<S-Right>", -1 },
  { "<S-Home>",  -1 },
  { "<S-End>",   -1 },
  { "<S-Next>",  -1 },
  { "<S-Prev>",  -1 },

  /* ALT+key */
  { "<A-Up>",    -1 },
  { "<A-Down>",  -1 },
  { "<A-Left>",  -1 },
  { "<A-Right>", -1 },
  { "<A-Home>",  -1 },
  { "<A-End>",   -1 },
  { "<A-Next>",  -1 },
  { "<A-Prev>",  -1 },
  { NULL, 0 },
  // clang-format on
};

/**
 * keymap_get_key_names - Get the KeyNames lookup table
 * @retval ptr KeyNames array
 */
struct Mapping *keymap_get_key_names(void)
{
  return KeyNames;
}

/**
 * keymap_alloc - Allocate space for a sequence of keys
 * @param len  Number of keys
 * @param keys Array of keys
 * @retval ptr Sequence of keys
 */
struct Keymap *keymap_alloc(size_t len, keycode_t *keys)
{
  struct Keymap *km = MUTT_MEM_CALLOC(1, struct Keymap);

  km->len = len;
  km->keys = MUTT_MEM_CALLOC(len, keycode_t);
  memcpy(km->keys, keys, len * sizeof(keycode_t));

  return km;
}

/**
 * keymap_free - Free a Keymap
 * @param pptr Keymap to free
 */
void keymap_free(struct Keymap **pptr)
{
  if (!pptr || !*pptr)
    return;

  struct Keymap *km = *pptr;
  FREE(&km->macro);
  FREE(&km->desc);
  FREE(&km->keys);

  FREE(pptr);
}

/**
 * keymaplist_free - Free a List of Keymaps
 * @param kml List of Keymaps to free
 */
void keymaplist_free(struct KeymapList *kml)
{
  struct Keymap *km = NULL;
  struct Keymap *km_tmp = NULL;
  STAILQ_FOREACH_SAFE(km, kml, entries, km_tmp)
  {
    STAILQ_REMOVE(kml, km, Keymap, entries);
    keymap_free(&km);
  }
}

/**
 * keymap_compare - Compare two keymaps' keyscodes and return the bigger one
 * @param[in]  km1 First keymap to compare
 * @param[in]  km2 Second keymap to compare
 * @param[out] pos Position where the two keycodes differ
 * @retval ptr Keymap with a bigger ASCII keycode
 */
struct Keymap *keymap_compare(struct Keymap *km1, struct Keymap *km2, size_t *pos)
{
  *pos = 0;

  while ((*pos < km1->len) && (*pos < km2->len))
  {
    if (km1->keys[*pos] < km2->keys[*pos])
      return km2;

    if (km1->keys[*pos] > km2->keys[*pos])
      return km1;

    *pos = *pos + 1;
  }

  return NULL;
}

/**
 * keymap_get_name - Get the human name for a key
 * @param[in]  c   Key code
 * @param[out] buf Buffer for the result
 *
 * The key is rendered using the modern `<…>` grammar where applicable:
 *   - control bytes  -> `<C-x>` (e.g. `0x01` -> `<C-a>`)
 *   - 0x7f (DEL)     -> `<C-?>`
 *   - named keys     -> table lookup (`<Up>`, `<Esc>`, …)
 *   - F-keys         -> `<F1>`, `<F2>`, …
 *   - printable      -> the literal character
 *   - everything else -> `<NNN>` octal fallback
 */
void keymap_get_name(int c, struct Buffer *buf)
{
  const char *name = mutt_map_get_name(c, KeyNames);
  if (name)
  {
    buf_addstr(buf, name);
    return;
  }

  if ((c < 256) && (c > -128) && iscntrl((unsigned char) c))
  {
    if (c < 0)
      c += 256;

    if (c == 0x7f)
    {
      buf_addstr(buf, "<C-?>");
    }
    else if ((c >= 1) && (c <= 26))
    {
      /* 0x01 .. 0x1A  ->  <C-a> .. <C-z> */
      buf_add_printf(buf, "<C-%c>", 'a' + c - 1);
    }
    else if ((c >= 0x1c) && (c <= 0x1f))
    {
      /* 0x1c..0x1f  ->  <C-\>, <C-]>, <C-^>, <C-_> */
      buf_add_printf(buf, "<C-%c>", '@' + c);
    }
    else if (c < 128)
    {
      /* ^@ (NUL) and any other control byte without a named entry */
      buf_addch(buf, '^');
      buf_addch(buf, (c + '@') & 0x7f);
    }
    else
    {
      buf_add_printf(buf, "\\%d%d%d", c >> 6, (c >> 3) & 7, c & 7);
    }
  }
  else if ((c >= KEY_F0) && (c < KEY_F(256))) /* this maximum is just a guess */
  {
    buf_add_printf(buf, "<F%d>", c - KEY_F0);
  }
  else if ((c < 256) && (c >= -128) && IsPrint(c))
  {
    buf_add_printf(buf, "%c", (unsigned char) c);
  }
  else
  {
    buf_add_printf(buf, "<%ho>", (unsigned short) c);
  }
}

/**
 * keymap_expand_key - Get the key string bound to a Keymap
 * @param[in]  km  Keybinding map
 * @param[out] buf Buffer for the result
 * @retval true Success
 *
 * Consecutive `<Esc>` + key pairs are folded into `<A-…>` form, so that
 * `\ea` (which is stored internally as ESC followed by `a`) is shown as the
 * modern `<A-a>`.  ESC followed by another ESC is not folded.
 */
bool keymap_expand_key(struct Keymap *km, struct Buffer *buf)
{
  if (!km || !buf)
    return false;

  struct Buffer *tmp = buf_pool_get();

  int i = 0;
  while (i < km->len)
  {
    int c = km->keys[i];
    /* Try to fold `<Esc> + key`  ->  `<A-key>` */
    if ((c == '\033') && ((i + 1) < km->len) && (km->keys[i + 1] != '\033'))
    {
      int next = km->keys[i + 1];
      buf_reset(tmp);
      keymap_get_name(next, tmp);
      const char *name = buf_string(tmp);
      size_t nlen = buf_len(tmp);

      if ((nlen >= 2) && (name[0] == '<') && (name[nlen - 1] == '>'))
      {
        /* Strip surrounding `<` `>` and prepend `A-` */
        buf_addstr(buf, "<A-");
        buf_addstr_n(buf, name + 1, nlen - 2);
        buf_addch(buf, '>');
      }
      else
      {
        buf_add_printf(buf, "<A-%s>", name);
      }
      i += 2; // consumed this key and the next key
      continue;
    }

    keymap_get_name(c, buf);
    i++;
  }

  buf_pool_release(&tmp);
  return true;
}

/**
 * keymap_expand_string - Get a human-readable key string
 * @param[in]  str Raw key string
 * @param[out] buf Buffer for the key string
 */
void keymap_expand_string(const char *str, struct Buffer *buf)
{
  if (!str)
    return;

  for (; *str; str++)
  {
    keymap_get_name(*str, buf);
  }
}

/**
 * parse_fkey - Parse a function key string
 * @param str String to parse
 * @retval num Number of the key
 * @retval  -1 Error
 *
 * Given "<f8>", it will return 8.
 */
int parse_fkey(char *str)
{
  char *t = NULL;
  int n = 0;

  if ((str[0] != '<') || (mutt_tolower(str[1]) != 'f'))
    return -1;

  for (t = str + 2; *t && mutt_isdigit(*t); t++)
  {
    n *= 10;
    n += *t - '0';
  }

  if (*t != '>')
    return -1;
  return n;
}

/**
 * parse_keycode - Parse a numeric keycode
 * @param str String to parse
 * @retval num Number of the key
 *
 * This function parses the string `<NNN>` and uses the octal value as the key
 * to bind.
 */
int parse_keycode(const char *str)
{
  char *end_char = NULL;
  long int result = strtol(str + 1, &end_char, 8);

  /* allow trailing whitespace, eg.  < 1001 > */
  while (mutt_isspace(*end_char))
    end_char++;

  /* negative keycodes don't make sense, also detect overflow */
  if ((*end_char != '>') || (result < 0) || (result == LONG_MAX))
  {
    return -1;
  }

  return result;
}

/**
 * apply_ctrl - Convert a base character to its Control-modified form
 * @param ch Input character
 * @retval num Resulting keycode (-1 on failure)
 *
 * Implements the standard ASCII Ctrl folding:
 *   - letters       -> ch & 0x1f      (`C-a` -> 0x01, `C-z` -> 0x1a)
 *   - `@` or space  -> 0x00 (NUL)
 *   - `[` `\` `]` `^` `_` -> 0x1b..0x1f
 *   - `?`           -> 0x7f (DEL)
 *   - any other     -> -1
 */
static int apply_ctrl(int ch)
{
  if (mutt_isalpha(ch))
    return mutt_toupper(ch) & 0x1f;
  if ((ch == '@') || (ch == ' '))
    return 0;
  if ((ch >= '[') && (ch <= '_'))
    return ch & 0x1f;
  if (ch == '?')
    return 0x7f;
  return -1;
}

/**
 * parse_modifier_key - Try to parse a `<C-/A-/S-/M-…>` modifier-key token
 * @param[in]  inner Inner content of the `<…>` token (e.g. "C-A-x")
 * @param[out] d     Buffer to write keycodes into
 * @param[in]  max   Capacity of `d`
 * @retval num   Number of keycodes written (>0 on success, 0 if not a
 *               modifier-key token, -1 on parse error)
 *
 * Accepts any combination of the modifier prefixes (case-insensitive):
 *   - `C-` Ctrl
 *   - `A-` Alt
 *   - `M-` Meta (alias of Alt)
 *   - `S-` Shift
 *
 * The order of the prefixes is irrelevant, e.g. `<C-A-x>`, `<A-C-x>` and
 * `<M-C-x>` are equivalent.
 *
 * The base may be:
 *   - a single ASCII character (`<C-a>`, `<S-1>`)
 *   - a symbolic key name (`<A-Tab>`, `<C-Up>`, `<F4>` etc.)
 *
 * Alt is encoded as an `<Esc>` prefix followed by the base keycode, which
 * matches what xterm-style terminals send for `Alt-key`.
 */
static int parse_modifier_key(const char *inner, keycode_t *d, size_t max)
{
  if (!inner || !d || (max < 1))
    return 0;

  bool mod_c = false;
  bool mod_a = false;
  bool mod_s = false;
  const char *s = inner;

  /* Parse modifier prefixes greedily, requiring at least one character to
   * remain after the last `X-` (otherwise `S-` could swallow the trailing
   * dash of a real binding such as `<S->`). */
  while ((s[0] != '\0') && (s[1] == '-') && (s[2] != '\0'))
  {
    char c = mutt_tolower((unsigned char) s[0]);
    if (c == 'c')
      mod_c = true;
    else if ((c == 'a') || (c == 'm'))
      mod_a = true;
    else if (c == 's')
      mod_s = true;
    else
      break;
    s += 2;
  }

  if (!mod_c && !mod_a && !mod_s)
    return 0; /* no modifier prefix found */

  size_t base_len = strlen(s);
  if (base_len == 0)
    return -1;

  size_t out = 0;

  /* Alt becomes an ESC prefix followed by the rest. */
  if (mod_a)
  {
    if (max < 2)
      return -1;
    d[out++] = '\033';
  }

  int base_kc = -1;

  if (base_len == 1)
  {
    int ch = (unsigned char) s[0];
    if (mod_s && mutt_isalpha(ch))
      ch = mutt_toupper(ch);
    if (mod_c)
    {
      ch = apply_ctrl(ch);
      if (ch < 0)
        return -1;
    }
    base_kc = ch;
  }
  else
  {
    /* Symbolic name.  Try modifier-prefixed forms first so that the
     * `<C-Up>` / `<S-Up>` / `<A-Up>` curses-extended entries are honoured.
     * Then fall back to the bare `<base>` lookup, in which case Ctrl/Shift
     * modifiers cannot be applied without an explicit keycode. */
    char buf[128] = { 0 };

    if (mod_c && mod_s)
    {
      snprintf(buf, sizeof(buf), "<C-S-%s>", s);
      base_kc = mutt_map_get_value(buf, KeyNames);
      if (base_kc != -1)
      {
        mod_c = false;
        mod_s = false;
      }
    }
    if ((base_kc == -1) && mod_c)
    {
      snprintf(buf, sizeof(buf), "<C-%s>", s);
      base_kc = mutt_map_get_value(buf, KeyNames);
      if (base_kc != -1)
        mod_c = false;
    }
    if ((base_kc == -1) && mod_s)
    {
      snprintf(buf, sizeof(buf), "<S-%s>", s);
      base_kc = mutt_map_get_value(buf, KeyNames);
      if (base_kc != -1)
        mod_s = false;
    }
    if (base_kc == -1)
    {
      snprintf(buf, sizeof(buf), "<%s>", s);
      base_kc = mutt_map_get_value(buf, KeyNames);
      if (base_kc == -1)
      {
        int fk = parse_fkey(buf);
        if (fk > 0)
          base_kc = KEY_F(fk);
      }
      if (base_kc == -1)
      {
        int kc = parse_keycode(buf);
        if (kc > 0)
          base_kc = kc;
      }
    }

    if (base_kc == -1)
      return -1;

    /* Any C/S modifier still active means we couldn't combine it with the
     * special key (no curses entry, etc.) — bail out. */
    if (mod_c || mod_s)
      return -1;
  }

  if (base_kc <= 0)
    return -1;

  d[out++] = (keycode_t) base_kc;
  return (int) out;
}

/**
 * parse_keys - Parse a key string into key codes
 * @param str Key string
 * @param d   Array for key codes
 * @param max Maximum length of key sequence
 * @retval num Length of key sequence
 *
 * Recognised forms inside `<…>`:
 *   - Symbolic key names (`<Up>`, `<Tab>`, `<Hash>`, …) — see KeyNames
 *   - F-keys (`<F1>`, `<F30>`, …)
 *   - Octal keycodes (`<177>`, `<0407>`, …)
 *   - Modifier prefixes (`<C-a>`, `<A-x>`, `<C-A-S-Right>`, …)
 *
 * Plain bytes (and backslash escapes that the tokenizer has already
 * resolved) become themselves.
 */
size_t parse_keys(const char *str, keycode_t *d, size_t max)
{
  int n;
  size_t len = max;
  char buf[128] = { 0 };
  char c;
  char *t = NULL;

  mutt_str_copy(buf, str, sizeof(buf));
  char *s = buf;

  while (*s && len)
  {
    *d = '\0';
    if ((*s == '<') && (t = strchr(s, '>')))
    {
      t++;
      c = *t;
      *t = '\0';

      n = mutt_map_get_value(s, KeyNames);
      if (n != -1)
      {
        s = t;
        *d = n;
      }
      else if ((n = parse_fkey(s)) > 0)
      {
        s = t;
        *d = KEY_F(n);
      }
      else if ((n = parse_keycode(s)) > 0)
      {
        s = t;
        *d = n;
      }
      else
      {
        /* Try the modern `<C-/A-/S-/M-…>` grammar.  The function may emit
         * one or two keycodes (e.g. Alt-x becomes `<Esc>` + `x`). */
        size_t inner_len = strlen(s);
        if ((inner_len >= 3) && (s[inner_len - 1] == '>'))
        {
          s[inner_len - 1] = '\0'; /* temporarily strip closing `>` */
          int written = parse_modifier_key(s + 1, d, len);
          s[inner_len - 1] = '>';
          if (written > 0)
          {
            s = t;
            /* The bottom of the loop already advances `d` and `len` by 1.
             * Account here for any *additional* keycodes emitted. */
            d += (written - 1);
            len -= (written - 1);
          }
        }
      }

      *t = c;
    }

    if (!*d)
    {
      *d = (unsigned char) *s;
      s++;
    }
    d++;
    len--;
  }

  return max - len;
}
