/**
 * @file
 * Set up the key bindings
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page key_init Set up the key bindings
 *
 * Set up the key bindings
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "menu/lib.h"

extern const struct MenuOpSeq AliasDefaultBindings[];
extern const struct MenuOpSeq AttachmentDefaultBindings[];
#ifdef USE_AUTOCRYPT
extern const struct MenuOpSeq AutocryptDefaultBindings[];
#endif
extern const struct MenuOpSeq BrowserDefaultBindings[];
extern const struct MenuOpSeq ComposeDefaultBindings[];
extern const struct MenuOpSeq EditorDefaultBindings[];
extern const struct MenuOpSeq IndexDefaultBindings[];
extern const struct MenuOpSeq PagerDefaultBindings[];
extern const struct MenuOpSeq PgpDefaultBindings[];
extern const struct MenuOpSeq PostponedDefaultBindings[];
extern const struct MenuOpSeq QueryDefaultBindings[];
extern const struct MenuOpSeq SmimeDefaultBindings[];

/**
 * create_bindings - Attach a set of keybindings to a Menu
 * @param map   Key bindings
 * @param mtype Menu type, e.g. #MENU_PAGER
 */
static void create_bindings(const struct MenuOpSeq *map, enum MenuType mtype)
{
  STAILQ_INIT(&Keymaps[mtype]);

  for (int i = 0; map[i].op != OP_NULL; i++)
    if (map[i].seq)
      km_bind(map[i].seq, mtype, map[i].op, NULL, NULL, NULL);
}

/**
 * km_init - Initialise all the menu keybindings
 */
void km_init(void)
{
  memset(Keymaps, 0, sizeof(struct KeymapList) * MENU_MAX);

  create_bindings(AliasDefaultBindings, MENU_ALIAS);
  create_bindings(AttachmentDefaultBindings, MENU_ATTACHMENT);
#ifdef USE_AUTOCRYPT
  create_bindings(AutocryptDefaultBindings, MENU_AUTOCRYPT);
#endif
  create_bindings(BrowserDefaultBindings, MENU_FOLDER);
  create_bindings(ComposeDefaultBindings, MENU_COMPOSE);
  create_bindings(DialogDefaultBindings, MENU_DIALOG);
  create_bindings(EditorDefaultBindings, MENU_EDITOR);
  create_bindings(GenericDefaultBindings, MENU_GENERIC);
  create_bindings(IndexDefaultBindings, MENU_INDEX);
  create_bindings(PagerDefaultBindings, MENU_PAGER);
  create_bindings(PgpDefaultBindings, MENU_PGP);
  create_bindings(PostponedDefaultBindings, MENU_POSTPONED);
  create_bindings(QueryDefaultBindings, MENU_QUERY);
  create_bindings(SmimeDefaultBindings, MENU_SMIME);
}

/**
 * mutt_keymaplist_free - Free a List of Keymaps
 * @param km_list List of Keymaps to free
 */
static void mutt_keymaplist_free(struct KeymapList *km_list)
{
  struct Keymap *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, km_list, entries, tmp)
  {
    STAILQ_REMOVE(km_list, np, Keymap, entries);
    mutt_keymap_free(&np);
  }
}

/**
 * mutt_keys_cleanup - Free the key maps
 */
void mutt_keys_cleanup(void)
{
  for (enum MenuType i = 1; i < MENU_MAX; i++)
  {
    mutt_keymaplist_free(&Keymaps[i]);
  }
}

/**
 * mutt_init_abort_key - Parse the abort_key config string
 *
 * Parse the string into `$abort_key` and put the keycode into AbortKey.
 */
void mutt_init_abort_key(void)
{
  keycode_t buf[2];
  const char *const c_abort_key = cs_subset_string(NeoMutt->sub, "abort_key");
  size_t len = parsekeys(c_abort_key, buf, countof(buf));
  if (len == 0)
  {
    mutt_error(_("Abort key is not set, defaulting to Ctrl-G"));
    AbortKey = ctrl('G');
    return;
  }
  if (len > 1)
  {
    mutt_warning(_("Specified abort key sequence (%s) will be truncated to first key"),
                 c_abort_key);
  }
  AbortKey = buf[0];
}

/**
 * main_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
int main_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  if (!mutt_str_equal(ev_c->name, "abort_key"))
    return 0;

  mutt_init_abort_key();
  mutt_debug(LL_DEBUG5, "config done\n");
  return 0;
}
