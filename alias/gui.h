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

struct Alias;

/**
 * AliasView - GUI data wrapping an Alias
 */
struct AliasView
{
  int num;             ///< Index number in list
  bool is_tagged;      ///< Is it tagged?
  bool is_deleted;     ///< Is it deleted?
  struct Alias *alias; ///< Alias
};

/**
 * AliasMenuData - GUI data required to maintain the Menu
 */
struct AliasMenuData
{
  struct AliasView **av; ///< An AliasView for each Alias
  int num_views;         ///< Number of AliasViews used
  int max_views;         ///< Size of AliasView array
};

void                  menu_data_clear(struct AliasMenuData *mdata);
void                  menu_data_free (struct AliasMenuData **ptr);
struct AliasMenuData *menu_data_new  (void);

int  menu_data_alias_add   (struct AliasMenuData *mdata, struct Alias *alias);
int  menu_data_alias_delete(struct AliasMenuData *mdata, struct Alias *alias);
void menu_data_sort        (struct AliasMenuData *mdata);

int alias_sort_address(const void *a, const void *b);
int alias_sort_name   (const void *a, const void *b);

#endif /* MUTT_ALIAS_GUI_H */
