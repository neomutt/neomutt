/**
 * @file
 * Mixmaster Chain Window
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page mixmaster_win_chain Mixmaster Chain Window
 *
 * Display a menu of Remailer Hosts for the user to select.
 *
 * ## Windows
 *
 * | Name                   | Type      | See Also        |
 * | :--------------------- | :-------- | :-------------- |
 * | Mixmaster Chain Window | WT_CUSTOM | win_chain_new() |
 *
 * **Parent**
 * - @ref mixmaster_dlg_mixmaster
 *
 * **Children**
 * - None
 *
 * ## Data
 * - #ChainData
 *
 * The Chain Window stores its data (#ChainData) in MuttWindow::wdata.
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "win_chain.h"
#include "color/lib.h"
#include "chain_data.h"
#include "remailer.h"

/**
 * cbar_update - Update the Chain bar (status bar)
 * @param cd Chain data
 */
static void cbar_update(struct ChainData *cd)
{
  char buf[1024] = { 0 };
  snprintf(buf, sizeof(buf), _("-- Remailer chain [Length: %d]"), cd->chain_len);
  sbar_set_title(cd->win_cbar, buf);
}

/**
 * chain_add - Add a host to the chain
 * @param cd Chain data
 * @param s  Hostname
 * @param ra Remailer List
 * @retval  0 Success
 * @retval -1 Error
 */
static int chain_add(struct ChainData *cd, const char *s, struct RemailerArray *ra)
{
  if (cd->chain_len >= MAX_MIXES)
    return -1;

  if (mutt_str_equal(s, "0") || mutt_istr_equal(s, "<random>"))
  {
    cd->chain[cd->chain_len++] = 0;
    return 0;
  }

  struct Remailer **r = NULL;
  ARRAY_FOREACH(r, ra)
  {
    if (mutt_istr_equal(s, (*r)->shortname))
    {
      cd->chain[cd->chain_len++] = ARRAY_FOREACH_IDX;
      return 0;
    }
  }

  /* replace unknown remailers by <random> */
  cd->chain[cd->chain_len++] = 0;
  return 0;
}

/**
 * win_chain_recalc - Recalculate the Chain list - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int win_chain_recalc(struct MuttWindow *win)
{
  const int wrap_indent = 2;

  if (!win || !win->wdata)
    return 0;

  struct ChainData *cd = win->wdata;
  cbar_update(cd);

  win->actions |= WA_REPAINT;
  if (cd->chain_len == 0)
    return 0;

  short col = 0, row = 0;
  for (int i = 0; i < cd->chain_len; i++)
  {
    short old_col = col;
    int index = cd->chain[i];
    struct Remailer **rp = ARRAY_GET(cd->ra, index);
    col += strlen((*rp)->shortname) + 2;

    if (col >= win->state.cols)
    {
      old_col = wrap_indent;
      col = wrap_indent;
      row++;
    }

    cd->coords[i].col = old_col;
    cd->coords[i].row = row;
  }

  return 0;
}

/**
 * win_chain_repaint - Repaint the Chain list - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int win_chain_repaint(struct MuttWindow *win)
{
  for (int i = 0; i < win->state.rows; i++)
  {
    mutt_window_move(win, 0, i);
    mutt_window_clrtoeol(win);
  }

  struct ChainData *cd = win->wdata;

  if (cd->chain_len == 0)
    return 0;

  for (int i = 0; i < cd->chain_len; i++)
  {
    if (cd->coords[i].row < win->state.rows)
    {
      if (i == cd->sel)
        mutt_curses_set_color_by_id(MT_COLOR_INDICATOR);
      else
        mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

      int index = cd->chain[i];
      struct Remailer **rp = ARRAY_GET(cd->ra, index);
      mutt_window_mvaddstr(win, cd->coords[i].col, cd->coords[i].row, (*rp)->shortname);
      mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

      if ((i + 1) < cd->chain_len)
        mutt_window_addstr(win, ", ");
    }
  }
  return 0;
}

/**
 * win_chain_new - Create a new Chain list Window
 * @param win_cbar Chain bar to keep updated (status bar)
 * @retval ptr New Chain list Window
 */
struct MuttWindow *win_chain_new(struct MuttWindow *win_cbar)
{
  struct MuttWindow *win = mutt_window_new(WT_CUSTOM, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                           MUTT_WIN_SIZE_UNLIMITED, 4);

  struct ChainData *cd = chain_data_new();
  cd->win_cbar = win_cbar;

  win->recalc = win_chain_recalc;
  win->repaint = win_chain_repaint;
  win->wdata = cd;
  win->wdata_free = chain_data_free;
  win->actions |= WA_RECALC;

  return win;
}

/**
 * win_chain_init - Initialise the Chain list Window
 * @param win   Chain list Window
 * @param chain Chain data
 * @param ra    Array of all Remailer hosts
 */
void win_chain_init(struct MuttWindow *win, struct ListHead *chain, struct RemailerArray *ra)
{
  if (!win || !win->wdata)
    return;

  struct ChainData *cd = win->wdata;
  cd->ra = ra;
  cd->sel = 0;

  struct ListNode *p = NULL;
  STAILQ_FOREACH(p, chain, entries)
  {
    chain_add(cd, p->data, cd->ra);
  }
  if (cd->chain_len > 1)
    cd->sel = cd->chain_len - 1;
  win->actions |= WA_RECALC;
}

/**
 * win_chain_extract - Extract the Chain list data
 * @param win   Chain list Window
 * @param chain Chain data
 * @retval num Number of entries in the Chain
 * @retval  -1 Error
 */
int win_chain_extract(struct MuttWindow *win, struct ListHead *chain)
{
  if (!win || !win->wdata)
    return -1;

  struct ChainData *cd = win->wdata;

  if (cd->chain_len)
  {
    char *t = NULL;
    for (int i = 0; i < cd->chain_len; i++)
    {
      const int j = cd->chain[i];
      if (j != 0)
      {
        struct Remailer **rp = ARRAY_GET(cd->ra, j);
        t = (*rp)->shortname;
      }
      else
      {
        t = "*";
      }

      mutt_list_insert_tail(chain, mutt_str_dup(t));
    }
  }
  return cd->chain_len;
}

/**
 * win_chain_get_length - Get the number of Remailers in the Chain
 * @param win Chain list Window
 * @retval num Number of entries in the Chain
 */
int win_chain_get_length(struct MuttWindow *win)
{
  if (!win || !win->wdata)
    return 0;

  struct ChainData *cd = win->wdata;

  return cd->chain_len;
}

/**
 * win_chain_next - Select the next entry in the Chain list
 * @param win Chain list Window
 * @retval true Selection changed
 */
bool win_chain_next(struct MuttWindow *win)
{
  if (!win || !win->wdata)
    return false;

  struct ChainData *cd = win->wdata;

  if (cd->chain_len && (cd->sel < (cd->chain_len - 1)))
  {
    cd->sel++;
  }
  else
  {
    mutt_error(_("You already have the last chain element selected"));
    return false;
  }

  win->actions |= WA_REPAINT;
  return true;
}

/**
 * win_chain_prev - Select the previous entry in the Chain list
 * @param win Chain list Window
 * @retval true Selection changed
 */
bool win_chain_prev(struct MuttWindow *win)
{
  if (!win || !win->wdata)
    return false;

  struct ChainData *cd = win->wdata;

  if (cd->sel)
  {
    cd->sel--;
  }
  else
  {
    mutt_error(_("You already have the first chain element selected"));
    return false;
  }

  win->actions |= WA_REPAINT;
  return true;
}

/**
 * win_chain_append - Add an item to the Chain, after the current item
 * @param win Chain list Window
 * @param r   Selected Remailer host
 * @retval true Item added to Chain
 */
bool win_chain_append(struct MuttWindow *win, struct Remailer *r)
{
  if (!win || !win->wdata || !r)
    return false;

  struct ChainData *cd = win->wdata;

  if ((cd->chain_len < MAX_MIXES) && (cd->sel < cd->chain_len))
    cd->sel++;

  return win_chain_insert(win, r);
}

/**
 * win_chain_insert - Add an item to the Chain, before the current item
 * @param win Chain list Window
 * @param r   Selected Remailer host
 * @retval true Item added to Chain
 */
bool win_chain_insert(struct MuttWindow *win, struct Remailer *r)
{
  if (!win || !win->wdata || !r)
    return false;

  struct ChainData *cd = win->wdata;

  if (cd->chain_len < MAX_MIXES)
  {
    cd->chain_len++;
    for (int i = cd->chain_len - 1; i > cd->sel; i--)
      cd->chain[i] = cd->chain[i - 1];

    cd->chain[cd->sel] = r->num;
  }
  else
  {
    /* L10N The '%d' here hard-coded to 19 */
    mutt_error(_("Mixmaster chains are limited to %d elements"), MAX_MIXES);
    return false;
  }

  win->actions |= WA_RECALC;
  return true;
}

/**
 * win_chain_delete - Delete the current item from the Chain
 * @param win   Chain list Window
 * @retval true Item deleted
 */
bool win_chain_delete(struct MuttWindow *win)
{
  if (!win || !win->wdata)
    return false;

  struct ChainData *cd = win->wdata;

  if (cd->chain_len)
  {
    cd->chain_len--;

    for (int i = cd->sel; i < cd->chain_len; i++)
      cd->chain[i] = cd->chain[i + 1];

    if ((cd->sel == cd->chain_len) && cd->sel)
      cd->sel--;
  }
  else
  {
    mutt_error(_("The remailer chain is already empty"));
    return false;
  }

  win->actions |= WA_RECALC;
  return true;
}

/**
 * win_chain_validate - Validate the Chain
 * @param win   Chain list Window
 * @retval true Chain is valid
 */
bool win_chain_validate(struct MuttWindow *win)
{
  if (!win || !win->wdata)
    return false;

  struct ChainData *cd = win->wdata;
  if (cd->chain_len == 0)
    return false;

  int last_index = cd->chain[cd->chain_len - 1];
  if (last_index != 0)
  {
    struct Remailer **rp = ARRAY_GET(cd->ra, last_index);
    if ((*rp)->caps & MIX_CAP_MIDDLEMAN)
    {
      mutt_error(_("Error: %s can't be used as the final remailer of a chain"),
                 (*rp)->shortname);
      return false;
    }
  }

  return true;
}
