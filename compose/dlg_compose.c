/**
 * @file
 * Compose Email Dialog
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2007,2010,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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

/**
 * @page compose_dlg_compose Compose Email Dialog
 *
 * The Compose Email Dialog lets the user edit the fields before sending an email.
 * They can also add/remove/reorder attachments.
 *
 * ## Windows
 *
 * | Name                 | Type           | See Also      |
 * | :------------------- | :------------- | :------------ |
 * | Compose Email Dialog | WT_DLG_COMPOSE | dlg_compose() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - @ref envelope_window
 * - @ref gui_sbar
 * - @ref compose_attach
 * - @ref compose_cbar
 *
 * ## Data
 * - #ComposeSharedData
 *
 * The Compose Email Dialog stores its data (#ComposeSharedData) in
 * MuttWindow::wdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type  | Handler                   |
 * | :---------- | :------------------------ |
 * | #NT_CONFIG  | compose_config_observer() |
 * | #NT_WINDOW  | compose_window_observer() |
 *
 * The Compose Email Dialog does not implement MuttWindow::recalc() or MuttWindow::repaint().
 *
 * Some other events are handled by the dialog's children.
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "attach/lib.h"
#include "envelope/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "attach_data.h"
#include "cbar.h"
#include "functions.h"
#include "globals.h"
#include "hook.h"
#include "mutt_logging.h"
#include "shared_data.h"

/// Help Bar for the Compose dialog
static const struct Mapping ComposeHelp[] = {
  // clang-format off
  { N_("Send"),        OP_COMPOSE_SEND_MESSAGE },
  { N_("Abort"),       OP_EXIT },
  /* L10N: compose menu help line entry */
  { N_("To"),          OP_ENVELOPE_EDIT_TO },
  /* L10N: compose menu help line entry */
  { N_("CC"),          OP_ENVELOPE_EDIT_CC },
  /* L10N: compose menu help line entry */
  { N_("Subj"),        OP_ENVELOPE_EDIT_SUBJECT },
  { N_("Attach file"), OP_ATTACHMENT_ATTACH_FILE },
  { N_("Descrip"),     OP_ATTACHMENT_EDIT_DESCRIPTION },
  { N_("Help"),        OP_HELP },
  { NULL, 0 },
  // clang-format on
};

#ifdef USE_NNTP
/// Help Bar for the News Compose dialog
static const struct Mapping ComposeNewsHelp[] = {
  // clang-format off
  { N_("Send"),        OP_COMPOSE_SEND_MESSAGE },
  { N_("Abort"),       OP_EXIT },
  { N_("Newsgroups"),  OP_ENVELOPE_EDIT_NEWSGROUPS },
  { N_("Subj"),        OP_ENVELOPE_EDIT_SUBJECT },
  { N_("Attach file"), OP_ATTACHMENT_ATTACH_FILE },
  { N_("Descrip"),     OP_ATTACHMENT_EDIT_DESCRIPTION },
  { N_("Help"),        OP_HELP },
  { NULL, 0 },
  // clang-format on
};
#endif

/**
 * compose_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int compose_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  if (!mutt_str_equal(ev_c->name, "status_on_top"))
    return 0;

  window_status_on_top(dlg, NeoMutt->sub);
  mutt_debug(LL_DEBUG5, "config done, request WA_REFLOW\n");
  return 0;
}

/**
 * compose_email_observer - Notification that an Email has changed - Implements ::observer_t - @ingroup observer_api
 */
static int compose_email_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_ENVELOPE)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct ComposeSharedData *shared = nc->global_data;

  if (nc->event_subtype == NT_ENVELOPE_FCC)
    shared->fcc_set = true;

  mutt_message_hook(shared->mailbox, shared->email, MUTT_SEND2_HOOK);
  return 0;
}

/**
 * compose_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int compose_window_observer(struct NotifyCallback *nc)
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

  notify_observer_remove(NeoMutt->sub->notify, compose_config_observer, dlg);
  notify_observer_remove(dlg->notify, compose_window_observer, dlg);
  mutt_debug(LL_DEBUG5, "window delete done\n");

  return 0;
}

/**
 * gen_attach_list - Generate the attachment list for the compose screen
 * @param actx        Attachment context
 * @param b           Attachment
 * @param parent_type Attachment type, e.g #TYPE_MULTIPART
 * @param level       Nesting depth of attachment
 */
static void gen_attach_list(struct AttachCtx *actx, struct Body *b, int parent_type, int level)
{
  for (; b; b = b->next)
  {
    struct AttachPtr *ap = mutt_aptr_new();
    mutt_actx_add_attach(actx, ap);
    ap->body = b;
    b->aptr = ap;
    ap->parent_type = parent_type;
    ap->level = level;
    if ((b->type == TYPE_MULTIPART) && b->parts &&
        (!(WithCrypto & APPLICATION_PGP) || !mutt_is_multipart_encrypted(b)))
    {
      gen_attach_list(actx, b->parts, b->type, level + 1);
    }
  }
}

/**
 * update_menu - Redraw the compose window
 * @param actx Attachment context
 * @param menu Current menu
 * @param init If true, initialise the attachment list
 */
void update_menu(struct AttachCtx *actx, struct Menu *menu, bool init)
{
  if (init)
  {
    gen_attach_list(actx, actx->email->body, -1, 0);
    mutt_attach_init(actx);

    struct ComposeAttachData *adata = menu->mdata;
    adata->actx = actx;
  }

  mutt_update_tree(actx);

  menu->max = actx->vcount;
  if (menu->max)
  {
    int index = menu_get_index(menu);
    if (index >= menu->max)
      menu_set_index(menu, menu->max - 1);
  }
  else
  {
    menu_set_index(menu, 0);
  }

  menu_queue_redraw(menu, MENU_REDRAW_INDEX);
}

/**
 * compose_dlg_init - Allocate the Windows for Compose
 * @param sub  ConfigSubset
 * @param e    Email
 * @param fcc  Buffer to save FCC
 * @retval ptr Dialog containing nested Windows
 */
static struct MuttWindow *compose_dlg_init(struct ConfigSubset *sub,
                                           struct Email *e, struct Buffer *fcc)
{
  struct ComposeSharedData *shared = compose_shared_data_new();
  shared->sub = sub;
  shared->email = e;

  struct MuttWindow *dlg = mutt_window_new(WT_DLG_COMPOSE, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);
  dlg->wdata = shared;
  dlg->wdata_free = compose_shared_data_free;

  struct MuttWindow *win_env = env_window_new(e, fcc, sub);
  struct MuttWindow *win_attach = attach_new(dlg, shared);
  struct MuttWindow *win_cbar = cbar_new(shared);
  struct MuttWindow *win_abar = sbar_new();
  sbar_set_title(win_abar, _("-- Attachments"));

  const bool c_status_on_top = cs_subset_bool(sub, "status_on_top");
  if (c_status_on_top)
  {
    mutt_window_add_child(dlg, win_cbar);
    mutt_window_add_child(dlg, win_env);
    mutt_window_add_child(dlg, win_abar);
    mutt_window_add_child(dlg, win_attach);
  }
  else
  {
    mutt_window_add_child(dlg, win_env);
    mutt_window_add_child(dlg, win_abar);
    mutt_window_add_child(dlg, win_attach);
    mutt_window_add_child(dlg, win_cbar);
  }

  dlg->help_data = ComposeHelp;
  dlg->help_menu = MENU_COMPOSE;

  return dlg;
}

/**
 * dlg_compose - Allow the user to edit the message envelope - @ingroup gui_dlg
 * @param e      Email to fill
 * @param fcc    Buffer to save FCC
 * @param flags  Flags, e.g. #MUTT_COMPOSE_NOFREEHEADER
 * @param sub    ConfigSubset
 * @retval  1 Message should be postponed
 * @retval  0 Normal exit
 * @retval -1 Abort message
 *
 * The Compose Dialog allows the user to edit the email envelope before sending.
 */
int dlg_compose(struct Email *e, struct Buffer *fcc, uint8_t flags, struct ConfigSubset *sub)
{
  struct MuttWindow *dlg = compose_dlg_init(sub, e, fcc);
  struct ComposeSharedData *shared = dlg->wdata;
  shared->mailbox = get_current_mailbox();
  shared->email = e;
  shared->sub = sub;
  shared->fcc = fcc;
  shared->fcc_set = false;
  shared->flags = flags;
  shared->rc = -1;

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, compose_config_observer, dlg);
  notify_observer_add(e->notify, NT_ALL, compose_email_observer, shared);
  notify_observer_add(dlg->notify, NT_WINDOW, compose_window_observer, dlg);

#ifdef USE_NNTP
  if (OptNewsSend)
    dlg->help_data = ComposeNewsHelp;
  else
#endif
    dlg->help_data = ComposeHelp;
  dlg->help_menu = MENU_COMPOSE;

  struct Menu *menu = shared->adata->menu;
  update_menu(shared->adata->actx, menu, true);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE, NULL);

  struct MuttWindow *win_env = window_find_child(dlg, WT_CUSTOM);

  dialog_push(dlg);
  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int rc = 0;
  int op = OP_NULL;
  do
  {
#ifdef USE_NNTP
    OptNews = false; /* for any case */
#endif
    menu_tagging_dispatcher(menu->win, op);
    window_redraw(NULL);

    op = km_dokey(MENU_COMPOSE, GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(MENU_COMPOSE);
      continue;
    }
    mutt_clear_error();

    rc = compose_function_dispatcher(dlg, op);
    if (rc == FR_UNKNOWN)
      rc = env_function_dispatcher(win_env, op);
    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while (rc != FR_DONE);
  // ---------------------------------------------------------------------------

#ifdef USE_AUTOCRYPT
  /* This is a fail-safe to make sure the bit isn't somehow turned
   * on.  The user could have disabled the option after setting SEC_AUTOCRYPT,
   * or perhaps resuming or replying to an autocrypt message.  */
  const bool c_autocrypt = cs_subset_bool(sub, "autocrypt");
  if (!c_autocrypt)
    e->security &= ~SEC_AUTOCRYPT;
#endif

  if (shared->adata->actx->idxlen)
    e->body = shared->adata->actx->idx[0]->body;
  else
    e->body = NULL;

  rc = shared->rc;

  window_set_focus(old_focus);
  dialog_pop();
  mutt_window_free(&dlg);

  return rc;
}
