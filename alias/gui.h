/**
 * @file
 * Shared code for the Alias and Query Dialogs
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_ALIAS_GUI_H
#define MUTT_ALIAS_GUI_H

#include <stdbool.h>
#include "mutt/lib.h"

struct Alias;
struct MuttWindow;

/**
 * AliasView - GUI data wrapping an Alias
 */
struct AliasView
{
  int num;              ///< Index number in list
  int orig_seq;         ///< Sequence in alias config file
  bool is_searched : 1; ///< Alias has been searched
  bool is_matched  : 1; ///< Search matches this Alias
  bool is_tagged   : 1; ///< Is it tagged?
  bool is_deleted  : 1; ///< Is it deleted?
  bool is_visible  : 1; ///< Is visible?
  struct Alias *alias;  ///< Alias
};

ARRAY_HEAD(AliasViewArray, struct AliasView);

/**
 * AliasMenuData - AliasView array wrapper with Pattern information - @extends Menu
 */
struct AliasMenuData
{
  char *str;                 ///< String representing the limit being used
  struct AliasViewArray ava; ///< Array of AliasView
  struct ConfigSubset *sub;  ///< Config items
};

int alias_config_observer(struct NotifyCallback *nc);

int  alias_array_alias_add    (struct AliasViewArray *ava, struct Alias *alias);
int  alias_array_alias_delete (struct AliasViewArray *ava, struct Alias *alias);
int  alias_array_count_visible(struct AliasViewArray *ava);

char *menu_create_alias_title(char *menu_name, char *limit);
int alias_recalc(struct MuttWindow *win);

#endif /* MUTT_ALIAS_GUI_H */
