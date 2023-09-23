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
 * | Name                      | Type             | See Also        |
 * | :------------------------ | :--------------- | :-------------- |
 * | Mixmaster Remailer Dialog | WT_DLG_MIXMASTER | dlg_mixmaster() |
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
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "functions.h"
#include "mutt_logging.h"
#include "private_data.h"
#include "remailer.h"
#include "win_chain.h"
#include "win_hosts.h"

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
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
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
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != dlg)
    return 0;

  notify_observer_remove(NeoMutt->sub->notify, remailer_config_observer, dlg);
  notify_observer_remove(dlg->notify, remailer_window_observer, dlg);
  mutt_debug(LL_DEBUG5, "window delete done\n");

  return 0;
}

/**
 * mix_dlg_new - Create a new Mixmaster Remailer Dialog
 * @param priv Mixmaster private data
 * @param ra   Array of all Remailer hosts
 * @retval ptr New Mixmaster Remailer Dialog
 */
static struct MuttWindow *mix_dlg_new(struct MixmasterPrivateData *priv,
                                      struct RemailerArray *ra)
{
  struct MuttWindow *dlg = mutt_window_new(WT_DLG_MIXMASTER, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);
  dlg->help_menu = MENU_MIXMASTER;
  dlg->help_data = RemailerHelp;
  dlg->wdata = priv;

  priv->win_hosts = win_hosts_new(ra);
  struct MuttWindow *win_cbar = sbar_new();
  priv->win_chain = win_chain_new(win_cbar);

  struct MuttWindow *win_rbar = sbar_new();
  sbar_set_title(win_rbar, _("Select a remailer chain"));

  const bool c_status_on_top = cs_subset_bool(NeoMutt->sub, "status_on_top");
  if (c_status_on_top)
  {
    mutt_window_add_child(dlg, win_rbar);
    mutt_window_add_child(dlg, priv->win_hosts);
    mutt_window_add_child(dlg, win_cbar);
    mutt_window_add_child(dlg, priv->win_chain);
  }
  else
  {
    mutt_window_add_child(dlg, priv->win_hosts);
    mutt_window_add_child(dlg, win_cbar);
    mutt_window_add_child(dlg, priv->win_chain);
    mutt_window_add_child(dlg, win_rbar);
  }

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, remailer_config_observer, dlg);
  notify_observer_add(dlg->notify, NT_WINDOW, remailer_window_observer, dlg);

  return dlg;
}

/**
 * dlg_mixmaster - Create a Mixmaster chain - @ingroup gui_dlg
 * @param chainhead List of chain links
 *
 * The Mixmaster Dialogs allows the user to create a chain of anonymous
 * remailers.  The user can add/delete/reorder the hostss.
 */
void dlg_mixmaster(struct ListHead *chainhead)
{
  struct MixmasterPrivateData priv = { 0 };

  struct RemailerArray ra = remailer_get_hosts();
  if (ARRAY_EMPTY(&ra))
  {
    mutt_error(_("Can't get mixmaster's type2.list"));
    return;
  }

  struct MuttWindow *dlg = mix_dlg_new(&priv, &ra);

  win_chain_init(priv.win_chain, chainhead, &ra);
  mutt_list_free(chainhead);

  dialog_push(dlg);
  struct MuttWindow *old_focus = window_set_focus(priv.win_hosts);

  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  int rc = FR_UNKNOWN;
  do
  {
    menu_tagging_dispatcher(priv.win_hosts, op);
    window_redraw(NULL);

    op = km_dokey(MENU_MIXMASTER, GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(MENU_MIXMASTER);
      continue;
    }
    mutt_clear_error();

    rc = mix_function_dispatcher(dlg, op);

    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(priv.win_hosts, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while ((rc != FR_DONE) && (rc != FR_NO_ACTION));
  // ---------------------------------------------------------------------------

  /* construct the remailer list */
  if (rc == FR_DONE)
    win_chain_extract(priv.win_chain, chainhead);

  window_set_focus(old_focus);
  dialog_pop();
  mutt_window_free(&dlg);

  remailer_clear_hosts(&ra);
}
