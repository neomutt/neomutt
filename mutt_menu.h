/**
 * @file
 * GUI handling of selectable lists
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

#ifndef _MUTT_MENU_H
#define _MUTT_MENU_H

#include <regex.h>
#include <stdbool.h>
#include <stdio.h>

#define REDRAW_INDEX          (1 << 0)
#define REDRAW_MOTION         (1 << 1)
#define REDRAW_MOTION_RESYNCH (1 << 2)
#define REDRAW_CURRENT        (1 << 3)
#define REDRAW_STATUS         (1 << 4)
#define REDRAW_FULL           (1 << 5)
#define REDRAW_BODY           (1 << 6)
#define REDRAW_FLOW           (1 << 7) /* Used by pager to reflow text */
#ifdef USE_SIDEBAR
#define REDRAW_SIDEBAR        (1 << 8)
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
  int redraw;  /**< when to redraw the screen */
  int menu;    /**< menu definition for keymap entries. */
  int offset;  /**< row offset within the window to start the index */
  int pagelen; /**< number of entries per screen */
  bool tagprefix : 1;
  int is_mailbox_list;
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

  /* callback to generate an index line for the requested element */
  void (*make_entry)(char *buf, size_t buflen, struct Menu *menu, int num);

  /* how to search the menu */
  int (*search)(struct Menu *, regex_t *re, int n);

  int (*tag)(struct Menu *, int i, int m);

  /* these are used for custom redrawing callbacks */
  void (*custom_menu_redraw)(struct Menu *);
  void *redraw_data;

  /* color pair to be used for the requested element
   * (default function returns ColorDefs[MT_COLOR_NORMAL])
   */
  int (*color)(int i);

  /* the following are used only by mutt_menu_loop() */
  int top;        /**< entry that is the top of the current page */
  int oldcurrent; /**< for driver use only. */
  int search_dir;  /**< direction of search */
  int tagged;     /**< number of tagged entries */
};

void mutt_menu_init(void);
void menu_redraw_full(struct Menu *menu);
#ifdef USE_SIDEBAR
void menu_redraw_sidebar(struct Menu *menu);
#endif
void menu_redraw_index(struct Menu *menu);
void menu_redraw_status(struct Menu *menu);
void menu_redraw_motion(struct Menu *menu);
void menu_redraw_current(struct Menu *menu);
int menu_redraw(struct Menu *menu);
void menu_first_entry(struct Menu *menu);
void menu_last_entry(struct Menu *menu);
void menu_top_page(struct Menu *menu);
void menu_bottom_page(struct Menu *menu);
void menu_middle_page(struct Menu *menu);
void menu_next_page(struct Menu *menu);
void menu_prev_page(struct Menu *menu);
void menu_next_line(struct Menu *menu);
void menu_prev_line(struct Menu *menu);
void menu_half_up(struct Menu *menu);
void menu_half_down(struct Menu *menu);
void menu_current_top(struct Menu *menu);
void menu_current_middle(struct Menu *menu);
void menu_current_bottom(struct Menu *menu);
void menu_check_recenter(struct Menu *menu);
void menu_status_line(char *buf, size_t buflen, struct Menu *menu, const char *p);
bool mutt_ts_capability(void);
void mutt_ts_status(char *str);
void mutt_ts_icon(char *str);

struct Menu *mutt_new_menu(int menu);
void mutt_menu_destroy(struct Menu **p);
void mutt_push_current_menu(struct Menu *menu);
void mutt_pop_current_menu(struct Menu *menu);
void mutt_set_current_menu_redraw(int redraw);
void mutt_set_current_menu_redraw_full(void);
void mutt_set_menu_redraw(int menu_type, int redraw);
void mutt_set_menu_redraw_full(int menu_type);
void mutt_current_menu_redraw(void);
int mutt_menu_loop(struct Menu *menu);

/* used in both the index and pager index to make an entry. */
void index_make_entry(char *s, size_t l, struct Menu *menu, int num);
int index_color(int index_no);

#endif /* _MUTT_MENU_H */
