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

#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include "config/lib.h"
#include "keymap.h"

/* These Config Variables are only used in menu.c */
extern short C_MenuContext;
extern bool  C_MenuMoveOff;
extern bool  C_MenuScroll;

struct ConfigSet;
struct HashElem;

typedef uint16_t MuttRedrawFlags;      ///< Flags, e.g. #REDRAW_INDEX
#define REDRAW_NO_FLAGS             0  ///< No flags are set
#define REDRAW_INDEX          (1 << 0) ///< Redraw the index
#define REDRAW_MOTION         (1 << 1) ///< Redraw after moving the menu list
#define REDRAW_MOTION_RESYNC  (1 << 2) ///< Redraw any changing the menu selection
#define REDRAW_CURRENT        (1 << 3) ///< Redraw the current line of the menu
#define REDRAW_STATUS         (1 << 4) ///< Redraw the status bar
#define REDRAW_FULL           (1 << 5) ///< Redraw everything
#define REDRAW_BODY           (1 << 6) ///< Redraw the pager
#define REDRAW_FLOW           (1 << 7) ///< Used by pager to reflow text
#ifdef USE_SIDEBAR
#define REDRAW_SIDEBAR        (1 << 8) ///< Redraw the sidebar
#endif

#define MUTT_MODEFMT "-- NeoMutt: %s"

/**
 * struct Menu - GUI selectable list of items
 */
struct Menu
{
  char *title; /**< the title of this menu */
  char *help;  /**< quickref for the current menu */
  void *data;  /**< extra data for the current menu */
  int current; /**< current entry */
  int max;     /**< the number of entries in the menu */
  MuttRedrawFlags redraw;  /**< when to redraw the screen */
  int menu;    /**< menu definition for keymap entries. */
  int offset;  /**< row offset within the window to start the index */
  int pagelen; /**< number of entries per screen */
  bool tagprefix : 1;
  bool is_mailbox_list : 1;
  struct MuttWindow *indexwin;
  struct MuttWindow *statuswin;
  struct MuttWindow *helpwin;
  struct MuttWindow *messagewin;

  /* Setting dialog != NULL overrides normal menu behavior.
   * In dialog mode menubar is hidden and prompt keys are checked before
   * normal menu movement keys. This can cause problems with scrolling, if
   * prompt keys override movement keys.
   */
  char **dialog; /**< dialog lines themselves */
  char *prompt;  /**< prompt for user, similar to mutt_multi_choice */
  char *keys;    /**< keys used in the prompt */

  /* the following are used only by mutt_menu_loop() */
  int top;        /**< entry that is the top of the current page */
  int oldcurrent; /**< for driver use only */
  int search_dir; /**< direction of search */
  int tagged;     /**< number of tagged entries */

  /**
   * menu_make_entry - Format a item for a menu
   * @param[out] buf    Buffer in which to save string
   * @param[in]  buflen Buffer length
   * @param[in]  menu   Menu containing items
   * @param[in]  line   Menu line number
   */
  void (*menu_make_entry)   (char *buf, size_t buflen, struct Menu *menu, int line);
  /**
   * menu_search - Search a menu for a item matching a regex
   * @param menu Menu to search
   * @param rx   Regex to match
   * @param line Menu entry to match
   * @retval  0 Success
   * @retval >0 Error, e.g. REG_NOMATCH
   */
  int  (*menu_search)       (struct Menu *menu, regex_t *rx, int line);
  /**
   * menu_tag - Tag some menu items
   * @param menu Menu to tag
   * @param sel  Current selection
   * @param act  Action: 0 untag, 1 tag, -1 toggle
   * @retval num Net change in number of tagged attachments
   */
  int  (*menu_tag)          (struct Menu *menu, int sel, int act);
  /**
   * menu_color - Calculate the colour for a line of the menu
   * @param line Menu line number
   * @retval >0 Colour pair in an integer
   * @retval  0 No colour
   */
  int  (*menu_color)        (int line);
  /**
   * menu_custom_redraw - Redraw the menu
   * @param menu Menu to redraw
   */
  void (*menu_custom_redraw)(struct Menu *menu);
  void *redraw_data;
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
#ifdef USE_SIDEBAR
void         menu_redraw_sidebar(struct Menu *menu);
#endif
void         menu_redraw_status(struct Menu *menu);
int          menu_redraw(struct Menu *menu);
void         menu_top_page(struct Menu *menu);
void         mutt_menu_current_redraw(void);
void         mutt_menu_destroy(struct Menu **p);
void         mutt_menu_init(void);
int          mutt_menu_loop(struct Menu *menu);
struct Menu *mutt_menu_new(enum MenuType type);
void         mutt_menu_pop_current(struct Menu *menu);
void         mutt_menu_push_current(struct Menu *menu);
void         mutt_menu_set_current_redraw_full(void);
void         mutt_menu_set_current_redraw(MuttRedrawFlags redraw);
void         mutt_menu_set_redraw_full(int menu_type);
void         mutt_menu_set_redraw(int menu_type, MuttRedrawFlags redraw);

bool mutt_menu_listener(const struct ConfigSet *cs, struct HashElem *he, const char *name, enum ConfigEvent ev);

#endif /* MUTT_MENU_H */
