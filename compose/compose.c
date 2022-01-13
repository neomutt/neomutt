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
 * @page compose_dialog Compose Email Dialog
 *
 * The Compose Email Dialog lets the user edit the fields before sending an email.
 * They can also add/remove/reorder attachments.
 *
 * ## Windows
 *
 * | Name                 | Type           | See Also            |
 * | :------------------- | :------------- | :------------------ |
 * | Compose Email Dialog | WT_DLG_COMPOSE | mutt_compose_menu() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - @ref compose_envelope
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
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "attach_data.h"
#include "cbar.h"
#include "context.h"
#include "functions.h"
#include "mutt_globals.h"
#include "opcodes.h"
#include "options.h"
#include "shared_data.h"

int HeaderPadding[HDR_ATTACH_TITLE] = { 0 };
int MaxHeaderWidth = 0;

const char *const Prompts[] = {
  /* L10N: Compose menu field.  May not want to translate. */
  N_("From: "),
  /* L10N: Compose menu field.  May not want to translate. */
  N_("To: "),
  /* L10N: Compose menu field.  May not want to translate. */
  N_("Cc: "),
  /* L10N: Compose menu field.  May not want to translate. */
  N_("Bcc: "),
  /* L10N: Compose menu field.  May not want to translate. */
  N_("Subject: "),
  /* L10N: Compose menu field.  May not want to translate. */
  N_("Reply-To: "),
  /* L10N: Compose menu field.  May not want to translate. */
  N_("Fcc: "),
#ifdef MIXMASTER
  /* L10N: "Mix" refers to the MixMaster chain for anonymous email */
  N_("Mix: "),
#endif
  /* L10N: Compose menu field.  Holds "Encrypt", "Sign" related information */
  N_("Security: "),
  /* L10N: This string is used by the compose menu.
     Since it is hidden by default, it does not increase the indentation of
     other compose menu fields.  However, if possible, it should not be longer
     than the other compose menu fields.  Since it shares the row with "Encrypt
     with:", it should not be longer than 15-20 character cells.  */
  N_("Sign as: "),
#ifdef USE_AUTOCRYPT
  // L10N: The compose menu autocrypt line
  N_("Autocrypt: "),
#endif
#ifdef USE_NNTP
  /* L10N: Compose menu field.  May not want to translate. */
  N_("Newsgroups: "),
  /* L10N: Compose menu field.  May not want to translate. */
  N_("Followup-To: "),
  /* L10N: Compose menu field.  May not want to translate. */
  N_("X-Comment-To: "),
#endif
  N_("Headers: "),
};

/// Help Bar for the Compose dialog
static const struct Mapping ComposeHelp[] = {
  // clang-format off
  { N_("Send"),        OP_COMPOSE_SEND_MESSAGE },
  { N_("Abort"),       OP_EXIT },
  /* L10N: compose menu help line entry */
  { N_("To"),          OP_COMPOSE_EDIT_TO },
  /* L10N: compose menu help line entry */
  { N_("CC"),          OP_COMPOSE_EDIT_CC },
  /* L10N: compose menu help line entry */
  { N_("Subj"),        OP_COMPOSE_EDIT_SUBJECT },
  { N_("Attach file"), OP_COMPOSE_ATTACH_FILE },
  { N_("Descrip"),     OP_COMPOSE_EDIT_DESCRIPTION },
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
  { N_("Newsgroups"),  OP_COMPOSE_EDIT_NEWSGROUPS },
  { N_("Subj"),        OP_COMPOSE_EDIT_SUBJECT },
  { N_("Attach file"), OP_COMPOSE_ATTACH_FILE },
  { N_("Descrip"),     OP_COMPOSE_EDIT_DESCRIPTION },
  { N_("Help"),        OP_HELP },
  { NULL, 0 },
  // clang-format on
};
#endif

/**
 * calc_header_width_padding - Calculate the width needed for the compose labels
 * @param idx      Store the result at this index of HeaderPadding
 * @param header   Header string
 * @param calc_max If true, calculate the maximum width
 */
static void calc_header_width_padding(int idx, const char *header, bool calc_max)
{
  int width;

  HeaderPadding[idx] = mutt_str_len(header);
  width = mutt_strwidth(header);
  if (calc_max && (MaxHeaderWidth < width))
    MaxHeaderWidth = width;
  HeaderPadding[idx] -= width;
}

/**
 * init_header_padding - Calculate how much padding the compose table will need
 *
 * The padding needed for each header is strlen() + max_width - strwidth().
 *
 * calc_header_width_padding sets each entry in HeaderPadding to strlen -
 * width.  Then, afterwards, we go through and add max_width to each entry.
 */
static void init_header_padding(void)
{
  static bool done = false;

  if (done)
    return;
  done = true;

  for (int i = 0; i < HDR_ATTACH_TITLE; i++)
  {
    if (i == HDR_CRYPTINFO)
      continue;
    calc_header_width_padding(i, _(Prompts[i]), true);
  }

  /* Don't include "Sign as: " in the MaxHeaderWidth calculation.  It
   * doesn't show up by default, and so can make the indentation of
   * the other fields look funny. */
  calc_header_width_padding(HDR_CRYPTINFO, _(Prompts[HDR_CRYPTINFO]), false);

  for (int i = 0; i < HDR_ATTACH_TITLE; i++)
  {
    HeaderPadding[i] += MaxHeaderWidth;
    if (HeaderPadding[i] < 0)
      HeaderPadding[i] = 0;
  }
}

/**
 * compose_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int compose_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  if (!mutt_str_equal(ev_c->name, "status_on_top"))
    return 0;

  struct MuttWindow *win_cbar = window_find_child(dlg, WT_STATUS_BAR);
  if (!win_cbar)
    return 0;

  TAILQ_REMOVE(&dlg->children, win_cbar, entries);

  const bool c_status_on_top = cs_subset_bool(ev_c->sub, "status_on_top");
  if (c_status_on_top)
    TAILQ_INSERT_HEAD(&dlg->children, win_cbar, entries);
  else
    TAILQ_INSERT_TAIL(&dlg->children, win_cbar, entries);

  mutt_window_reflow(dlg);
  mutt_debug(LL_DEBUG5, "config done, request WA_REFLOW\n");
  return 0;
}

/**
 * compose_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int compose_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *dlg = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != dlg)
    return 0;

  notify_observer_remove(NeoMutt->notify, compose_config_observer, dlg);
  notify_observer_remove(dlg->notify, compose_window_observer, dlg);
  mutt_debug(LL_DEBUG5, "window delete done\n");

  return 0;
}

/**
 * gen_attach_list - Generate the attachment list for the compose screen
 * @param actx        Attachment context
 * @param m           Attachment
 * @param parent_type Attachment type, e.g #TYPE_MULTIPART
 * @param level       Nesting depth of attachment
 */
static void gen_attach_list(struct AttachCtx *actx, struct Body *m, int parent_type, int level)
{
  for (; m; m = m->next)
  {
    struct AttachPtr *ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
    mutt_actx_add_attach(actx, ap);
    ap->body = m;
    m->aptr = ap;
    ap->parent_type = parent_type;
    ap->level = level;
    if ((m->type == TYPE_MULTIPART) && m->parts &&
        (!(WithCrypto & APPLICATION_PGP) || !mutt_is_multipart_encrypted(m)))
    {
      gen_attach_list(actx, m->parts, m->type, level + 1);
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
    menu_set_index(menu, 0);

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

  struct MuttWindow *dlg =
      mutt_window_new(WT_DLG_COMPOSE, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  dlg->wdata = shared;
  dlg->wdata_free = compose_shared_data_free;

  struct MuttWindow *win_env = compose_env_new(shared, fcc);
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
  dlg->focus = win_attach;

  return dlg;
}

/**
 * mutt_compose_menu - Allow the user to edit the message envelope
 * @param e      Email to fill
 * @param fcc    Buffer to save FCC
 * @param flags  Flags, e.g. #MUTT_COMPOSE_NOFREEHEADER
 * @param sub    ConfigSubset
 * @retval  1 Message should be postponed
 * @retval  0 Normal exit
 * @retval -1 Abort message
 */
int mutt_compose_menu(struct Email *e, struct Buffer *fcc, uint8_t flags,
                      struct ConfigSubset *sub)
{
  init_header_padding();

  bool loop = true;

  struct MuttWindow *dlg = compose_dlg_init(sub, e, fcc);
  struct ComposeSharedData *shared = dlg->wdata;
  shared->mailbox = get_current_mailbox();
  shared->email = e;
  shared->sub = sub;
  shared->fcc = fcc;
  shared->fcc_set = false;
  shared->flags = flags;
  shared->rc = -1;
#ifdef USE_NNTP
  shared->news = OptNewsSend;
#endif

#ifdef USE_NNTP
  if (shared->news)
    dlg->help_data = ComposeNewsHelp;
#endif

  notify_observer_add(NeoMutt->notify, NT_CONFIG, compose_config_observer, dlg);
  notify_observer_add(dlg->notify, NT_WINDOW, compose_window_observer, dlg);
  dialog_push(dlg);

#ifdef USE_NNTP
  if (shared->news)
    dlg->help_data = ComposeNewsHelp;
  else
#endif
    dlg->help_data = ComposeHelp;
  dlg->help_menu = MENU_COMPOSE;

  update_menu(shared->adata->actx, shared->adata->menu, true);
  update_crypt_info(shared);

  while (loop)
  {
#ifdef USE_NNTP
    OptNews = false; /* for any case */
#endif
    window_redraw(NULL);
    const int op = menu_loop(shared->adata->menu);
    if (op >= 0)
      mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", OpStrings[op][0], op);

    int rc = compose_function_dispatcher(dlg, op);
    if (rc == IR_DONE)
      break;
  }

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

  const int rc = shared->rc;

  dialog_pop();
  mutt_window_free(&dlg);

  return rc;
}
