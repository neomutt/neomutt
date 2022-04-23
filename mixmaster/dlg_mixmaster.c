/**
 * @file
 * Mixmaster Remailer Dialog
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
 * @page mixmaster_dlg_mixmaster Mixmaster Remailer Dialog
 *
 * The Mixmaster Remailer Dialog lets the user edit anonymous remailer chain.
 *
 * ## Windows
 *
 * | Name                      | Type            | See Also        |
 * | :------------------------ | :-------------- | :-------------- |
 * | Mixmaster Remailer Dialog | WT_DLG_REMAILER | dlg_mixmaster() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - Hosts:        @ref mixmaster_win_hosts
 * - Chain Bar:    @ref gui_sbar
 * - Chain:        @ref mixmaster_win_chain
 * - Remailer Bar: @ref gui_sbar
 *
 * ## Data
 * - #Remailer
 *
 * The Mixmaster Remailer Dialog stores its data (#Remailer) in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                    |
 * | :---------- | :------------------------- |
 * | #NT_CONFIG  | remailer_config_observer() |
 * | #NT_WINDOW  | remailer_window_observer() |
 *
 * The Mixmaster Remailer Dialog does not implement MuttWindow::recalc() or MuttWindow::repaint().
 *
 * Some other events are handled by the dialog's children.
 */

#include "config.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "menu/lib.h"
#include "functions.h"
#include "keymap.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "opcodes.h"
#include "private_data.h"
#include "remailer.h"

/// Help Bar for the Mixmaster dialog
static const struct Mapping RemailerHelp[] = {
  // clang-format off
  { N_("Append"), OP_MIX_APPEND },
  { N_("Insert"), OP_MIX_INSERT },
  { N_("Delete"), OP_MIX_DELETE },
  { N_("Abort"),  OP_EXIT },
  { N_("OK"),     OP_MIX_USE },
  { NULL, 0 },
  // clang-format on
};

/**
 * remailer_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int remailer_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  if (!mutt_str_equal(ev_c->name, "status_on_top"))
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  window_status_on_top(dlg, NeoMutt->sub);
  mutt_debug(LL_DEBUG5, "config done\n");
  return 0;
}

/**
 * remailer_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int remailer_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != dlg)
    return 0;

  notify_observer_remove(NeoMutt->notify, remailer_config_observer, dlg);
  notify_observer_remove(dlg->notify, remailer_window_observer, dlg);
  mutt_debug(LL_DEBUG5, "window delete done\n");

  return 0;
}

/**
 * mix_screen_coordinates - Get the screen coordinates to place a chain
 * @param[in]  win        Window
 * @param[out] type2_list Remailer List
 * @param[out] coordsp    On screen coordinates
 * @param[in]  chain      Chain
 * @param[in]  i          Index in chain
 */
void mix_screen_coordinates(struct MuttWindow *win, struct Remailer **type2_list,
                            struct Coord **coordsp, struct MixChain *chain, int i)
{
  const int wrap_indent = 2;

  if (!chain->cl)
    return;

  short c, r;

  mutt_mem_realloc(coordsp, sizeof(struct Coord) * chain->cl);

  struct Coord *coords = *coordsp;

  if (i == 0)
  {
    r = 0;
    c = 0;
  }
  else
  {
    c = coords[i - 1].c + strlen(type2_list[chain->ch[i - 1]]->shortname) + 2;
    r = coords[i - 1].r;
  }

  for (; i < chain->cl; i++)
  {
    short oc = c;
    c += strlen(type2_list[chain->ch[i]]->shortname) + 2;

    if (c >= win->state.cols)
    {
      oc = wrap_indent;
      c = wrap_indent;
      r++;
    }

    coords[i].c = oc;
    coords[i].r = r;
  }
}

/**
 * mix_redraw_ce - Redraw the Remailer chain
 * @param[in]  win        Window
 * @param[out] type2_list Remailer List
 * @param[in]  coords     Screen Coordinates
 * @param[in]  chain      Chain
 * @param[in]  i          Index in chain
 * @param[in]  selected   true, if this item is selected
 */
static void mix_redraw_ce(struct MuttWindow *win, struct Remailer **type2_list,
                          struct Coord *coords, struct MixChain *chain, int i, bool selected)
{
  if (!coords || !chain)
    return;

  if (coords[i].r < win->state.rows)
  {
    if (selected)
      mutt_curses_set_color_by_id(MT_COLOR_INDICATOR);
    else
      mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

    mutt_window_mvaddstr(win, coords[i].c, coords[i].r, type2_list[chain->ch[i]]->shortname);
    mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

    if (i + 1 < chain->cl)
      mutt_window_addstr(win, ", ");
  }
}

/**
 * mix_redraw_chain - Redraw the chain on screen
 * @param[in]  win        Window
 * @param[out] type2_list Remailer List
 * @param[in]  coords     Where to draw the list on screen
 * @param[in]  chain      Chain to display
 * @param[in]  cur        Chain index of current selection
 */
void mix_redraw_chain(struct MuttWindow *win, struct Remailer **type2_list,
                      struct Coord *coords, struct MixChain *chain, int cur)
{
  for (int i = 0; i < win->state.rows; i++)
  {
    mutt_window_move(win, 0, i);
    mutt_window_clrtoeol(win);
  }

  for (int i = 0; i < chain->cl; i++)
    mix_redraw_ce(win, type2_list, coords, chain, i, i == cur);
}

/**
 * mix_redraw_head - Redraw the Chain info
 * @param win   Window
 * @param chain Chain
 */
void mix_redraw_head(struct MuttWindow *win, struct MixChain *chain)
{
  char buf[1024];
  snprintf(buf, sizeof(buf), "-- Remailer chain [Length: %zu]", chain ? chain->cl : 0);
  sbar_set_title(win, buf);
}

/**
 * mix_format_caps - Turn flags into a MixMaster capability string
 * @param r Remailer to use
 * @retval ptr Capability string
 *
 * @note The string is a static buffer
 */
static const char *mix_format_caps(struct Remailer *r)
{
  static char capbuf[10];
  char *t = capbuf;

  if (r->caps & MIX_CAP_COMPRESS)
    *t++ = 'C';
  else
    *t++ = ' ';

  if (r->caps & MIX_CAP_MIDDLEMAN)
    *t++ = 'M';
  else
    *t++ = ' ';

  if (r->caps & MIX_CAP_NEWSPOST)
  {
    *t++ = 'N';
    *t++ = 'p';
  }
  else
  {
    *t++ = ' ';
    *t++ = ' ';
  }

  if (r->caps & MIX_CAP_NEWSMAIL)
  {
    *t++ = 'N';
    *t++ = 'm';
  }
  else
  {
    *t++ = ' ';
    *t++ = ' ';
  }

  *t = '\0';

  return capbuf;
}

/**
 * mix_format_str - Format a string for the remailer menu - Implements ::format_t - @ingroup expando_api
 *
 * | Expando | Description
 * | :------ | :-------------------------------------------------------
 * | \%a     | The remailer's e-mail address
 * | \%c     | Remailer capabilities
 * | \%n     | The running number on the menu
 * | \%s     | The remailer's short name
 */
static const char *mix_format_str(char *buf, size_t buflen, size_t col, int cols,
                                  char op, const char *src, const char *prec,
                                  const char *if_str, const char *else_str,
                                  intptr_t data, MuttFormatFlags flags)
{
  char fmt[128];
  struct Remailer *remailer = (struct Remailer *) data;
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'a':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(remailer->addr));
      }
      else if (!remailer->addr)
        optional = false;
      break;

    case 'c':
      if (optional)
        break;

      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, mix_format_caps(remailer));
      break;

    case 'n':
      if (optional)
        break;

      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, remailer->num);
      break;

    case 's':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(remailer->shortname));
      }
      else if (!remailer->shortname)
        optional = false;
      break;

    default:
      *buf = '\0';
  }

  if (optional)
  {
    mutt_expando_format(buf, buflen, col, cols, if_str, mix_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str, mix_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  }

  /* We return the format string, unchanged */
  return src;
}

/**
 * mix_make_entry - Format a menu item for the mixmaster chain list - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $mix_entry_format, mix_format_str()
 */
static void mix_make_entry(struct Menu *menu, char *buf, size_t buflen, int num)
{
  struct Remailer **type2_list = menu->mdata;
  const char *const c_mix_entry_format = cs_subset_string(NeoMutt->sub, "mix_entry_format");
  mutt_expando_format(buf, buflen, 0, menu->win->state.cols,
                      NONULL(c_mix_entry_format), mix_format_str,
                      (intptr_t) type2_list[num], MUTT_FORMAT_ARROWCURSOR);
}

/**
 * mix_chain_add - Add a host to the chain
 * @param[in]  chain      Chain to add to
 * @param[in]  s          Hostname
 * @param[out] type2_list Remailer List
 * @retval  0 Success
 * @retval -1 Error
 */
static int mix_chain_add(struct MixChain *chain, const char *s, struct Remailer **type2_list)
{
  int i;

  if (chain->cl >= MAX_MIXES)
    return -1;

  if (mutt_str_equal(s, "0") || mutt_istr_equal(s, "<random>"))
  {
    chain->ch[chain->cl++] = 0;
    return 0;
  }

  for (i = 0; type2_list[i]; i++)
  {
    if (mutt_istr_equal(s, type2_list[i]->shortname))
    {
      chain->ch[chain->cl++] = i;
      return 0;
    }
  }

  /* replace unknown remailers by <random> */

  if (!type2_list[i])
    chain->ch[chain->cl++] = 0;

  return 0;
}

/**
 * dlg_mixmaster - Create a Mixmaster chain
 * @param chainhead List of chain links
 *
 * Ask the user to select Mixmaster hosts to create a chain.
 */
void dlg_mixmaster(struct ListHead *chainhead)
{
  struct MixmasterPrivateData priv = { 0 };
  size_t ttll = 0;

  priv.c_redraw = true;

  priv.type2_list = remailer_get_hosts(&ttll);
  if (!priv.type2_list)
  {
    mutt_error(_("Can't get mixmaster's type2.list"));
    return;
  }

  priv.chain = mutt_mem_calloc(1, sizeof(struct MixChain));

  struct ListNode *p = NULL;
  STAILQ_FOREACH(p, chainhead, entries)
  {
    mix_chain_add(priv.chain, p->data, priv.type2_list);
  }
  mutt_list_free(chainhead);

  /* safety check */
  for (int i = 0; i < priv.chain->cl; i++)
  {
    if (priv.chain->ch[i] >= ttll)
      priv.chain->ch[i] = 0;
  }

  struct MuttWindow *dlg = mutt_window_new(WT_DLG_REMAILER, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);
  dlg->help_menu = MENU_MIX;
  dlg->help_data = RemailerHelp;
  dlg->wdata = &priv;

  struct MuttWindow *win_hosts = menu_new_window(MENU_MIX, NeoMutt->sub);
  win_hosts->focus = win_hosts;

  priv.win_chain = mutt_window_new(WT_CUSTOM, MUTT_WIN_ORIENT_VERTICAL,
                                   MUTT_WIN_SIZE_FIXED, MUTT_WIN_SIZE_UNLIMITED, 4);

  priv.win_cbar = sbar_new();
  struct MuttWindow *win_rbar = sbar_new();

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if (c_status_on_top)
  {
    mutt_window_add_child(dlg, win_rbar);
    mutt_window_add_child(dlg, win_hosts);
    mutt_window_add_child(dlg, priv.win_cbar);
    mutt_window_add_child(dlg, priv.win_chain);
  }
  else
  {
    mutt_window_add_child(dlg, win_hosts);
    mutt_window_add_child(dlg, priv.win_cbar);
    mutt_window_add_child(dlg, priv.win_chain);
    mutt_window_add_child(dlg, win_rbar);
  }
  sbar_set_title(win_rbar, _("Select a remailer chain"));

  mix_screen_coordinates(dlg, priv.type2_list, &priv.coords, priv.chain, 0);

  priv.menu = win_hosts->wdata;
  priv.menu->max = ttll;
  priv.menu->make_entry = mix_make_entry;
  priv.menu->tag = NULL;
  priv.menu->mdata = priv.type2_list;
  priv.menu->mdata_free = NULL; // Menu doesn't own the data

  notify_observer_add(NeoMutt->notify, NT_CONFIG, remailer_config_observer, dlg);
  notify_observer_add(dlg->notify, NT_WINDOW, remailer_window_observer, dlg);
  dialog_push(dlg);

  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  int rc;
  do
  {
    if (priv.c_redraw)
    {
      mix_redraw_head(priv.win_cbar, priv.chain);
      mix_redraw_chain(priv.win_chain, priv.type2_list, priv.coords, priv.chain, priv.c_cur);
      priv.c_redraw = false;
    }
    else if (priv.c_cur != priv.c_old)
    {
      mix_redraw_ce(priv.win_chain, priv.type2_list, priv.coords, priv.chain,
                    priv.c_old, false);
      mix_redraw_ce(priv.win_chain, priv.type2_list, priv.coords, priv.chain,
                    priv.c_cur, true);
    }

    priv.c_old = priv.c_cur;

    rc = FR_UNKNOWN;
    menu_tagging_dispatcher(priv.menu->win, op);
    window_redraw(NULL);

    op = km_dokey(priv.menu->type);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(priv.menu->type);
      continue;
    }
    mutt_clear_error();

    rc = mix_function_dispatcher(dlg, op);

    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(priv.menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(priv.menu->win, op);
  } while (rc != FR_DONE);
  // ---------------------------------------------------------------------------

  dialog_pop();
  mutt_window_free(&dlg);

  /* construct the remailer list */

  if (priv.chain->cl)
  {
    char *t = NULL;
    for (int i = 0; i < priv.chain->cl; i++)
    {
      const int j = priv.chain->ch[i];
      if (j != 0)
        t = priv.type2_list[j]->shortname;
      else
        t = "*";

      mutt_list_insert_tail(chainhead, mutt_str_dup(t));
    }
  }

  remailer_clear_hosts(&priv.type2_list);
  FREE(&priv.coords);
  FREE(&priv.chain);
}
