/**
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 *
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

/*
 * This file is named mutt_menu.h so it doesn't collide with ncurses menu.h
 */

#ifndef _MUTT_MENU_H
#define _MUTT_MENU_H 1

#include "keymap.h"
#include "mutt_curses.h"
#include "mutt_regex.h"

#define REDRAW_INDEX          (1 << 0)
#define REDRAW_MOTION         (1 << 1)
#define REDRAW_MOTION_RESYNCH (1 << 2)
#define REDRAW_CURRENT        (1 << 3)
#define REDRAW_STATUS         (1 << 4)
#define REDRAW_FULL           (1 << 5)
#define REDRAW_BODY           (1 << 6)
#define REDRAW_SIGWINCH       (1 << 7)
#ifdef USE_SIDEBAR
#define REDRAW_SIDEBAR        (1 << 8)
#endif

#define MUTT_MODEFMT "-- Mutt: %s"

typedef struct menu_t
{
  char *title; /* the title of this menu */
  char *help;  /* quickref for the current menu */
  void *data;  /* extra data for the current menu */
  int current; /* current entry */
  int max;     /* the number of entries in the menu */
  int redraw;  /* when to redraw the screen */
  int menu;    /* menu definition for keymap entries. */
  int offset;  /* row offset within the window to start the index */
  int pagelen; /* number of entries per screen */
  int tagprefix;
  int is_mailbox_list;
  mutt_window_t *indexwin;
  mutt_window_t *statuswin;
  mutt_window_t *helpwin;
  mutt_window_t *messagewin;

  /* Setting dialog != NULL overrides normal menu behavior.
   * In dialog mode menubar is hidden and prompt keys are checked before
   * normal menu movement keys. This can cause problems with scrolling, if
   * prompt keys override movement keys.
   */
  char **dialog; /* dialog lines themselves */
  char *prompt;  /* prompt for user, similar to mutt_multi_choice */
  char *keys;    /* keys used in the prompt */

  /* callback to generate an index line for the requested element */
  void (*make_entry)(char *, size_t, struct menu_t *, int);

  /* how to search the menu */
  int (*search)(struct menu_t *, regex_t *re, int n);

  int (*tag)(struct menu_t *, int i, int m);

  /* these are used for custom redrawing callbacks */
  void (*custom_menu_redraw)(struct menu_t *);
  void *redraw_data;

  /* color pair to be used for the requested element
   * (default function returns ColorDefs[MT_COLOR_NORMAL])
   */
  int (*color)(int i);

  /* the following are used only by mutt_menu_loop() */
  int top;        /* entry that is the top of the current page */
  int oldcurrent; /* for driver use only. */
  int searchDir;  /* direction of search */
  int tagged;     /* number of tagged entries */
} MUTTMENU;

void mutt_menu_init(void);
void menu_redraw_full(MUTTMENU *menu);
#ifdef USE_SIDEBAR
void menu_redraw_sidebar(MUTTMENU *menu);
#endif
void menu_redraw_index(MUTTMENU *menu);
void menu_redraw_status(MUTTMENU *menu);
void menu_redraw_motion(MUTTMENU *menu);
void menu_redraw_current(MUTTMENU *menu);
int menu_redraw(MUTTMENU *menu);
void menu_first_entry(MUTTMENU *menu);
void menu_last_entry(MUTTMENU *menu);
void menu_top_page(MUTTMENU *menu);
void menu_bottom_page(MUTTMENU *menu);
void menu_middle_page(MUTTMENU *menu);
void menu_next_page(MUTTMENU *menu);
void menu_prev_page(MUTTMENU *menu);
void menu_next_line(MUTTMENU *menu);
void menu_prev_line(MUTTMENU *menu);
void menu_half_up(MUTTMENU *menu);
void menu_half_down(MUTTMENU *menu);
void menu_current_top(MUTTMENU *menu);
void menu_current_middle(MUTTMENU *menu);
void menu_current_bottom(MUTTMENU *menu);
void menu_check_recenter(MUTTMENU *menu);
void menu_status_line(char *buf, size_t buflen, MUTTMENU *menu, const char *p);
bool mutt_ts_capability(void);
void mutt_ts_status(char *str);
void mutt_ts_icon(char *str);

MUTTMENU *mutt_new_menu(int menu);
void mutt_menu_destroy(MUTTMENU **p);
void mutt_push_current_menu(MUTTMENU *);
void mutt_pop_current_menu(MUTTMENU *);
void mutt_set_current_menu_redraw(int redraw);
void mutt_set_current_menu_redraw_full(void);
void mutt_set_menu_redraw_full(int);
void mutt_current_menu_redraw(void);
int mutt_menu_loop(MUTTMENU *menu);

/* used in both the index and pager index to make an entry. */
void index_make_entry(char *s, size_t l, MUTTMENU *menu, int num);
int index_color(int index_no);

bool mutt_limit_current_thread(HEADER *h);

#endif /* _MUTT_MENU_H */
