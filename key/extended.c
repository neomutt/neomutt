/**
 * @file
 * Set up the extended keys
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
 * @page key_extended Set up the extended keys
 *
 * Set up the extended keys
 */

#include "config.h"
#include <stdbool.h>
#include <strings.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "extended.h"
#include "keymap.h"

/**
 * struct Extkey - Map key names from NeoMutt's style to Curses style
 */
struct Extkey
{
  const char *name; ///< NeoMutt key name
  const char *sym;  ///< Curses key name
};

/**
 * struct ExtKeys - Mapping between NeoMutt and Curses key names
 */
static const struct Extkey ExtKeys[] = {
  { "<c-up>", "kUP5" },
  { "<s-up>", "kUP" },
  { "<a-up>", "kUP3" },

  { "<s-down>", "kDN" },
  { "<a-down>", "kDN3" },
  { "<c-down>", "kDN5" },

  { "<c-right>", "kRIT5" },
  { "<s-right>", "kRIT" },
  { "<a-right>", "kRIT3" },

  { "<s-left>", "kLFT" },
  { "<a-left>", "kLFT3" },
  { "<c-left>", "kLFT5" },

  { "<s-home>", "kHOM" },
  { "<a-home>", "kHOM3" },
  { "<c-home>", "kHOM5" },

  { "<s-end>", "kEND" },
  { "<a-end>", "kEND3" },
  { "<c-end>", "kEND5" },

  { "<s-next>", "kNXT" },
  { "<a-next>", "kNXT3" },
  { "<c-next>", "kNXT5" },

  { "<s-prev>", "kPRV" },
  { "<a-prev>", "kPRV3" },
  { "<c-prev>", "kPRV5" },

  { 0, 0 },
};

/**
 * ext_key_find - Find the curses name for a key
 * @param key Key name
 * @retval ptr Curses name
 *
 * Look up NeoMutt's name for a key and find the ncurses extended name for it.
 *
 * @note This returns a static string.
 */
const char *ext_key_find(const char *key)
{
  for (int i = 0; ExtKeys[i].name; i++)
  {
    if (strcasecmp(key, ExtKeys[i].name) == 0)
      return ExtKeys[i].sym;
  }
  return 0;
}

/**
 * ext_keys_init - Initialise map of ncurses extended keys
 *
 * Determine the keycodes for ncurses extended keys and fill in the KeyNames array.
 *
 * This function must be called *after* initscr(), or mutt_tigetstr() fails.
 * This creates a bit of a chicken-and-egg problem because km_init() is called
 * prior to start_curses().  This means that the default keybindings can't
 * include any of the extended keys because they won't be defined until later.
 */
void ext_keys_init(void)
{
  use_extended_names(true);

  for (int i = 0; KeyNames[i].name; i++)
  {
    if (KeyNames[i].value == -1)
    {
      const char *keyname = ext_key_find(KeyNames[i].name);

      if (keyname)
      {
        const char *s = mutt_tigetstr((char *) keyname);
        if (s && ((long) (s) != -1))
        {
          int code = key_defined(s);
          if (code > 0)
            KeyNames[i].value = code;
        }
      }
    }
  }
}
