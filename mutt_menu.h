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

#ifndef MUTT_MENU_H
#define MUTT_MENU_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "keymap.h"

typedef uint16_t MuttRedrawFlags;      ///< Flags, e.g. #REDRAW_INDEX
#define REDRAW_NO_FLAGS             0  ///< No flags are set
#define REDRAW_INDEX          (1 << 0) ///< Redraw the index
#define REDRAW_MOTION         (1 << 1) ///< Redraw after moving the menu list
#define REDRAW_CURRENT        (1 << 2) ///< Redraw the current line of the menu
#define REDRAW_STATUS         (1 << 3) ///< Redraw the status bar
#define REDRAW_FULL           (1 << 4) ///< Redraw everything
#define REDRAW_BODY           (1 << 5) ///< Redraw the pager
#define REDRAW_FLOW           (1 << 6) ///< Used by pager to reflow text

/**
 * struct Menu - GUI selectable list of items
 */
struct Menu
{
  const char *title;      ///< Title of this menu
  int current;            ///< Current entry
  int max;                ///< Number of entries in the menu
  MuttRedrawFlags redraw; ///< When to redraw the screen
  enum MenuType type;     ///< Menu definition for keymap entries
  int pagelen;            ///< Number of entries per screen
  bool tagprefix : 1;
  bool is_mailbox_list : 1;
  struct MuttWindow *win_index;
  struct MuttWindow *win_ibar;

  /* Setting a non-empty dialog overrides normal menu behavior.
   * In dialog mode menubar is hidden and prompt keys are checked before
   * normal menu movement keys. This can cause problems with scrolling, if
   * prompt keys override movement keys.  */
  ARRAY_HEAD(,char*) dialog; ///< Dialog lines themselves
  char *prompt;           ///< Prompt for user, similar to mutt_multi_choice
  char *keys;             ///< Keys used in the prompt

  /* the following are used only by menu_loop() */
  int top;                ///< Entry that is the top of the current page
  int oldcurrent;         ///< For driver use only
  int search_dir;         ///< Direction of search
  int tagged;             ///< Number of tagged entries
  bool custom_search : 1; ///< The menu implements its own non-Menu::search()-compatible search, trickle OP_SEARCH*

  /**
   * make_entry - Format a item for a menu
   * @param[in]  menu   Menu containing items
   * @param[out] buf    Buffer in which to save string
   * @param[in]  buflen Buffer length
   * @param[in]  line   Menu line number
   */
  void (*make_entry)(struct Menu *menu, char *buf, size_t buflen, int line);

  /**
   * search - Search a menu for a item matching a regex
   * @param menu Menu to search
   * @param rx   Regex to match
   * @param line Menu entry to match
   * @retval  0 Success
   * @retval >0 Error, e.g. REG_NOMATCH
   */
  int (*search)(struct Menu *menu, regex_t *rx, int line);

  /**
   * tag - Tag some menu items
   * @param menu Menu to tag
   * @param sel  Current selection
   * @param act  Action: 0 untag, 1 tag, -1 toggle
   * @retval num Net change in number of tagged attachments
   */
  int (*tag)(struct Menu *menu, int sel, int act);

  /**
   * color - Calculate the colour for a line of the menu
   * @param menu Menu containing items
   * @param line Menu line number
   * @retval >0 Colour pair in an integer
   * @retval  0 No colour
   */
  int (*color)(struct Menu *menu, int line);

  /**
   * custom_redraw - Redraw the menu
   * @param menu Menu to redraw
   */
  void (*custom_redraw)(struct Menu *menu);

  struct Notify *notify;  ///< Notifications

  void *mdata;            ///< Private data

  /**
   * mdata_free - Free the private data attached to the Menu
   * @param menu Menu
   * @param ptr Menu data to free
   */
  void (*mdata_free)(struct Menu *menu, void **ptr);
};

void         menu_bottom_page(struct Menu *menu);
void         menu_check_recenter(struct Menu *menu);
void         menu_current_bottom(struct Menu *menu);
void         menu_current_middle(struct Menu *menu);
void         menu_current_top(struct Menu *menu);
void         menu_first_entry(struct Menu *menu);
void         menu_half_down(struct Menu *menu);
void         menu_half_up(struct Menu *menu);
void         menu_last_entry(struct Menu *menu);
void         menu_middle_page(struct Menu *menu);
void         menu_next_line(struct Menu *menu);
void         menu_next_page(struct Menu *menu);
void         menu_prev_line(struct Menu *menu);
void         menu_prev_page(struct Menu *menu);
void         menu_redraw_current(struct Menu *menu);
void         menu_redraw_full(struct Menu *menu);
void         menu_redraw_index(struct Menu *menu);
void         menu_redraw_motion(struct Menu *menu);
void         menu_redraw_status(struct Menu *menu);
int          menu_redraw(struct Menu *menu);
void         menu_top_page(struct Menu *menu);

void         menu_add_dialog_row(struct Menu *menu, const char *row);
void         menu_current_redraw(void);
void         menu_free(struct Menu **ptr);
enum MenuType menu_get_current_type(void);
void         menu_init(void);
int          menu_loop(struct Menu *menu);
struct Menu *menu_new(struct MuttWindow *win, enum MenuType type);
void         menu_pop_current(struct Menu *menu);
void         menu_push_current(struct Menu *menu);
void         menu_set_current_redraw_full(void);
void         menu_set_current_redraw(MuttRedrawFlags redraw);
void         menu_set_redraw_full(enum MenuType menu);
void         menu_set_redraw(enum MenuType menu, MuttRedrawFlags redraw);

#endif /* MUTT_MENU_H */
