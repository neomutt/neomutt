/**
 * @file
 * Shared code for the Alias and Query Dialogs
 *
 * @authors
 * Copyright (C) 2020 Romeu Vieira <romeu.bizz@gmail.com>
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * struct AliasView - GUI data wrapping an Alias
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
 * struct AliasMenuData - AliasView array wrapper with Pattern information - @extends Menu
 */
struct AliasMenuData
{
  struct AliasViewArray  ava;           ///< All Aliases/Queries
  struct AliasList      *al;            ///< Alias data
  struct ConfigSubset   *sub;           ///< Config items
  struct Menu           *menu;          ///< Menu
  struct Buffer         *query;         ///< Query string
  char                  *limit;         ///< Limit being used
  struct MuttWindow     *sbar;          ///< Status Bar
  char                  *title;         ///< Title for the status bar
  struct SearchState    *search_state;  ///< State of the current search
};

/**
 * ExpandoDataAlias - Expando UIDs for Aliases
 *
 * @sa ED_ALIAS, ExpandoDomain
 */
enum ExpandoDataAlias
{
  ED_ALI_ADDRESS = 1,          ///< Alias.addr
  ED_ALI_COMMENT,              ///< Alias.comment
  ED_ALI_FLAGS,                ///< Alias.flags
  ED_ALI_NAME,                 ///< Alias.name
  ED_ALI_NUMBER,               ///< AliasView.num
  ED_ALI_TAGGED,               ///< AliasView.tagged
  ED_ALI_TAGS,                 ///< Alias.tags
};

int alias_config_observer(struct NotifyCallback *nc);

int  alias_array_alias_add    (struct AliasViewArray *ava, struct Alias *alias);
int  alias_array_alias_delete (struct AliasViewArray *ava, const struct Alias *alias);
int  alias_array_count_visible(struct AliasViewArray *ava);

void alias_set_title(struct MuttWindow *sbar, char *menu_name, char *limit);
int alias_recalc(struct MuttWindow *win);

#endif /* MUTT_ALIAS_GUI_H */
