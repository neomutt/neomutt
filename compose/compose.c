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
 * ## Overview
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
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "attach_data.h"
#include "browser.h"
#include "cbar.h"
#include "commands.h"
#include "context.h"
#include "env_data.h"
#include "hook.h"
#include "mutt_attach.h"
#include "mutt_globals.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "recvattach.h"
#include "rfc3676.h"
#include "shared_data.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef MIXMASTER
#include "remailer.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#include "nntp/adata.h" // IWYU pragma: keep
#endif
#ifdef USE_POP
#include "pop/lib.h"
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

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
 * compose_config_observer - Notification that a Config Variable has changed - Implements ::observer_t
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
 * compose_window_observer - Notification that a Window has changed - Implements ::observer_t
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
 * check_count - Check if there are any attachments
 * @param actx Attachment context
 * @retval true There are attachments
 */
static bool check_count(struct AttachCtx *actx)
{
  if (actx->idxlen == 0)
  {
    mutt_error(_("There are no attachments"));
    return false;
  }

  return true;
}

/**
 * current_attachment - Get the current attachment
 * @param actx Attachment context
 * @param menu Menu
 * @retval ptr Current Attachment
 */
static struct AttachPtr *current_attachment(struct AttachCtx *actx, struct Menu *menu)
{
  const int virt = menu_get_index(menu);
  const int index = actx->v2r[virt];

  return actx->idx[index];
}

#ifdef USE_AUTOCRYPT
/**
 * autocrypt_compose_menu - Autocrypt compose settings
 * @param e Email
 * @param sub ConfigSubset
 */
static void autocrypt_compose_menu(struct Email *e, const struct ConfigSubset *sub)
{
  /* L10N: The compose menu autocrypt prompt.
     (e)ncrypt enables encryption via autocrypt.
     (c)lear sets cleartext.
     (a)utomatic defers to the recommendation.  */
  const char *prompt = _("Autocrypt: (e)ncrypt, (c)lear, (a)utomatic?");

  e->security |= APPLICATION_PGP;

  /* L10N: The letter corresponding to the compose menu autocrypt prompt
     (e)ncrypt, (c)lear, (a)utomatic */
  const char *letters = _("eca");

  int choice = mutt_multi_choice(prompt, letters);
  switch (choice)
  {
    case 1:
      e->security |= (SEC_AUTOCRYPT | SEC_AUTOCRYPT_OVERRIDE);
      e->security &= ~(SEC_ENCRYPT | SEC_SIGN | SEC_OPPENCRYPT | SEC_INLINE);
      break;
    case 2:
      e->security &= ~SEC_AUTOCRYPT;
      e->security |= SEC_AUTOCRYPT_OVERRIDE;
      break;
    case 3:
    {
      e->security &= ~SEC_AUTOCRYPT_OVERRIDE;
      const bool c_crypt_opportunistic_encrypt =
          cs_subset_bool(sub, "crypt_opportunistic_encrypt");
      if (c_crypt_opportunistic_encrypt)
        e->security |= SEC_OPPENCRYPT;
      break;
    }
  }
}
#endif

/**
 * update_crypt_info - Update the crypto info
 * @param shared Shared compose data
 */
static void update_crypt_info(struct ComposeSharedData *shared)
{
  struct Email *e = shared->email;
  struct Mailbox *m = shared->mailbox;

  const bool c_crypt_opportunistic_encrypt =
      cs_subset_bool(shared->sub, "crypt_opportunistic_encrypt");
  if (c_crypt_opportunistic_encrypt)
    crypt_opportunistic_encrypt(m, e);

#ifdef USE_AUTOCRYPT
  const bool c_autocrypt = cs_subset_bool(shared->sub, "autocrypt");
  if (c_autocrypt)
  {
    struct ComposeEnvelopeData *edata = shared->edata;
    edata->autocrypt_rec = mutt_autocrypt_ui_recommendation(m, e, NULL);

    /* Anything that enables SEC_ENCRYPT or SEC_SIGN, or turns on SMIME
     * overrides autocrypt, be it oppenc or the user having turned on
     * those flags manually. */
    if (e->security & (SEC_ENCRYPT | SEC_SIGN | APPLICATION_SMIME))
      e->security &= ~(SEC_AUTOCRYPT | SEC_AUTOCRYPT_OVERRIDE);
    else
    {
      if (!(e->security & SEC_AUTOCRYPT_OVERRIDE))
      {
        if (edata->autocrypt_rec == AUTOCRYPT_REC_YES)
        {
          e->security |= (SEC_AUTOCRYPT | APPLICATION_PGP);
          e->security &= ~(SEC_INLINE | APPLICATION_SMIME);
        }
        else
          e->security &= ~SEC_AUTOCRYPT;
      }
    }
  }
#endif
}

/**
 * check_attachments - Check if any attachments have changed or been deleted
 * @param actx Attachment context
 * @param sub  ConfigSubset
 * @retval  0 Success
 * @retval -1 Error
 */
static int check_attachments(struct AttachCtx *actx, struct ConfigSubset *sub)
{
  int rc = -1;
  struct stat st;
  struct Buffer *pretty = NULL, *msg = NULL;

  for (int i = 0; i < actx->idxlen; i++)
  {
    if (actx->idx[i]->body->type == TYPE_MULTIPART)
      continue;
    if (stat(actx->idx[i]->body->filename, &st) != 0)
    {
      if (!pretty)
        pretty = mutt_buffer_pool_get();
      mutt_buffer_strcpy(pretty, actx->idx[i]->body->filename);
      mutt_buffer_pretty_mailbox(pretty);
      /* L10N: This message is displayed in the compose menu when an attachment
         doesn't stat.  %d is the attachment number and %s is the attachment
         filename.  The filename is located last to avoid a long path hiding
         the error message.  */
      mutt_error(_("Attachment #%d no longer exists: %s"), i + 1,
                 mutt_buffer_string(pretty));
      goto cleanup;
    }

    if (actx->idx[i]->body->stamp < st.st_mtime)
    {
      if (!pretty)
        pretty = mutt_buffer_pool_get();
      mutt_buffer_strcpy(pretty, actx->idx[i]->body->filename);
      mutt_buffer_pretty_mailbox(pretty);

      if (!msg)
        msg = mutt_buffer_pool_get();
      /* L10N: This message is displayed in the compose menu when an attachment
         is modified behind the scenes.  %d is the attachment number and %s is
         the attachment filename.  The filename is located last to avoid a long
         path hiding the prompt question.  */
      mutt_buffer_printf(msg, _("Attachment #%d modified. Update encoding for %s?"),
                         i + 1, mutt_buffer_string(pretty));

      enum QuadOption ans = mutt_yesorno(mutt_buffer_string(msg), MUTT_YES);
      if (ans == MUTT_YES)
        mutt_update_encoding(actx->idx[i]->body, sub);
      else if (ans == MUTT_ABORT)
        goto cleanup;
    }
  }

  rc = 0;

cleanup:
  mutt_buffer_pool_release(&pretty);
  mutt_buffer_pool_release(&msg);
  return rc;
}

/**
 * edit_address_list - Let the user edit the address list
 * @param[in]     field Field to edit, e.g. #HDR_FROM
 * @param[in,out] al    AddressList to edit
 * @retval true The address list was changed
 */
static bool edit_address_list(int field, struct AddressList *al)
{
  char buf[8192] = { 0 }; /* needs to be large for alias expansion */
  char old_list[8192] = { 0 };

  mutt_addrlist_to_local(al);
  mutt_addrlist_write(al, buf, sizeof(buf), false);
  mutt_str_copy(old_list, buf, sizeof(buf));
  if (mutt_get_field(_(Prompts[field]), buf, sizeof(buf), MUTT_ALIAS, false, NULL, NULL) == 0)
  {
    mutt_addrlist_clear(al);
    mutt_addrlist_parse2(al, buf);
    mutt_expand_aliases(al);
  }

  char *err = NULL;
  if (mutt_addrlist_to_intl(al, &err) != 0)
  {
    mutt_error(_("Bad IDN: '%s'"), err);
    mutt_refresh();
    FREE(&err);
  }

  return !mutt_str_equal(buf, old_list);
}

/**
 * delete_attachment - Delete an attachment
 * @param actx Attachment context
 * @param x    Index number of attachment
 * @retval  0 Success
 * @retval -1 Error
 */
static int delete_attachment(struct AttachCtx *actx, int x)
{
  struct AttachPtr **idx = actx->idx;
  int rindex = actx->v2r[x];

  if ((rindex == 0) && (actx->idxlen == 1))
  {
    mutt_error(_("You may not delete the only attachment"));
    idx[rindex]->body->tagged = false;
    return -1;
  }

  for (int y = 0; y < actx->idxlen; y++)
  {
    if (idx[y]->body->next == idx[rindex]->body)
    {
      idx[y]->body->next = idx[rindex]->body->next;
      break;
    }
  }

  idx[rindex]->body->next = NULL;
  /* mutt_make_message_attach() creates body->parts, shared by
   * body->email->body.  If we NULL out that, it creates a memory
   * leak because mutt_free_body() frees body->parts, not
   * body->email->body.
   *
   * Other mutt_send_message() message constructors are careful to free
   * any body->parts, removing depth:
   *  - mutt_prepare_template() used by postponed, resent, and draft files
   *  - mutt_copy_body() used by the recvattach menu and $forward_attachments.
   *
   * I believe it is safe to completely remove the "body->parts =
   * NULL" statement.  But for safety, am doing so only for the case
   * it must be avoided: message attachments.
   */
  if (!idx[rindex]->body->email)
    idx[rindex]->body->parts = NULL;
  mutt_body_free(&(idx[rindex]->body));
  FREE(&idx[rindex]->tree);
  FREE(&idx[rindex]);
  for (; rindex < actx->idxlen - 1; rindex++)
    idx[rindex] = idx[rindex + 1];
  idx[actx->idxlen - 1] = NULL;
  actx->idxlen--;

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
    if ((m->type == TYPE_MULTIPART) && m->parts &&
        (!(WithCrypto & APPLICATION_PGP) || !mutt_is_multipart_encrypted(m)))
    {
      gen_attach_list(actx, m->parts, m->type, level);
    }
    else
    {
      struct AttachPtr *ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
      mutt_actx_add_attach(actx, ap);
      ap->body = m;
      m->aptr = ap;
      ap->parent_type = parent_type;
      ap->level = level;

      /* We don't support multipart messages in the compose menu yet */
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
 * update_idx - Add a new attchment to the message
 * @param menu Current menu
 * @param actx Attachment context
 * @param ap   Attachment to add
 */
static void update_idx(struct Menu *menu, struct AttachCtx *actx, struct AttachPtr *ap)
{
  ap->level = (actx->idxlen > 0) ? actx->idx[actx->idxlen - 1]->level : 0;
  if (actx->idxlen)
    actx->idx[actx->idxlen - 1]->body->next = ap->body;
  ap->body->aptr = ap;
  mutt_actx_add_attach(actx, ap);
  update_menu(actx, menu, false);
  menu_set_index(menu, actx->vcount - 1);
}

/**
 * compose_attach_swap - Swap two adjacent entries in the attachment list
 * @param[in]  msg   Body of email
 * @param[out] idx   Array of Attachments
 * @param[in]  first Index of first attachment to swap
 */
static void compose_attach_swap(struct Body *msg, struct AttachPtr **idx, short first)
{
  /* Reorder Body pointers.
   * Must traverse msg from top since Body has no previous ptr.  */
  for (struct Body *part = msg; part; part = part->next)
  {
    if (part->next == idx[first]->body)
    {
      idx[first]->body->next = idx[first + 1]->body->next;
      idx[first + 1]->body->next = idx[first]->body;
      part->next = idx[first + 1]->body;
      break;
    }
  }

  /* Reorder index */
  struct AttachPtr *saved = idx[first];
  idx[first] = idx[first + 1];
  idx[first + 1] = saved;

  /* Swap ptr->num */
  int i = idx[first]->num;
  idx[first]->num = idx[first + 1]->num;
  idx[first + 1]->num = i;
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

  struct MuttWindow *win_env = compose_env_new(dlg, shared, fcc);
  struct MuttWindow *win_attach = attach_new(dlg, shared);
  struct MuttWindow *win_cbar = cbar_new(dlg, shared);
  struct MuttWindow *win_abar = sbar_new(dlg);
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

  char buf[PATH_MAX];
  int rc = -1;
  bool loop = true;
  bool fcc_set = false; /* has the user edited the Fcc: field ? */
  struct Mailbox *m = ctx_mailbox(Context);

#ifdef USE_NNTP
  bool news = OptNewsSend; /* is it a news article ? */
#endif

  struct MuttWindow *dlg = compose_dlg_init(sub, e, fcc);
#ifdef USE_NNTP
  if (news)
    dlg->help_data = ComposeNewsHelp;
#endif
  notify_observer_add(NeoMutt->notify, NT_CONFIG, compose_config_observer, dlg);
  notify_observer_add(dlg->notify, NT_WINDOW, compose_window_observer, dlg);
  dialog_push(dlg);

#ifdef USE_NNTP
  if (news)
    dlg->help_data = ComposeNewsHelp;
  else
#endif
    dlg->help_data = ComposeHelp;
  dlg->help_menu = MENU_COMPOSE;

  struct ComposeSharedData *shared = dlg->wdata;

  // win_env->req_rows = calc_envelope(win_env, shared, shared->edata);

  struct Menu *menu = shared->adata->menu;

  update_menu(shared->adata->actx, menu, true);
  struct AttachCtx *actx = shared->adata->actx;

  update_crypt_info(shared);

  /* Since this is rather long lived, we don't use the pool */
  struct Buffer fname = mutt_buffer_make(PATH_MAX);

  while (loop)
  {
#ifdef USE_NNTP
    OptNews = false; /* for any case */
#endif
    window_redraw(NULL);
    const int op = menu_loop(menu);
    if (op >= 0)
      mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", OpStrings[op][0], op);
    switch (op)
    {
      case OP_COMPOSE_EDIT_FROM:
        if (edit_address_list(HDR_FROM, &e->env->from))
        {
          update_crypt_info(shared);
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;

      case OP_COMPOSE_EDIT_TO:
      {
#ifdef USE_NNTP
        if (news)
          break;
#endif
        if (edit_address_list(HDR_TO, &e->env->to))
        {
          update_crypt_info(shared);
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;
      }

      case OP_COMPOSE_EDIT_BCC:
      {
#ifdef USE_NNTP
        if (news)
          break;
#endif
        if (edit_address_list(HDR_BCC, &e->env->bcc))
        {
          update_crypt_info(shared);
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;
      }

      case OP_COMPOSE_EDIT_CC:
      {
#ifdef USE_NNTP
        if (news)
          break;
#endif
        if (edit_address_list(HDR_CC, &e->env->cc))
        {
          update_crypt_info(shared);
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;
      }

#ifdef USE_NNTP
      case OP_COMPOSE_EDIT_NEWSGROUPS:
        if (!news)
          break;
        mutt_str_copy(buf, e->env->newsgroups, sizeof(buf));
        if (mutt_get_field(Prompts[HDR_NEWSGROUPS], buf, sizeof(buf),
                           MUTT_COMP_NO_FLAGS, false, NULL, NULL) == 0)
        {
          mutt_str_replace(&e->env->newsgroups, buf);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;

      case OP_COMPOSE_EDIT_FOLLOWUP_TO:
        if (!news)
          break;
        mutt_str_copy(buf, e->env->followup_to, sizeof(buf));
        if (mutt_get_field(Prompts[HDR_FOLLOWUPTO], buf, sizeof(buf),
                           MUTT_COMP_NO_FLAGS, false, NULL, NULL) == 0)
        {
          mutt_str_replace(&e->env->followup_to, buf);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;

      case OP_COMPOSE_EDIT_X_COMMENT_TO:
      {
        const bool c_x_comment_to = cs_subset_bool(sub, "x_comment_to");
        if (!(news && c_x_comment_to))
          break;
        mutt_str_copy(buf, e->env->x_comment_to, sizeof(buf));
        if (mutt_get_field(Prompts[HDR_XCOMMENTTO], buf, sizeof(buf),
                           MUTT_COMP_NO_FLAGS, false, NULL, NULL) == 0)
        {
          mutt_str_replace(&e->env->x_comment_to, buf);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;
      }
#endif

      case OP_COMPOSE_EDIT_SUBJECT:
        mutt_str_copy(buf, e->env->subject, sizeof(buf));
        if (mutt_get_field(Prompts[HDR_SUBJECT], buf, sizeof(buf),
                           MUTT_COMP_NO_FLAGS, false, NULL, NULL) == 0)
        {
          if (!mutt_str_equal(e->env->subject, buf))
          {
            mutt_str_replace(&e->env->subject, buf);
            notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
            mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          }
        }
        break;

      case OP_COMPOSE_EDIT_REPLY_TO:
        if (edit_address_list(HDR_REPLYTO, &e->env->reply_to))
        {
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;

      case OP_COMPOSE_EDIT_FCC:
        mutt_buffer_copy(&fname, fcc);
        if (mutt_buffer_get_field(Prompts[HDR_FCC], &fname, MUTT_FILE | MUTT_CLEAR,
                                  false, NULL, NULL, NULL) == 0)
        {
          if (!mutt_str_equal(fcc->data, fname.data))
          {
            mutt_buffer_copy(fcc, &fname);
            mutt_buffer_pretty_mailbox(fcc);
            fcc_set = true;
            notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
            mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          }
        }
        break;

      case OP_COMPOSE_EDIT_MESSAGE:
      {
        const bool c_edit_headers = cs_subset_bool(sub, "edit_headers");
        if (!c_edit_headers)
        {
          mutt_rfc3676_space_unstuff(e);
          const char *const c_editor = cs_subset_string(sub, "editor");
          mutt_edit_file(c_editor, e->body->filename);
          mutt_rfc3676_space_stuff(e);
          mutt_update_encoding(e->body, sub);
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
          /* Unconditional hook since editor was invoked */
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          break;
        }
      }
        /* fallthrough */

      case OP_COMPOSE_EDIT_HEADERS:
      {
        mutt_rfc3676_space_unstuff(e);
        const char *tag = NULL;
        char *err = NULL;
        mutt_env_to_local(e->env);
        const char *const c_editor = cs_subset_string(sub, "editor");
        mutt_edit_headers(NONULL(c_editor), e->body->filename, e, fcc);
        if (mutt_env_to_intl(e->env, &tag, &err))
        {
          mutt_error(_("Bad IDN in '%s': '%s'"), tag, err);
          FREE(&err);
        }
        update_crypt_info(shared);
        notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);

        mutt_rfc3676_space_stuff(e);
        mutt_update_encoding(e->body, sub);

        /* attachments may have been added */
        if (actx->idxlen && actx->idx[actx->idxlen - 1]->body->next)
        {
          mutt_actx_entries_free(actx);
          update_menu(actx, menu, true);
        }

        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        /* Unconditional hook since editor was invoked */
        mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_ATTACH_KEY:
      {
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        struct AttachPtr *ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
        ap->body = crypt_pgp_make_key_attachment();
        if (ap->body)
        {
          update_idx(menu, actx, ap);
          menu_queue_redraw(menu, MENU_REDRAW_INDEX);
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        }
        else
          FREE(&ap);

        notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
        break;
      }

      case OP_COMPOSE_MOVE_UP:
      {
        int index = menu_get_index(menu);
        if (index == 0)
        {
          mutt_error(_("Attachment is already at top"));
          break;
        }
        if (index == 1)
        {
          mutt_error(_("The fundamental part can't be moved"));
          break;
        }
        compose_attach_swap(e->body, actx->idx, index - 1);
        menu_queue_redraw(menu, MENU_REDRAW_INDEX);
        menu_set_index(menu, index - 1);
        break;
      }

      case OP_COMPOSE_MOVE_DOWN:
      {
        int index = menu_get_index(menu);
        if (index == (actx->idxlen - 1))
        {
          mutt_error(_("Attachment is already at bottom"));
          break;
        }
        if (index == 0)
        {
          mutt_error(_("The fundamental part can't be moved"));
          break;
        }
        compose_attach_swap(e->body, actx->idx, index);
        menu_queue_redraw(menu, MENU_REDRAW_INDEX);
        menu_set_index(menu, index + 1);
        break;
      }

      case OP_COMPOSE_GROUP_ALTS:
      {
        if (menu->tagged < 2)
        {
          mutt_error(
              _("Grouping 'alternatives' requires at least 2 tagged messages"));
          break;
        }

        struct Body *group = mutt_body_new();
        group->type = TYPE_MULTIPART;
        group->subtype = mutt_str_dup("alternative");
        group->disposition = DISP_INLINE;

        struct Body *alts = NULL;
        /* group tagged message into a multipart/alternative */
        struct Body *bptr = e->body;
        for (int i = 0; bptr;)
        {
          if (bptr->tagged)
          {
            bptr->tagged = false;
            bptr->disposition = DISP_INLINE;

            /* for first match, set group desc according to match */
#define ALTS_TAG "Alternatives for \"%s\""
            if (!group->description)
            {
              char *p = bptr->description ? bptr->description : bptr->filename;
              if (p)
              {
                group->description =
                    mutt_mem_calloc(1, strlen(p) + strlen(ALTS_TAG) + 1);
                sprintf(group->description, ALTS_TAG, p);
              }
            }

            // append bptr to the alts list, and remove from the e->body list
            if (alts)
            {
              alts->next = bptr;
              bptr = bptr->next;
              alts = alts->next;
              alts->next = NULL;
            }
            else
            {
              group->parts = bptr;
              alts = bptr;
              bptr = bptr->next;
              alts->next = NULL;
            }

            for (int j = i; j < actx->idxlen - 1; j++)
            {
              actx->idx[j] = actx->idx[j + 1];
              actx->idx[j + 1] = NULL; /* for debug reason */
            }
            actx->idxlen--;
          }
          else
          {
            bptr = bptr->next;
            i++;
          }
        }

        group->next = NULL;
        mutt_generate_boundary(&group->parameter);

        /* if no group desc yet, make one up */
        if (!group->description)
          group->description = mutt_str_dup("unknown alternative group");

        struct AttachPtr *gptr = mutt_mem_calloc(1, sizeof(struct AttachPtr));
        gptr->body = group;
        update_idx(menu, actx, gptr);
        menu_queue_redraw(menu, MENU_REDRAW_INDEX);
        break;
      }

      case OP_COMPOSE_GROUP_LINGUAL:
      {
        if (menu->tagged < 2)
        {
          mutt_error(
              _("Grouping 'multilingual' requires at least 2 tagged messages"));
          break;
        }

        /* traverse to see whether all the parts have Content-Language: set */
        int tagged_with_lang_num = 0;
        for (struct Body *b = e->body; b; b = b->next)
          if (b->tagged && b->language && *b->language)
            tagged_with_lang_num++;

        if (menu->tagged != tagged_with_lang_num)
        {
          if (mutt_yesorno(
                  _("Not all parts have 'Content-Language' set, continue?"), MUTT_YES) != MUTT_YES)
          {
            mutt_message(_("Not sending this message"));
            break;
          }
        }

        struct Body *group = mutt_body_new();
        group->type = TYPE_MULTIPART;
        group->subtype = mutt_str_dup("multilingual");
        group->disposition = DISP_INLINE;

        struct Body *alts = NULL;
        /* group tagged message into a multipart/multilingual */
        struct Body *bptr = e->body;
        for (int i = 0; bptr;)
        {
          if (bptr->tagged)
          {
            bptr->tagged = false;
            bptr->disposition = DISP_INLINE;

            /* for first match, set group desc according to match */
#define LINGUAL_TAG "Multilingual part for \"%s\""
            if (!group->description)
            {
              char *p = bptr->description ? bptr->description : bptr->filename;
              if (p)
              {
                group->description =
                    mutt_mem_calloc(1, strlen(p) + strlen(LINGUAL_TAG) + 1);
                sprintf(group->description, LINGUAL_TAG, p);
              }
            }

            // append bptr to the alts list, and remove from the e->body list
            if (alts)
            {
              alts->next = bptr;
              bptr = bptr->next;
              alts = alts->next;
              alts->next = NULL;
            }
            else
            {
              group->parts = bptr;
              alts = bptr;
              bptr = bptr->next;
              alts->next = NULL;
            }

            for (int j = i; j < actx->idxlen - 1; j++)
            {
              actx->idx[j] = actx->idx[j + 1];
              actx->idx[j + 1] = NULL; /* for debug reason */
            }
            actx->idxlen--;
          }
          else
          {
            bptr = bptr->next;
            i++;
          }
        }

        group->next = NULL;
        mutt_generate_boundary(&group->parameter);

        /* if no group desc yet, make one up */
        if (!group->description)
          group->description = mutt_str_dup("unknown multilingual group");

        struct AttachPtr *gptr = mutt_mem_calloc(1, sizeof(struct AttachPtr));
        gptr->body = group;
        update_idx(menu, actx, gptr);
        menu_queue_redraw(menu, MENU_REDRAW_INDEX);
        break;
      }

      case OP_COMPOSE_ATTACH_FILE:
      {
        char *prompt = _("Attach file");
        int numfiles = 0;
        char **files = NULL;

        mutt_buffer_reset(&fname);
        if ((mutt_buffer_enter_fname(prompt, &fname, false, NULL, true, &files,
                                     &numfiles, MUTT_SEL_MULTI) == -1) ||
            mutt_buffer_is_empty(&fname))
        {
          for (int i = 0; i < numfiles; i++)
            FREE(&files[i]);

          FREE(&files);
          break;
        }

        bool error = false;
        bool added_attachment = false;
        if (numfiles > 1)
        {
          mutt_message(ngettext("Attaching selected file...",
                                "Attaching selected files...", numfiles));
        }
        for (int i = 0; i < numfiles; i++)
        {
          char *att = files[i];
          struct AttachPtr *ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
          ap->unowned = true;
          ap->body = mutt_make_file_attach(att, sub);
          if (ap->body)
          {
            added_attachment = true;
            update_idx(menu, actx, ap);
          }
          else
          {
            error = true;
            mutt_error(_("Unable to attach %s"), att);
            FREE(&ap);
          }
          FREE(&files[i]);
        }

        FREE(&files);
        if (!error)
          mutt_clear_error();

        menu_queue_redraw(menu, MENU_REDRAW_INDEX);
        notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
        if (added_attachment)
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_ATTACH_MESSAGE:
#ifdef USE_NNTP
      case OP_COMPOSE_ATTACH_NEWS_MESSAGE:
#endif
      {
        mutt_buffer_reset(&fname);
        char *prompt = _("Open mailbox to attach message from");

#ifdef USE_NNTP
        OptNews = false;
        if (Context && (op == OP_COMPOSE_ATTACH_NEWS_MESSAGE))
        {
          const char *const c_news_server =
              cs_subset_string(sub, "news_server");
          CurrentNewsSrv = nntp_select_server(Context->mailbox, c_news_server, false);
          if (!CurrentNewsSrv)
            break;

          prompt = _("Open newsgroup to attach message from");
          OptNews = true;
        }
#endif

        if (Context)
        {
#ifdef USE_NNTP
          if ((op == OP_COMPOSE_ATTACH_MESSAGE) ^ (Context->mailbox->type == MUTT_NNTP))
#endif
          {
            mutt_buffer_strcpy(&fname, mailbox_path(Context->mailbox));
            mutt_buffer_pretty_mailbox(&fname);
          }
        }

        if ((mutt_buffer_enter_fname(prompt, &fname, true, m, false, NULL, NULL,
                                     MUTT_SEL_NO_FLAGS) == -1) ||
            mutt_buffer_is_empty(&fname))
        {
          break;
        }

#ifdef USE_NNTP
        if (OptNews)
          nntp_expand_path(fname.data, fname.dsize, &CurrentNewsSrv->conn->account);
        else
#endif
          mutt_buffer_expand_path(&fname);
#ifdef USE_IMAP
        if (imap_path_probe(mutt_buffer_string(&fname), NULL) != MUTT_IMAP)
#endif
#ifdef USE_POP
          if (pop_path_probe(mutt_buffer_string(&fname), NULL) != MUTT_POP)
#endif
#ifdef USE_NNTP
            if (!OptNews && (nntp_path_probe(mutt_buffer_string(&fname), NULL) != MUTT_NNTP))
#endif
              if (mx_path_probe(mutt_buffer_string(&fname)) != MUTT_NOTMUCH)
              {
                /* check to make sure the file exists and is readable */
                if (access(mutt_buffer_string(&fname), R_OK) == -1)
                {
                  mutt_perror(mutt_buffer_string(&fname));
                  break;
                }
              }

        menu_queue_redraw(menu, MENU_REDRAW_FULL);

        struct Mailbox *m_attach = mx_path_resolve(mutt_buffer_string(&fname));
        const bool old_readonly = m_attach->readonly;
        if (!mx_mbox_open(m_attach, MUTT_READONLY))
        {
          mutt_error(_("Unable to open mailbox %s"), mutt_buffer_string(&fname));
          mx_fastclose_mailbox(m_attach);
          m_attach = NULL;
          break;
        }
        if (m_attach->msg_count == 0)
        {
          mx_mbox_close(m_attach);
          mutt_error(_("No messages in that folder"));
          break;
        }

        /* `$sort`, `$sort_aux`, `$use_threads` could be changed in mutt_index_menu() */
        const enum SortType old_sort = cs_subset_sort(sub, "sort");
        const enum SortType old_sort_aux = cs_subset_sort(sub, "sort_aux");
        const unsigned char old_use_threads =
            cs_subset_enum(sub, "use_threads");

        OptAttachMsg = true;
        mutt_message(_("Tag the messages you want to attach"));
        struct MuttWindow *dlg_index = index_pager_init();
        dialog_push(dlg_index);
        struct Mailbox *m_attach_new = mutt_index_menu(dlg_index, m_attach);
        dialog_pop();
        mutt_window_free(&dlg_index);
        OptAttachMsg = false;

        if (!Context)
        {
          /* Restore old $sort variables */
          cs_subset_str_native_set(sub, "sort", old_sort, NULL);
          cs_subset_str_native_set(sub, "sort_aux", old_sort_aux, NULL);
          cs_subset_str_native_set(sub, "use_threads", old_use_threads, NULL);
          menu_queue_redraw(menu, MENU_REDRAW_INDEX);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
          break;
        }

        bool added_attachment = false;
        for (int i = 0; i < m_attach_new->msg_count; i++)
        {
          if (!m_attach_new->emails[i])
            break;
          if (!message_is_tagged(m_attach_new->emails[i]))
            continue;

          struct AttachPtr *ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
          ap->body = mutt_make_message_attach(m_attach_new,
                                              m_attach_new->emails[i], true, sub);
          if (ap->body)
          {
            added_attachment = true;
            update_idx(menu, actx, ap);
          }
          else
          {
            mutt_error(_("Unable to attach"));
            FREE(&ap);
          }
        }
        menu_queue_redraw(menu, MENU_REDRAW_FULL);

        if (m_attach_new == m_attach)
        {
          m_attach->readonly = old_readonly;
        }
        mx_fastclose_mailbox(m_attach_new);

        /* Restore old $sort variables */
        cs_subset_str_native_set(sub, "sort", old_sort, NULL);
        cs_subset_str_native_set(sub, "sort_aux", old_sort_aux, NULL);
        cs_subset_str_native_set(sub, "use_threads", old_use_threads, NULL);
        if (added_attachment)
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        break;
      }

      case OP_DELETE:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        if (cur_att->unowned)
          cur_att->body->unlink = false;
        int index = menu_get_index(menu);
        if (delete_attachment(actx, index) == -1)
          break;
        update_menu(actx, menu, false);
        notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
        index = menu_get_index(menu);
        if (index == 0)
          e->body = actx->idx[0]->body;

        mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_TOGGLE_RECODE:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        if (!mutt_is_text_part(cur_att->body))
        {
          mutt_error(_("Recoding only affects text attachments"));
          break;
        }
        cur_att->body->noconv = !cur_att->body->noconv;
        if (cur_att->body->noconv)
          mutt_message(_("The current attachment won't be converted"));
        else
          mutt_message(_("The current attachment will be converted"));
        menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
        mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_EDIT_DESCRIPTION:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_str_copy(buf, cur_att->body->description, sizeof(buf));
        /* header names should not be translated */
        if (mutt_get_field("Description: ", buf, sizeof(buf),
                           MUTT_COMP_NO_FLAGS, false, NULL, NULL) == 0)
        {
          if (!mutt_str_equal(cur_att->body->description, buf))
          {
            mutt_str_replace(&cur_att->body->description, buf);
            menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
            mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          }
        }
        break;
      }

      case OP_COMPOSE_UPDATE_ENCODING:
      {
        if (!check_count(actx))
          break;
        bool encoding_updated = false;
        if (menu->tagprefix)
        {
          struct Body *top = NULL;
          for (top = e->body; top; top = top->next)
          {
            if (top->tagged)
            {
              encoding_updated = true;
              mutt_update_encoding(top, sub);
            }
          }
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
        }
        else
        {
          struct AttachPtr *cur_att = current_attachment(actx, menu);
          mutt_update_encoding(cur_att->body, sub);
          encoding_updated = true;
          menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
        }
        if (encoding_updated)
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_TOGGLE_DISPOSITION:
      {
        /* toggle the content-disposition between inline/attachment */
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        cur_att->body->disposition =
            (cur_att->body->disposition == DISP_INLINE) ? DISP_ATTACH : DISP_INLINE;
        menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
        break;
      }

      case OP_EDIT_TYPE:
      {
        if (!check_count(actx))
          break;
        {
          struct AttachPtr *cur_att = current_attachment(actx, menu);
          if (mutt_edit_content_type(NULL, cur_att->body, NULL))
          {
            /* this may have been a change to text/something */
            mutt_update_encoding(cur_att->body, sub);
            menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
            mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          }
        }
        break;
      }

      case OP_COMPOSE_EDIT_LANGUAGE:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_str_copy(buf, cur_att->body->language, sizeof(buf));
        if (mutt_get_field("Content-Language: ", buf, sizeof(buf),
                           MUTT_COMP_NO_FLAGS, false, NULL, NULL) == 0)
        {
          if (!mutt_str_equal(cur_att->body->language, buf))
          {
            cur_att->body->language = mutt_str_dup(buf);
            menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
            notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
            mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          }
          mutt_clear_error();
        }
        else
          mutt_warning(_("Empty 'Content-Language'"));
        break;
      }

      case OP_COMPOSE_EDIT_ENCODING:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_str_copy(buf, ENCODING(cur_att->body->encoding), sizeof(buf));
        if ((mutt_get_field("Content-Transfer-Encoding: ", buf, sizeof(buf),
                            MUTT_COMP_NO_FLAGS, false, NULL, NULL) == 0) &&
            (buf[0] != '\0'))
        {
          int enc = mutt_check_encoding(buf);
          if ((enc != ENC_OTHER) && (enc != ENC_UUENCODED))
          {
            if (enc != cur_att->body->encoding)
            {
              cur_att->body->encoding = enc;
              menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
              notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
              mutt_clear_error();
              mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
            }
          }
          else
            mutt_error(_("Invalid encoding"));
        }
        break;
      }

      case OP_COMPOSE_SEND_MESSAGE:
        /* Note: We don't invoke send2-hook here, since we want to leave
         * users an opportunity to change settings from the ":" prompt.  */
        if (check_attachments(actx, sub) != 0)
        {
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
          break;
        }

#ifdef MIXMASTER
        if (!STAILQ_EMPTY(&e->chain) && (mix_check_message(e) != 0))
          break;
#endif

        if (!fcc_set && !mutt_buffer_is_empty(fcc))
        {
          const enum QuadOption c_copy = cs_subset_quad(sub, "copy");
          enum QuadOption ans =
              query_quadoption(c_copy, _("Save a copy of this message?"));
          if (ans == MUTT_ABORT)
            break;
          else if (ans == MUTT_NO)
            mutt_buffer_reset(fcc);
        }

        loop = false;
        rc = 0;
        break;

      case OP_COMPOSE_EDIT_FILE:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        const char *const c_editor = cs_subset_string(sub, "editor");
        mutt_edit_file(NONULL(c_editor), cur_att->body->filename);
        mutt_update_encoding(cur_att->body, sub);
        menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
        notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
        /* Unconditional hook since editor was invoked */
        mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_TOGGLE_UNLINK:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        cur_att->body->unlink = !cur_att->body->unlink;

        menu_queue_redraw(menu, MENU_REDRAW_INDEX);
        /* No send2hook since this doesn't change the message. */
        break;
      }

      case OP_COMPOSE_GET_ATTACHMENT:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        if (menu->tagprefix)
        {
          for (struct Body *top = e->body; top; top = top->next)
          {
            if (top->tagged)
              mutt_get_tmp_attachment(top);
          }
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
        }
        else if (mutt_get_tmp_attachment(cur_att->body) == 0)
          menu_queue_redraw(menu, MENU_REDRAW_CURRENT);

        /* No send2hook since this doesn't change the message. */
        break;
      }

      case OP_COMPOSE_RENAME_ATTACHMENT:
      {
        if (!check_count(actx))
          break;
        char *src = NULL;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        if (cur_att->body->d_filename)
          src = cur_att->body->d_filename;
        else
          src = cur_att->body->filename;
        mutt_buffer_strcpy(&fname, mutt_path_basename(NONULL(src)));
        int ret = mutt_buffer_get_field(_("Send attachment with name: "), &fname,
                                        MUTT_FILE, false, NULL, NULL, NULL);
        if (ret == 0)
        {
          /* As opposed to RENAME_FILE, we don't check buf[0] because it's
           * valid to set an empty string here, to erase what was set */
          mutt_str_replace(&cur_att->body->d_filename, mutt_buffer_string(&fname));
          menu_queue_redraw(menu, MENU_REDRAW_CURRENT);
        }
        break;
      }

      case OP_COMPOSE_RENAME_FILE:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_buffer_strcpy(&fname, cur_att->body->filename);
        mutt_buffer_pretty_mailbox(&fname);
        if ((mutt_buffer_get_field(_("Rename to: "), &fname, MUTT_FILE, false,
                                   NULL, NULL, NULL) == 0) &&
            !mutt_buffer_is_empty(&fname))
        {
          struct stat st;
          if (stat(cur_att->body->filename, &st) == -1)
          {
            /* L10N: "stat" is a system call. Do "man 2 stat" for more information. */
            mutt_error(_("Can't stat %s: %s"), mutt_buffer_string(&fname), strerror(errno));
            break;
          }

          mutt_buffer_expand_path(&fname);
          if (mutt_file_rename(cur_att->body->filename, mutt_buffer_string(&fname)))
            break;

          mutt_str_replace(&cur_att->body->filename, mutt_buffer_string(&fname));
          menu_queue_redraw(menu, MENU_REDRAW_CURRENT);

          if (cur_att->body->stamp >= st.st_mtime)
            mutt_stamp_attachment(cur_att->body);
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        }
        break;
      }

      case OP_COMPOSE_NEW_MIME:
      {
        mutt_buffer_reset(&fname);
        if ((mutt_buffer_get_field(_("New file: "), &fname, MUTT_FILE, false,
                                   NULL, NULL, NULL) != 0) ||
            mutt_buffer_is_empty(&fname))
        {
          continue;
        }
        mutt_buffer_expand_path(&fname);

        /* Call to lookup_mime_type () ?  maybe later */
        char type[256] = { 0 };
        if ((mutt_get_field("Content-Type: ", type, sizeof(type),
                            MUTT_COMP_NO_FLAGS, false, NULL, NULL) != 0) ||
            (type[0] == '\0'))
        {
          continue;
        }

        char *p = strchr(type, '/');
        if (!p)
        {
          mutt_error(_("Content-Type is of the form base/sub"));
          continue;
        }
        *p++ = 0;
        enum ContentType itype = mutt_check_mime_type(type);
        if (itype == TYPE_OTHER)
        {
          mutt_error(_("Unknown Content-Type %s"), type);
          continue;
        }
        struct AttachPtr *ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
        /* Touch the file */
        FILE *fp = mutt_file_fopen(mutt_buffer_string(&fname), "w");
        if (!fp)
        {
          mutt_error(_("Can't create file %s"), mutt_buffer_string(&fname));
          FREE(&ap);
          continue;
        }
        mutt_file_fclose(&fp);

        ap->body = mutt_make_file_attach(mutt_buffer_string(&fname), sub);
        if (!ap->body)
        {
          mutt_error(_("What we have here is a failure to make an attachment"));
          FREE(&ap);
          continue;
        }
        update_idx(menu, actx, ap);

        struct AttachPtr *cur_att = current_attachment(actx, menu);
        cur_att->body->type = itype;
        mutt_str_replace(&cur_att->body->subtype, p);
        cur_att->body->unlink = true;
        menu_queue_redraw(menu, MENU_REDRAW_INDEX);
        notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);

        if (mutt_compose_attachment(cur_att->body))
        {
          mutt_update_encoding(cur_att->body, sub);
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
        }
        mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_EDIT_MIME:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        if (mutt_edit_attachment(cur_att->body))
        {
          mutt_update_encoding(cur_att->body, sub);
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        }
        break;
      }

      case OP_VIEW_ATTACH:
      case OP_DISPLAY_HEADERS:
        if (!check_count(actx))
          break;
        mutt_attach_display_loop(sub, menu, op, e, actx, false);
        menu_queue_redraw(menu, MENU_REDRAW_FULL);
        /* no send2hook, since this doesn't modify the message */
        break;

      case OP_SAVE:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_save_attachment_list(actx, NULL, menu->tagprefix, cur_att->body, NULL, menu);
        /* no send2hook, since this doesn't modify the message */
        break;
      }

      case OP_PRINT:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_print_attachment_list(actx, NULL, menu->tagprefix, cur_att->body);
        /* no send2hook, since this doesn't modify the message */
        break;
      }

      case OP_PIPE:
      case OP_FILTER:
      {
        if (!check_count(actx))
          break;
        struct AttachPtr *cur_att = current_attachment(actx, menu);
        mutt_pipe_attachment_list(actx, NULL, menu->tagprefix, cur_att->body,
                                  (op == OP_FILTER));
        if (op == OP_FILTER) /* cte might have changed */
          menu_queue_redraw(menu, menu->tagprefix ? MENU_REDRAW_FULL : MENU_REDRAW_CURRENT);
        notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
        mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        break;
      }

      case OP_EXIT:
      {
        const enum QuadOption c_postpone = cs_subset_quad(sub, "postpone");
        enum QuadOption ans =
            query_quadoption(c_postpone, _("Save (postpone) draft message?"));
        if (ans == MUTT_NO)
        {
          for (int i = 0; i < actx->idxlen; i++)
            if (actx->idx[i]->unowned)
              actx->idx[i]->body->unlink = false;

          if (!(flags & MUTT_COMPOSE_NOFREEHEADER))
          {
            for (int i = 0; i < actx->idxlen; i++)
            {
              /* avoid freeing other attachments */
              actx->idx[i]->body->next = NULL;
              /* See the comment in delete_attachment() */
              if (!actx->idx[i]->body->email)
                actx->idx[i]->body->parts = NULL;
              mutt_body_free(&actx->idx[i]->body);
            }
          }
          rc = -1;
          loop = false;
          break;
        }
        else if (ans == MUTT_ABORT)
          break; /* abort */
      }
        /* fallthrough */

      case OP_COMPOSE_POSTPONE_MESSAGE:
        if (check_attachments(actx, sub) != 0)
        {
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
          break;
        }

        loop = false;
        rc = 1;
        break;

      case OP_COMPOSE_ISPELL:
      {
        endwin();
        const char *const c_ispell = cs_subset_string(sub, "ispell");
        snprintf(buf, sizeof(buf), "%s -x %s", NONULL(c_ispell), e->body->filename);
        if (mutt_system(buf) == -1)
          mutt_error(_("Error running \"%s\""), buf);
        else
        {
          mutt_update_encoding(e->body, sub);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ATTACH, NULL);
        }
        break;
      }

      case OP_COMPOSE_WRITE_MESSAGE:
        mutt_buffer_reset(&fname);
        if (Context)
        {
          mutt_buffer_strcpy(&fname, mailbox_path(Context->mailbox));
          mutt_buffer_pretty_mailbox(&fname);
        }
        if (actx->idxlen)
          e->body = actx->idx[0]->body;
        if ((mutt_buffer_enter_fname(_("Write message to mailbox"), &fname, true, m,
                                     false, NULL, NULL, MUTT_SEL_NO_FLAGS) != -1) &&
            !mutt_buffer_is_empty(&fname))
        {
          mutt_message(_("Writing message to %s ..."), mutt_buffer_string(&fname));
          mutt_buffer_expand_path(&fname);

          if (e->body->next)
            e->body = mutt_make_multipart(e->body);

          if (mutt_write_fcc(mutt_buffer_string(&fname), e, NULL, false, NULL, NULL, sub) == 0)
            mutt_message(_("Message written"));

          e->body = mutt_remove_multipart(e->body);
        }
        break;

      case OP_COMPOSE_PGP_MENU:
      {
        const SecurityFlags old_flags = e->security;
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        if (!crypt_has_module_backend(APPLICATION_PGP))
        {
          mutt_error(_("No PGP backend configured"));
          break;
        }
        if (((WithCrypto & APPLICATION_SMIME) != 0) && (e->security & APPLICATION_SMIME))
        {
          if (e->security & (SEC_ENCRYPT | SEC_SIGN))
          {
            if (mutt_yesorno(_("S/MIME already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
            {
              mutt_clear_error();
              break;
            }
            e->security &= ~(SEC_ENCRYPT | SEC_SIGN);
          }
          e->security &= ~APPLICATION_SMIME;
          e->security |= APPLICATION_PGP;
          update_crypt_info(shared);
        }
        e->security = crypt_pgp_send_menu(m, e);
        update_crypt_info(shared);
        if (old_flags != e->security)
        {
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;
      }

      case OP_FORGET_PASSPHRASE:
        crypt_forget_passphrase();
        break;

      case OP_COMPOSE_SMIME_MENU:
      {
        const SecurityFlags old_flags = e->security;
        if (!(WithCrypto & APPLICATION_SMIME))
          break;
        if (!crypt_has_module_backend(APPLICATION_SMIME))
        {
          mutt_error(_("No S/MIME backend configured"));
          break;
        }

        if (((WithCrypto & APPLICATION_PGP) != 0) && (e->security & APPLICATION_PGP))
        {
          if (e->security & (SEC_ENCRYPT | SEC_SIGN))
          {
            if (mutt_yesorno(_("PGP already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
            {
              mutt_clear_error();
              break;
            }
            e->security &= ~(SEC_ENCRYPT | SEC_SIGN);
          }
          e->security &= ~APPLICATION_PGP;
          e->security |= APPLICATION_SMIME;
          update_crypt_info(shared);
        }
        e->security = crypt_smime_send_menu(m, e);
        update_crypt_info(shared);
        if (old_flags != e->security)
        {
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;
      }

#ifdef MIXMASTER
      case OP_COMPOSE_MIX:
        dlg_select_mixmaster_chain(&e->chain);
        mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
        notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        break;
#endif

#ifdef USE_AUTOCRYPT
      case OP_COMPOSE_AUTOCRYPT_MENU:
      {
        const SecurityFlags old_flags = e->security;
        const bool c_autocrypt = cs_subset_bool(sub, "autocrypt");
        if (!c_autocrypt)
          break;

        if ((WithCrypto & APPLICATION_SMIME) && (e->security & APPLICATION_SMIME))
        {
          if (e->security & (SEC_ENCRYPT | SEC_SIGN))
          {
            if (mutt_yesorno(_("S/MIME already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
            {
              mutt_clear_error();
              break;
            }
            e->security &= ~(SEC_ENCRYPT | SEC_SIGN);
          }
          e->security &= ~APPLICATION_SMIME;
          e->security |= APPLICATION_PGP;
          update_crypt_info(shared);
        }
        autocrypt_compose_menu(e, sub);
        update_crypt_info(shared);
        if (old_flags != e->security)
        {
          mutt_message_hook(NULL, e, MUTT_SEND2_HOOK);
          notify_send(shared->notify, NT_COMPOSE, NT_COMPOSE_ENVELOPE, NULL);
        }
        break;
      }
#endif
    }
  }

  mutt_buffer_dealloc(&fname);

#ifdef USE_AUTOCRYPT
  /* This is a fail-safe to make sure the bit isn't somehow turned
   * on.  The user could have disabled the option after setting SEC_AUTOCRYPT,
   * or perhaps resuming or replying to an autocrypt message.  */
  const bool c_autocrypt = cs_subset_bool(sub, "autocrypt");
  if (!c_autocrypt)
    e->security &= ~SEC_AUTOCRYPT;
#endif

  if (actx->idxlen)
    e->body = actx->idx[0]->body;
  else
    e->body = NULL;

  dialog_pop();
  mutt_window_free(&dlg);

  return rc;
}
