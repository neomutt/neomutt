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

/**
 * KeyNames - Key name lookup table
 */
struct Mapping KeyNames[] = {
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
  // clang-format off
};

/**
 * keymap_alloc - Allocate space for a sequence of keys
 * @param len  Number of keys
 * @param keys Array of keys
 * @retval ptr Sequence of keys
 */
struct Keymap *keymap_alloc(size_t len, keycode_t *keys)
{
  struct Keymap *km = mutt_mem_calloc_T(1, struct Keymap);

  km->len = len;
  km->keys = mutt_mem_calloc_T(len, keycode_t);
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

    if (c < 128)
    {
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
 */
bool keymap_expand_key(struct Keymap *km, struct Buffer *buf)
{
  if (!km || !buf)
    return false;

  for (int i = 0; i < km->len; i++)
  {
    keymap_get_name(km->keys[i], buf);
  }

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
 * parse_keys - Parse a key string into key codes
 * @param str Key string
 * @param d   Array for key codes
 * @param max Maximum length of key sequence
 * @retval num Length of key sequence
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
