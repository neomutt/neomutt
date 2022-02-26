/**
 * @file
 * GUI present the user with a selectable list
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page lib_menu Menu
 *
 * A selectable list
 *
 * | File             | Description             |
 * | :--------------- | :---------------------- |
 * | menu/config.c    | @subpage menu_config    |
 * | menu/draw.c      | @subpage menu_draw      |
 * | menu/menu.c      | @subpage menu_menu      |
 * | menu/move.c      | @subpage menu_move      |
 * | menu/observer.c  | @subpage menu_observer  |
 * | menu/window.c    | @subpage menu_window    |
 */

#ifndef MUTT_MENU_LIB_H
#define MUTT_MENU_LIB_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "type.h"

struct ConfigSubset;

// Observers of #NT_MENU will not be passed any Event data.
typedef uint8_t MenuRedrawFlags;       ///< Flags, e.g. #MENU_REDRAW_INDEX
#define MENU_REDRAW_NO_FLAGS        0  ///< No flags are set
#define MENU_REDRAW_INDEX     (1 << 0) ///< Redraw the index
#define MENU_REDRAW_MOTION    (1 << 1) ///< Redraw after moving the menu list
#define MENU_REDRAW_CURRENT   (1 << 2) ///< Redraw the current line of the menu
#define MENU_REDRAW_FULL      (1 << 3) ///< Redraw everything
#define MENU_REDRAW_BODY      (1 << 4) ///< Redraw the pager
#define MENU_REDRAW_FLOW      (1 << 5) ///< Used by pager to reflow text

ARRAY_HEAD(DialogLines, char *);

/**
 * @defgroup menu_api Menu API
 *
 * The Menu API
 *
 * GUI selectable list of items
 */
struct Menu
{
  int current;              ///< Current entry
  int max;                  ///< Number of entries in the menu
  MenuRedrawFlags redraw;   ///< When to redraw the screen
  enum MenuType type;       ///< Menu definition for keymap entries
  int page_len;             ///< Number of entries per screen
  bool tag_prefix : 1;      ///< User has pressed <tag-prefix>
  struct MuttWindow *win;   ///< Window holding the Menu
  struct ConfigSubset *sub; ///< Inherited config items

  /* Setting a non-empty dialog overrides normal menu behavior.
   * In dialog mode menubar is hidden and prompt keys are checked before
   * normal menu movement keys. This can cause problems with scrolling, if
   * prompt keys override movement keys.  */
  struct DialogLines dialog;   ///< Dialog lines themselves
  char *prompt;                ///< Prompt for user, similar to mutt_multi_choice
  char *keys;                  ///< Keys used in the prompt

  /* the following are used only by menu_loop() */
  int top;                ///< Entry that is the top of the current page
  int old_current;        ///< For driver use only
  int search_dir;         ///< Direction of search
  int num_tagged;         ///< Number of tagged entries
  bool custom_search : 1; ///< The menu implements its own non-Menu::search()-compatible search, trickle OP_SEARCH*

  /**
   * @defgroup menu_make_entry make_entry()
   * @ingroup menu_api
   *
   * make_entry - Format a item for a menu
   * @param[in]  menu   Menu containing items
   * @param[out] buf    Buffer in which to save string
   * @param[in]  buflen Buffer length
   * @param[in]  line   Menu line number
   */
  void (*make_entry)(struct Menu *menu, char *buf, size_t buflen, int line);

  /**
   * @defgroup menu_search search()
   * @ingroup menu_api
   *
   * search - Search a menu for a item matching a regex
   * @param menu Menu to search
   * @param rx   Regex to match
   * @param line Menu entry to match
   * @retval  0 Success
   * @retval >0 Error, e.g. REG_NOMATCH
   */
  int (*search)(struct Menu *menu, regex_t *rx, int line);

  /**
   * @defgroup menu_tag tag()
   * @ingroup menu_api
   *
   * tag - Tag some menu items
   * @param menu Menu to tag
   * @param sel  Current selection
   * @param act  Action: 0 untag, 1 tag, -1 toggle
   * @retval num Net change in number of tagged attachments
   */
  int (*tag)(struct Menu *menu, int sel, int act);

  /**
   * @defgroup menu_color color()
   * @ingroup menu_api
   *
   * color - Calculate the colour for a line of the menu
   * @param menu Menu containing items
   * @param line Menu line number
   * @retval >0 Colour pair in an integer
   * @retval  0 No colour
   */
  struct AttrColor *(*color)(struct Menu *menu, int line);

  /**
   * @defgroup menu_custom_redraw custom_redraw()
   * @ingroup menu_api
   *
   * custom_redraw - Redraw the menu
   * @param menu Menu to redraw
   */
  void (*custom_redraw)(struct Menu *menu);

  struct Notify *notify;  ///< Notifications

  void *mdata;            ///< Private data

  /**
   * @defgroup menu_mdata_free mdata_free()
   * @ingroup menu_api
   *
   * mdata_free - Free the private data attached to the Menu
   * @param menu Menu
   * @param ptr Menu data to free
   *
   * **Contract**
   * - @a menu is not NULL
   * - @a ptr  is not NULL
   * - @a *ptr is not NULL
   */
  void (*mdata_free)(struct Menu *menu, void **ptr);
};

// Simple movement
MenuRedrawFlags menu_bottom_page   (struct Menu *menu);
MenuRedrawFlags menu_current_bottom(struct Menu *menu);
MenuRedrawFlags menu_current_middle(struct Menu *menu);
MenuRedrawFlags menu_current_top   (struct Menu *menu);
MenuRedrawFlags menu_first_entry   (struct Menu *menu);
MenuRedrawFlags menu_half_down     (struct Menu *menu);
MenuRedrawFlags menu_half_up       (struct Menu *menu);
MenuRedrawFlags menu_last_entry    (struct Menu *menu);
MenuRedrawFlags menu_middle_page   (struct Menu *menu);
MenuRedrawFlags menu_next_entry    (struct Menu *menu);
MenuRedrawFlags menu_next_line     (struct Menu *menu);
MenuRedrawFlags menu_next_page     (struct Menu *menu);
MenuRedrawFlags menu_prev_entry    (struct Menu *menu);
MenuRedrawFlags menu_prev_line     (struct Menu *menu);
MenuRedrawFlags menu_prev_page     (struct Menu *menu);
MenuRedrawFlags menu_top_page      (struct Menu *menu);

void         menu_redraw_current(struct Menu *menu);
void         menu_redraw_full   (struct Menu *menu);
void         menu_redraw_index  (struct Menu *menu);
void         menu_redraw_motion (struct Menu *menu);
int          menu_redraw        (struct Menu *menu);

void         menu_add_dialog_row(struct Menu *menu, const char *row);
void         menu_cleanup(void);
enum MenuType menu_get_current_type(void);
void         menu_init(void);
int          menu_loop(struct Menu *menu);

struct MuttWindow *menu_new_window(enum MenuType type, struct ConfigSubset *sub);

int  menu_get_index(struct Menu *menu);
MenuRedrawFlags menu_set_index(struct Menu *menu, int index);
MenuRedrawFlags menu_move_selection(struct Menu *menu, int index);
void menu_queue_redraw(struct Menu *menu, MenuRedrawFlags redraw);
MenuRedrawFlags menu_move_view_relative(struct Menu *menu, int relative);
MenuRedrawFlags menu_set_and_notify(struct Menu *menu, int top, int index);
void menu_adjust(struct Menu *menu);

int menu_function_dispatcher(struct MuttWindow *win, int op);

#endif /* MUTT_MENU_LIB_H */
