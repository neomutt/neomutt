/**
 * @file
 * Pager functions
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page pager_functions Pager functions
 *
 * Pager functions
 */

#include "config.h"
#include <stddef.h>
#include <assert.h>
#include <inttypes.h> // IWYU pragma: keep
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "lib.h"
#include "attach/lib.h"
#include "color/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "commands.h"
#include "context.h"
#include "display.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_header.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#include "private_data.h"
#include "protos.h"
#include "recvcmd.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#include "nntp/mdata.h" // IWYU pragma: keep
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

static const char *Not_available_in_this_menu =
    N_("Not available in this menu");
static const char *Mailbox_is_read_only = N_("Mailbox is read-only");
static const char *Function_not_permitted_in_attach_message_mode =
    N_("Function not permitted in attach-message mode");

static int op_search_next(struct IndexSharedData *shared,
                          struct PagerPrivateData *priv, int op);

/**
 * assert_pager_mode - Check that pager is in correct mode
 * @param test   Test condition
 * @retval true  Expected mode is set
 * @retval false Pager is is some other mode
 *
 * @note On failure, the input will be flushed and an error message displayed
 */
static inline bool assert_pager_mode(bool test)
{
  if (test)
    return true;

  mutt_flushinp();
  mutt_error(_(Not_available_in_this_menu));
  return false;
}

/**
 * assert_mailbox_writable - checks that mailbox is writable
 * @param mailbox mailbox to check
 * @retval true  Mailbox is writable
 * @retval false Mailbox is not writable
 *
 * @note On failure, the input will be flushed and an error message displayed
 */
static inline bool assert_mailbox_writable(struct Mailbox *mailbox)
{
  assert(mailbox);
  if (mailbox->readonly)
  {
    mutt_flushinp();
    mutt_error(_(Mailbox_is_read_only));
    return false;
  }
  return true;
}

/**
 * assert_attach_msg_mode - Check that attach message mode is on
 * @param attach_msg Globally-named boolean pseudo-option
 * @retval true  Attach message mode in on
 * @retval false Attach message mode is off
 *
 * @note On true, the input will be flushed and an error message displayed
 */
static inline bool assert_attach_msg_mode(bool attach_msg)
{
  if (attach_msg)
  {
    mutt_flushinp();
    mutt_error(_(Function_not_permitted_in_attach_message_mode));
    return true;
  }
  return false;
}

/**
 * assert_mailbox_permissions - checks that mailbox is has requested acl flags set
 * @param m      Mailbox to check
 * @param acl    AclFlags required to be set on a given mailbox
 * @param action String to augment error message
 * @retval true  Mailbox has necessary flags set
 * @retval false Mailbox does not have necessary flags set
 *
 * @note On failure, the input will be flushed and an error message displayed
 */
static inline bool assert_mailbox_permissions(struct Mailbox *m, AclFlags acl, char *action)
{
  assert(m);
  assert(action);
  if (m->rights & acl)
  {
    return true;
  }
  mutt_flushinp();
  /* L10N: %s is one of the CHECK_ACL entries below. */
  mutt_error(_("%s: Operation not permitted by ACL"), action);
  return false;
}

/**
 * up_n_lines - Reposition the pager's view up by n lines
 * @param nlines Number of lines to move
 * @param info   Line info array
 * @param cur    Current line number
 * @param hiding true if lines have been hidden
 * @retval num New current line number
 */
static int up_n_lines(int nlines, struct Line *info, int cur, bool hiding)
{
  while ((cur > 0) && (nlines > 0))
  {
    cur--;
    if (!hiding || (info[cur].color != MT_COLOR_QUOTED))
      nlines--;
  }

  return cur;
}

/**
 * jump_to_bottom - make sure the bottom line is displayed
 * @param priv   Private Pager data
 * @param pview PagerView
 * @retval true Something changed
 * @retval false Bottom was already displayed
 */
bool jump_to_bottom(struct PagerPrivateData *priv, struct PagerView *pview)
{
  if (!(priv->lines[priv->cur_line].offset < (priv->st.st_size - 1)))
  {
    return false;
  }

  int line_num = priv->cur_line;
  /* make sure the types are defined to the end of file */
  while (display_line(priv->fp, &priv->bytes_read, &priv->lines, line_num,
                      &priv->lines_used, &priv->lines_max,
                      priv->has_types | (pview->flags & MUTT_PAGER_NOWRAP),
                      &priv->quote_list, &priv->q_level, &priv->force_redraw,
                      &priv->search_re, priv->pview->win_pager) == 0)
  {
    line_num++;
  }
  priv->top_line = up_n_lines(priv->pview->win_pager->state.rows, priv->lines,
                              priv->lines_used, priv->hide_quoted);
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return true;
}

/**
 * op_bounce_message - remail a message to another user - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_bounce_message(struct IndexSharedData *shared,
                             struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH_E)))
  {
    return IR_NOT_IMPL;
  }
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;
  if (pview->mode == PAGER_MODE_ATTACH_E)
  {
    mutt_attach_bounce(shared->mailbox, pview->pdata->fp, pview->pdata->actx,
                       pview->pdata->body);
  }
  else
  {
    struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
    emaillist_add_email(&el, shared->email);
    ci_bounce_message(shared->mailbox, &el);
    emaillist_clear(&el);
  }
  return IR_SUCCESS;
}

/**
 * op_check_stats - calculate message statistics for all mailboxes - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_check_stats(struct IndexSharedData *shared,
                          struct PagerPrivateData *priv, int op)
{
  mutt_check_stats(shared->mailbox);
  return IR_SUCCESS;
}

/**
 * op_check_traditional - check for classic PGP - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_check_traditional(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (!(WithCrypto & APPLICATION_PGP))
    return IR_NO_ACTION;
  if (!(shared->email->security & PGP_TRADITIONAL_CHECKED))
  {
    priv->rc = OP_CHECK_TRADITIONAL;
    return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_compose_to_sender - compose new message to the current message sender - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_compose_to_sender(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH_E)))
  {
    return IR_NOT_IMPL;
  }
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;
  if (pview->mode == PAGER_MODE_ATTACH_E)
  {
    mutt_attach_mail_sender(pview->pdata->actx, pview->pdata->body);
  }
  else
  {
    struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
    emaillist_add_email(&el, shared->email);

    mutt_send_message(SEND_TO_SENDER, NULL, NULL, shared->mailbox, &el, NeoMutt->sub);
    emaillist_clear(&el);
  }
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_copy_message - copy a message to a file/mailbox - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_copy_message(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  if (!(WithCrypto != 0) && (op == OP_DECRYPT_COPY))
    return IR_DONE;

  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
  emaillist_add_email(&el, shared->email);

  const enum MessageSaveOpt save_opt =
      ((op == OP_SAVE) || (op == OP_DECODE_SAVE) || (op == OP_DECRYPT_SAVE)) ? SAVE_MOVE : SAVE_COPY;

  enum MessageTransformOpt transform_opt =
      ((op == OP_DECODE_SAVE) || (op == OP_DECODE_COPY))   ? TRANSFORM_DECODE :
      ((op == OP_DECRYPT_SAVE) || (op == OP_DECRYPT_COPY)) ? TRANSFORM_DECRYPT :
                                                             TRANSFORM_NONE;

  const int rc2 = mutt_save_message(shared->mailbox, &el, save_opt, transform_opt);
  if ((rc2 == 0) && (save_opt == SAVE_MOVE))
  {
    const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
    if (c_resolve)
    {
      op = -1;
      priv->rc = OP_MAIN_NEXT_UNDELETED;
    }
    else
      pager_queue_redraw(priv, MENU_REDRAW_INDEX);
  }
  emaillist_clear(&el);
  return IR_SUCCESS;
}

/**
 * op_create_alias - create an alias from a message sender - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_create_alias(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH_E)))
  {
    return IR_NOT_IMPL;
  }
  struct AddressList *al = NULL;
  if (pview->mode == PAGER_MODE_ATTACH_E)
    al = mutt_get_address(pview->pdata->body->email->env, NULL);
  else
    al = mutt_get_address(shared->email->env, NULL);
  alias_create(al, NeoMutt->sub);
  return IR_SUCCESS;
}

/**
 * op_delete - delete the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_delete(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (!assert_mailbox_writable(shared->mailbox))
    return IR_NO_ACTION;
  /* L10N: CHECK_ACL */
  if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_DELETE, _("Can't delete message")))
  {
    return IR_ERROR;
  }

  mutt_set_flag(shared->mailbox, shared->email, MUTT_DELETE, true);
  mutt_set_flag(shared->mailbox, shared->email, MUTT_PURGE, (op == OP_PURGE_MESSAGE));
  const bool c_delete_untag = cs_subset_bool(NeoMutt->sub, "delete_untag");
  if (c_delete_untag)
    mutt_set_flag(shared->mailbox, shared->email, MUTT_TAG, false);
  pager_queue_redraw(priv, MENU_REDRAW_INDEX);
  const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
  if (c_resolve)
  {
    priv->rc = OP_MAIN_NEXT_UNDELETED;
    return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_delete_thread - delete all messages in thread - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_delete_thread(struct IndexSharedData *shared,
                            struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (!assert_mailbox_writable(shared->mailbox))
    return IR_NO_ACTION;
  /* L10N: CHECK_ACL */
  /* L10N: Due to the implementation details we do not know whether we
     delete zero, 1, 12, ... messages. So in English we use
     "messages". Your language might have other means to express this.  */
  if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_DELETE, _("Can't delete messages")))
  {
    return IR_ERROR;
  }

  int subthread = (op == OP_DELETE_SUBTHREAD);
  int r = mutt_thread_set_flag(shared->mailbox, shared->email, MUTT_DELETE, 1, subthread);
  if (r == -1)
    return IR_ERROR;
  if (op == OP_PURGE_THREAD)
  {
    r = mutt_thread_set_flag(shared->mailbox, shared->email, MUTT_PURGE, true, subthread);
    if (r == -1)
      return IR_ERROR;
  }

  const bool c_delete_untag = cs_subset_bool(NeoMutt->sub, "delete_untag");
  if (c_delete_untag)
    mutt_thread_set_flag(shared->mailbox, shared->email, MUTT_TAG, 0, subthread);
  const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
  if (c_resolve)
  {
    priv->rc = OP_MAIN_NEXT_UNDELETED;
    return IR_DONE;
  }

  if (!c_resolve && (cs_subset_number(NeoMutt->sub, "pager_index_lines") != 0))
    pager_queue_redraw(priv, MENU_REDRAW_FULL);
  else
    pager_queue_redraw(priv, MENU_REDRAW_INDEX);

  return IR_SUCCESS;
}

/**
 * op_display_address - display full address of sender - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_display_address(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH_E)))
  {
    return IR_NOT_IMPL;
  }
  if (pview->mode == PAGER_MODE_ATTACH_E)
    mutt_display_address(pview->pdata->body->email->env);
  else
    mutt_display_address(shared->email->env);
  return IR_SUCCESS;
}

/**
 * op_edit_label - add, change, or delete a message's label - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_edit_label(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;

  struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
  emaillist_add_email(&el, shared->email);
  priv->rc = mutt_label_message(shared->mailbox, &el);
  emaillist_clear(&el);

  if (priv->rc > 0)
  {
    shared->mailbox->changed = true;
    pager_queue_redraw(priv, MENU_REDRAW_FULL);
    mutt_message(ngettext("%d label changed", "%d labels changed", priv->rc), priv->rc);
  }
  else
  {
    mutt_message(_("No labels changed"));
  }
  return IR_SUCCESS;
}

/**
 * op_enter_command - enter a neomuttrc command - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_enter_command(struct IndexSharedData *shared,
                            struct PagerPrivateData *priv, int op)
{
  mutt_enter_command();
  pager_queue_redraw(priv, MENU_REDRAW_FULL);

  if (OptNeedResort)
  {
    OptNeedResort = false;
    if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
      return IR_NOT_IMPL;
    OptNeedResort = true;
  }

  if ((priv->redraw & MENU_REDRAW_FLOW) && (priv->pview->flags & MUTT_PAGER_RETWINCH))
  {
    priv->rc = OP_REFORMAT_WINCH;
    return IR_DONE;
  }

  return IR_SUCCESS;
}

/**
 * op_exit - exit this menu - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_exit(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  priv->rc = -1;
  return IR_DONE;
}

/**
 * op_extract_keys - extract supported public keys - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_extract_keys(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  if (!WithCrypto)
    return IR_DONE;

  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;

  struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
  emaillist_add_email(&el, shared->email);
  crypt_extract_keys_from_messages(shared->mailbox, &el);
  emaillist_clear(&el);
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_flag_message - toggle a message's 'important' flag - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_flag_message(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (!assert_mailbox_writable(shared->mailbox))
    return IR_NO_ACTION;
  /* L10N: CHECK_ACL */
  if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_WRITE, "Can't flag message"))
    return IR_ERROR;

  mutt_set_flag(shared->mailbox, shared->email, MUTT_FLAG, !shared->email->flagged);
  pager_queue_redraw(priv, MENU_REDRAW_INDEX);
  const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
  if (c_resolve)
  {
    priv->rc = OP_MAIN_NEXT_UNDELETED;
    return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_forget_passphrase - wipe passphrases from memory - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_forget_passphrase(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  crypt_forget_passphrase();
  return IR_SUCCESS;
}

/**
 * op_forward_message - forward a message with comments - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_forward_message(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH_E)))
  {
    return IR_NOT_IMPL;
  }
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;
  if (pview->mode == PAGER_MODE_ATTACH_E)
  {
    mutt_attach_forward(pview->pdata->fp, shared->email, pview->pdata->actx,
                        pview->pdata->body, SEND_NO_FLAGS);
  }
  else
  {
    struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
    emaillist_add_email(&el, shared->email);

    mutt_send_message(SEND_FORWARD, NULL, NULL, shared->mailbox, &el, NeoMutt->sub);
    emaillist_clear(&el);
  }
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_half_down - scroll down 1/2 page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_half_down(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  const bool c_pager_stop = cs_subset_bool(NeoMutt->sub, "pager_stop");
  if (priv->lines[priv->cur_line].offset < (priv->st.st_size - 1))
  {
    priv->top_line = up_n_lines(priv->pview->win_pager->state.rows / 2,
                                priv->lines, priv->cur_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else if (c_pager_stop)
  {
    /* emulate "less -q" and don't go on to the next message. */
    mutt_message(_("Bottom of message is shown"));
  }
  else
  {
    /* end of the current message, so display the next message. */
    priv->rc = OP_MAIN_NEXT_UNDELETED;
    return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_half_up - scroll up 1/2 page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_half_up(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (priv->top_line)
  {
    priv->top_line = up_n_lines(priv->pview->win_pager->state.rows / 2 +
                                    (priv->pview->win_pager->state.rows % 2),
                                priv->lines, priv->top_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else
    mutt_message(_("Top of message is shown"));
  return IR_SUCCESS;
}

/**
 * op_help - this screen - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_help(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (priv->pview->mode == PAGER_MODE_HELP)
  {
    /* don't let the user enter the help-menu from the help screen! */
    mutt_error(_("Help is currently being shown"));
    return IR_ERROR;
  }
  mutt_help(MENU_PAGER);
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_mail - compose a new mail message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_mail(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;

  mutt_send_message(SEND_NO_FLAGS, NULL, NULL, shared->mailbox, NULL, NeoMutt->sub);
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_mailbox_list - list mailboxes with new mail - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_mailbox_list(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  mutt_mailbox_list();
  return IR_SUCCESS;
}

/**
 * op_mail_key - mail a PGP public key - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_mail_key(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (!(WithCrypto & APPLICATION_PGP))
    return IR_DONE;
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;
  struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
  emaillist_add_email(&el, shared->email);

  mutt_send_message(SEND_KEY, NULL, NULL, shared->mailbox, &el, NeoMutt->sub);
  emaillist_clear(&el);
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_main_set_flag - set a status flag on a message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_main_set_flag(struct IndexSharedData *shared,
                            struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (!assert_mailbox_writable(shared->mailbox))
    return IR_NO_ACTION;

  struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
  emaillist_add_email(&el, shared->email);

  if (mutt_change_flag(shared->mailbox, &el, (op == OP_MAIN_SET_FLAG)) == 0)
    pager_queue_redraw(priv, MENU_REDRAW_INDEX);
  const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
  if (shared->email->deleted && c_resolve)
  {
    priv->rc = OP_MAIN_NEXT_UNDELETED;
    return IR_DONE;
  }
  emaillist_clear(&el);
  return IR_SUCCESS;
}

/**
 * op_next_line - scroll down one line - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_next_line(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (priv->lines[priv->cur_line].offset < (priv->st.st_size - 1))
  {
    priv->top_line++;
    if (priv->hide_quoted)
    {
      while ((priv->lines[priv->top_line].color == MT_COLOR_QUOTED) &&
             (priv->top_line < priv->lines_used))
      {
        priv->top_line++;
      }
    }
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else
  {
    mutt_message(_("Bottom of message is shown"));
  }
  return IR_SUCCESS;
}

/**
 * op_next_page - move to the next page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_next_page(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  const bool c_pager_stop = cs_subset_bool(NeoMutt->sub, "pager_stop");
  if (priv->lines[priv->cur_line].offset < (priv->st.st_size - 1))
  {
    const short c_pager_context =
        cs_subset_number(NeoMutt->sub, "pager_context");
    priv->top_line =
        up_n_lines(c_pager_context, priv->lines, priv->cur_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else if (c_pager_stop)
  {
    /* emulate "less -q" and don't go on to the next message. */
    mutt_message(_("Bottom of message is shown"));
  }
  else
  {
    /* end of the current message, so display the next message. */
    priv->rc = OP_MAIN_NEXT_UNDELETED;
    return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_pager_bottom - jump to the bottom of the message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_bottom(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  if (!jump_to_bottom(priv, priv->pview))
    mutt_message(_("Bottom of message is shown"));

  return IR_SUCCESS;
}

/**
 * op_pager_hide_quoted - toggle display of quoted text - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_hide_quoted(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  if (!priv->has_types)
    return IR_NO_ACTION;

  priv->hide_quoted ^= MUTT_HIDE;
  if (priv->hide_quoted && (priv->lines[priv->top_line].color == MT_COLOR_QUOTED))
  {
    priv->top_line = up_n_lines(1, priv->lines, priv->top_line, priv->hide_quoted);
  }
  else
  {
    pager_queue_redraw(priv, MENU_REDRAW_BODY);
  }
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return IR_SUCCESS;
}

/*
 * TODO - how about a simple API to send an email given from/to/subject/body?
 */
static void send_simple_email(struct Mailbox *m, const char *mailto, const char *body)
{
  struct Email *e = email_new();
  e->env = mutt_env_new();
  mutt_parse_mailto(e->env, NULL, mailto);
  e->body = mutt_body_new();
  char ctype[] = "text/plain";
  mutt_parse_content_type(ctype, e->body);
  char tempfile[PATH_MAX];
  mutt_mktemp(tempfile, sizeof(tempfile));
  FILE *fp = mutt_file_fopen(tempfile, "w+");
  fprintf(fp, "%s\n", body);
  mutt_file_fclose(&fp);
  e->body->unlink = true;
  e->body->filename = mutt_str_dup(tempfile);
  mutt_send_message(SEND_DRAFT_FILE, e, NULL, m, NULL, NeoMutt->sub);
}

/**
 * op_pager_list_subscribe - subscribe to a mailing list - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_list_subscribe(struct IndexSharedData *shared,
                                   struct PagerPrivateData *priv, int op)
{
  if (!(shared->email && shared->email->env))
  {
    return IR_NO_ACTION;
  }

  const char *mailto = shared->email->env->list_subscribe;
  if (!mailto)
  {
    mutt_warning(_("No List-Subscribe header found"));
    return IR_NO_ACTION;
  }

  send_simple_email(shared->mailbox, mailto, "subscribe");
  return IR_SUCCESS;
}

/**
 * op_pager_list_unsubscribe - unsubscribe from mailing list - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_list_unsubscribe(struct IndexSharedData *shared,
                                     struct PagerPrivateData *priv, int op)
{
  if (!(shared->email && shared->email->env))
  {
    return IR_NO_ACTION;
  }

  const char *mailto = shared->email->env->list_unsubscribe;
  if (!mailto)
  {
    mutt_warning(_("No List-Subscribe header found"));
    return IR_NO_ACTION;
  }

  send_simple_email(shared->mailbox, mailto, "unsubscribe");
  return IR_SUCCESS;
}

/**
 * op_pager_skip_headers - jump to first line after headers - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_skip_headers(struct IndexSharedData *shared,
                                 struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!priv->has_types)
    return IR_NO_ACTION;

  int dretval = 0;
  int new_topline = 0;

  while (((new_topline < priv->lines_used) ||
          (0 == (dretval = display_line(
                     priv->fp, &priv->bytes_read, &priv->lines, new_topline, &priv->lines_used,
                     &priv->lines_max, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                     &priv->quote_list, &priv->q_level, &priv->force_redraw,
                     &priv->search_re, priv->pview->win_pager)))) &&
         mutt_color_is_header(priv->lines[new_topline].color))
  {
    new_topline++;
  }

  if (dretval < 0)
  {
    /* L10N: Displayed if <skip-headers> is invoked in the pager, but
       there is no text past the headers.
       (I don't think this is actually possible in Mutt's code, but
       display some kind of message in case it somehow occurs.) */
    mutt_warning(_("No text past headers"));
    return IR_NO_ACTION;
  }
  priv->top_line = new_topline;
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return IR_SUCCESS;
}

/**
 * op_pager_skip_quoted - skip beyond quoted text - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_skip_quoted(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!priv->has_types)
    return IR_NO_ACTION;

  const short c_skip_quoted_context =
      cs_subset_number(NeoMutt->sub, "pager_skip_quoted_context");
  int dretval = 0;
  int new_topline = priv->top_line;
  int num_quoted = 0;

  /* In a header? Skip all the email headers, and done */
  if (mutt_color_is_header(priv->lines[new_topline].color))
  {
    while (((new_topline < priv->lines_used) ||
            (0 == (dretval = display_line(
                       priv->fp, &priv->bytes_read, &priv->lines, new_topline, &priv->lines_used,
                       &priv->lines_max, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                       &priv->quote_list, &priv->q_level, &priv->force_redraw,
                       &priv->search_re, priv->pview->win_pager)))) &&
           mutt_color_is_header(priv->lines[new_topline].color))
    {
      new_topline++;
    }
    priv->top_line = new_topline;
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
    return IR_SUCCESS;
  }

  /* Already in the body? Skip past previous "context" quoted lines */
  if (c_skip_quoted_context > 0)
  {
    while (((new_topline < priv->lines_used) ||
            (0 == (dretval = display_line(
                       priv->fp, &priv->bytes_read, &priv->lines, new_topline, &priv->lines_used,
                       &priv->lines_max, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                       &priv->quote_list, &priv->q_level, &priv->force_redraw,
                       &priv->search_re, priv->pview->win_pager)))) &&
           (priv->lines[new_topline].color == MT_COLOR_QUOTED))
    {
      new_topline++;
      num_quoted++;
    }

    if (dretval < 0)
    {
      mutt_error(_("No more unquoted text after quoted text"));
      return IR_NO_ACTION;
    }
  }

  if (num_quoted <= c_skip_quoted_context)
  {
    num_quoted = 0;

    while (((new_topline < priv->lines_used) ||
            (0 == (dretval = display_line(
                       priv->fp, &priv->bytes_read, &priv->lines, new_topline, &priv->lines_used,
                       &priv->lines_max, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                       &priv->quote_list, &priv->q_level, &priv->force_redraw,
                       &priv->search_re, priv->pview->win_pager)))) &&
           (priv->lines[new_topline].color != MT_COLOR_QUOTED))
    {
      new_topline++;
    }

    if (dretval < 0)
    {
      mutt_error(_("No more quoted text"));
      return IR_NO_ACTION;
    }

    while (((new_topline < priv->lines_used) ||
            (0 == (dretval = display_line(
                       priv->fp, &priv->bytes_read, &priv->lines, new_topline, &priv->lines_used,
                       &priv->lines_max, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                       &priv->quote_list, &priv->q_level, &priv->force_redraw,
                       &priv->search_re, priv->pview->win_pager)))) &&
           (priv->lines[new_topline].color == MT_COLOR_QUOTED))
    {
      new_topline++;
      num_quoted++;
    }

    if (dretval < 0)
    {
      mutt_error(_("No more unquoted text after quoted text"));
      return IR_NO_ACTION;
    }
  }
  priv->top_line = new_topline - MIN(c_skip_quoted_context, num_quoted);
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return IR_SUCCESS;
}

/**
 * op_pager_top - jump to the top of the message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_top(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (priv->top_line)
    priv->top_line = 0;
  else
    mutt_message(_("Top of message is shown"));
  return IR_SUCCESS;
}

/**
 * op_pipe - pipe message/attachment to a shell command - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pipe(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH)))
  {
    return IR_NOT_IMPL;
  }
  if (pview->mode == PAGER_MODE_ATTACH)
  {
    mutt_pipe_attachment_list(pview->pdata->actx, pview->pdata->fp, false,
                              pview->pdata->body, false);
  }
  else
  {
    struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
    el_add_tagged(&el, shared->ctx, shared->email, false);
    mutt_pipe_message(shared->mailbox, &el);
    emaillist_clear(&el);
  }
  return IR_SUCCESS;
}

/**
 * op_prev_line - scroll up one line - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_prev_line(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (priv->top_line)
  {
    priv->top_line = up_n_lines(1, priv->lines, priv->top_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else
  {
    mutt_message(_("Top of message is shown"));
  }
  return IR_SUCCESS;
}

/**
 * op_prev_page - move to the previous page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_prev_page(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (priv->top_line == 0)
  {
    mutt_message(_("Top of message is shown"));
  }
  else
  {
    const short c_pager_context =
        cs_subset_number(NeoMutt->sub, "pager_context");
    priv->top_line = up_n_lines(priv->pview->win_pager->state.rows - c_pager_context,
                                priv->lines, priv->top_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  return IR_SUCCESS;
}

/**
 * op_print - print the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_print(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH)))
  {
    return IR_NOT_IMPL;
  }
  if (pview->mode == PAGER_MODE_ATTACH)
  {
    mutt_print_attachment_list(pview->pdata->actx, pview->pdata->fp, false,
                               pview->pdata->body);
  }
  else
  {
    struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
    el_add_tagged(&el, shared->ctx, shared->email, false);
    mutt_print_message(shared->mailbox, &el);
    emaillist_clear(&el);
  }
  return IR_SUCCESS;
}

/**
 * op_quit - save changes to mailbox and quit - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_quit(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  const enum QuadOption c_quit = cs_subset_quad(NeoMutt->sub, "quit");
  if (query_quadoption(c_quit, _("Quit NeoMutt?")) == MUTT_YES)
  {
    /* avoid prompting again in the index menu */
    cs_subset_str_native_set(NeoMutt->sub, "quit", MUTT_YES, NULL);
    return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_recall_message - recall a postponed message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_recall_message(struct IndexSharedData *shared,
                             struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;
  struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
  emaillist_add_email(&el, shared->email);

  mutt_send_message(SEND_POSTPONED, NULL, NULL, shared->mailbox, &el, NeoMutt->sub);
  emaillist_clear(&el);
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_redraw - clear and redraw the screen - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_redraw(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  mutt_window_reflow(NULL);
  clearok(stdscr, true);
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_reply - reply to a message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_reply(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH_E)))
  {
    return IR_NOT_IMPL;
  }
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;

  SendFlags replyflags = SEND_REPLY;
  if (op == OP_GROUP_REPLY)
    replyflags |= SEND_GROUP_REPLY;
  else if (op == OP_GROUP_CHAT_REPLY)
    replyflags |= SEND_GROUP_CHAT_REPLY;
  else if (op == OP_LIST_REPLY)
    replyflags |= SEND_LIST_REPLY;

  if (pview->mode == PAGER_MODE_ATTACH_E)
  {
    mutt_attach_reply(pview->pdata->fp, shared->mailbox, shared->email,
                      pview->pdata->actx, pview->pdata->body, replyflags);
  }
  else
  {
    struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
    emaillist_add_email(&el, shared->email);
    mutt_send_message(replyflags, NULL, NULL, shared->mailbox, &el, NeoMutt->sub);
    emaillist_clear(&el);
  }
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_resend - use the current message as a template for a new one - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_resend(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH_E)))
  {
    return IR_NOT_IMPL;
  }
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;
  if (pview->mode == PAGER_MODE_ATTACH_E)
  {
    mutt_attach_resend(pview->pdata->fp, shared->mailbox, pview->pdata->actx,
                       pview->pdata->body);
  }
  else
  {
    mutt_resend_message(NULL, shared->mailbox, shared->email, NeoMutt->sub);
  }
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_save - save message/attachment to a mailbox/file - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_save(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if ((op == OP_DECRYPT_SAVE) && !WithCrypto)
    return IR_DONE;

  struct PagerView *pview = priv->pview;

  if (pview->mode == PAGER_MODE_ATTACH)
  {
    mutt_save_attachment_list(pview->pdata->actx, pview->pdata->fp, false,
                              pview->pdata->body, shared->email, NULL);
    return IR_SUCCESS;
  }

  return op_copy_message(shared, priv, op);
}

/**
 * op_search - search for a regular expression - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_search(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  char buf[1024] = { 0 };
  mutt_str_copy(buf, priv->search_str, sizeof(buf));
  if (mutt_get_field(
          ((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ? _("Search for: ") : _("Reverse search for: "),
          buf, sizeof(buf), MUTT_CLEAR | MUTT_PATTERN, false, NULL, NULL) != 0)
  {
    return IR_NO_ACTION;
  }

  if (strcmp(buf, priv->search_str) == 0)
  {
    if (priv->search_compiled)
    {
      /* do an implicit search-next */
      if (op == OP_SEARCH)
        op = OP_SEARCH_NEXT;
      else
        op = OP_SEARCH_OPPOSITE;

      priv->wrapped = false;
      op_search_next(shared, priv, op);
    }
  }

  if (buf[0] == '\0')
    return IR_NO_ACTION;

  mutt_str_copy(priv->search_str, buf, sizeof(priv->search_str));

  /* leave search_back alone if op == OP_SEARCH_NEXT */
  if (op == OP_SEARCH)
    priv->search_back = false;
  else if (op == OP_SEARCH_REVERSE)
    priv->search_back = true;

  if (priv->search_compiled)
  {
    regfree(&priv->search_re);
    for (size_t i = 0; i < priv->lines_used; i++)
    {
      FREE(&(priv->lines[i].search));
      priv->lines[i].search_arr_size = -1;
    }
  }

  uint16_t rflags = mutt_mb_is_lower(priv->search_str) ? REG_ICASE : 0;
  int err = REG_COMP(&priv->search_re, priv->search_str, REG_NEWLINE | rflags);
  if (err != 0)
  {
    regerror(err, &priv->search_re, buf, sizeof(buf));
    mutt_error("%s", buf);
    for (size_t i = 0; i < priv->lines_max; i++)
    {
      /* cleanup */
      FREE(&(priv->lines[i].search));
      priv->lines[i].search_arr_size = -1;
    }
    priv->search_flag = 0;
    priv->search_compiled = false;
  }
  else
  {
    priv->search_compiled = true;
    /* update the search pointers */
    int line_num = 0;
    while (display_line(priv->fp, &priv->bytes_read, &priv->lines, line_num,
                        &priv->lines_used, &priv->lines_max,
                        MUTT_SEARCH | (pview->flags & MUTT_PAGER_NSKIP) |
                            (pview->flags & MUTT_PAGER_NOWRAP),
                        &priv->quote_list, &priv->q_level, &priv->force_redraw,
                        &priv->search_re, priv->pview->win_pager) == 0)
    {
      line_num++;
    }

    if (!priv->search_back)
    {
      /* searching forward */
      int i;
      for (i = priv->top_line; i < priv->lines_used; i++)
      {
        if ((!priv->hide_quoted || (priv->lines[i].color != MT_COLOR_QUOTED)) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      if (i < priv->lines_used)
        priv->top_line = i;
    }
    else
    {
      /* searching backward */
      int i;
      for (i = priv->top_line; i >= 0; i--)
      {
        if ((!priv->hide_quoted || (priv->lines[i].color != MT_COLOR_QUOTED)) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      if (i >= 0)
        priv->top_line = i;
    }

    if (priv->lines[priv->top_line].search_arr_size == 0)
    {
      priv->search_flag = 0;
      mutt_error(_("Not found"));
    }
    else
    {
      const short c_search_context =
          cs_subset_number(NeoMutt->sub, "search_context");
      priv->search_flag = MUTT_SEARCH;
      /* give some context for search results */
      if (c_search_context < priv->pview->win_pager->state.rows)
        priv->searchctx = c_search_context;
      else
        priv->searchctx = 0;
      if (priv->top_line - priv->searchctx > 0)
        priv->top_line -= priv->searchctx;
    }
  }
  pager_queue_redraw(priv, MENU_REDRAW_BODY);
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return IR_SUCCESS;
}

/**
 * op_search_next - search for next match - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_search_next(struct IndexSharedData *shared,
                          struct PagerPrivateData *priv, int op)
{
  if (priv->search_compiled)
  {
    const short c_search_context =
        cs_subset_number(NeoMutt->sub, "search_context");
    priv->wrapped = false;

    if (c_search_context < priv->pview->win_pager->state.rows)
      priv->searchctx = c_search_context;
    else
      priv->searchctx = 0;

  search_next:
    if ((!priv->search_back && (op == OP_SEARCH_NEXT)) ||
        (priv->search_back && (op == OP_SEARCH_OPPOSITE)))
    {
      /* searching forward */
      int i;
      for (i = priv->wrapped ? 0 : priv->top_line + priv->searchctx + 1;
           i < priv->lines_used; i++)
      {
        if ((!priv->hide_quoted || (priv->lines[i].color != MT_COLOR_QUOTED)) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      const bool c_wrap_search = cs_subset_bool(NeoMutt->sub, "wrap_search");
      if (i < priv->lines_used)
        priv->top_line = i;
      else if (priv->wrapped || !c_wrap_search)
        mutt_error(_("Not found"));
      else
      {
        mutt_message(_("Search wrapped to top"));
        priv->wrapped = true;
        goto search_next;
      }
    }
    else
    {
      /* searching backward */
      int i;
      for (i = priv->wrapped ? priv->lines_used : priv->top_line + priv->searchctx - 1;
           i >= 0; i--)
      {
        if ((!priv->hide_quoted ||
             (priv->has_types && (priv->lines[i].color != MT_COLOR_QUOTED))) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      const bool c_wrap_search = cs_subset_bool(NeoMutt->sub, "wrap_search");
      if (i >= 0)
        priv->top_line = i;
      else if (priv->wrapped || !c_wrap_search)
        mutt_error(_("Not found"));
      else
      {
        mutt_message(_("Search wrapped to bottom"));
        priv->wrapped = true;
        goto search_next;
      }
    }

    if (priv->lines[priv->top_line].search_arr_size > 0)
    {
      priv->search_flag = MUTT_SEARCH;
      /* give some context for search results */
      if (priv->top_line - priv->searchctx > 0)
        priv->top_line -= priv->searchctx;
    }

    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
    return IR_SUCCESS;
  }

  /* no previous search pattern */
  return op_search(shared, priv, op);
}

/**
 * op_search_toggle - toggle search pattern coloring - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_search_toggle(struct IndexSharedData *shared,
                            struct PagerPrivateData *priv, int op)
{
  if (priv->search_compiled)
  {
    priv->search_flag ^= MUTT_SEARCH;
    pager_queue_redraw(priv, MENU_REDRAW_BODY);
  }
  return IR_SUCCESS;
}

/**
 * op_shell_escape - invoke a command in a subshell - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_shell_escape(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  if (mutt_shell_escape())
  {
    mutt_mailbox_check(shared->mailbox, MUTT_MAILBOX_CHECK_FORCE);
  }
  return IR_SUCCESS;
}

/**
 * op_sort - sort messages - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_sort(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (mutt_select_sort(op == OP_SORT_REVERSE))
  {
    OptNeedResort = true;
    priv->rc = OP_DISPLAY_MESSAGE;
    return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_tag - tag the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_tag(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  mutt_set_flag(shared->mailbox, shared->email, MUTT_TAG, !shared->email->tagged);

  pager_queue_redraw(priv, MENU_REDRAW_INDEX);
  const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
  if (c_resolve)
  {
    priv->rc = OP_NEXT_ENTRY;
    return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_toggle_new - toggle a message's 'new' flag - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_toggle_new(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (!assert_mailbox_writable(shared->mailbox))
    return IR_NO_ACTION;
  /* L10N: CHECK_ACL */
  if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_SEEN, _("Can't toggle new")))
    return IR_ERROR;

  if (shared->email->read || shared->email->old)
    mutt_set_flag(shared->mailbox, shared->email, MUTT_NEW, true);
  else if (!priv->first || (priv->delay_read_timestamp != 0))
    mutt_set_flag(shared->mailbox, shared->email, MUTT_READ, true);
  priv->delay_read_timestamp = 0;
  priv->first = false;
  shared->ctx->msg_in_pager = -1;
  priv->win_pbar->actions |= WA_RECALC;
  pager_queue_redraw(priv, MENU_REDRAW_INDEX);
  const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
  if (c_resolve)
  {
    priv->rc = OP_MAIN_NEXT_UNDELETED;
    return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_undelete - undelete the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_undelete(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (!assert_mailbox_writable(shared->mailbox))
    return IR_NO_ACTION;
  /* L10N: CHECK_ACL */
  if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_DELETE, _("Can't undelete message")))
  {
    return IR_ERROR;
  }

  mutt_set_flag(shared->mailbox, shared->email, MUTT_DELETE, false);
  mutt_set_flag(shared->mailbox, shared->email, MUTT_PURGE, false);
  pager_queue_redraw(priv, MENU_REDRAW_INDEX);
  const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
  if (c_resolve)
  {
    priv->rc = OP_NEXT_ENTRY;
    return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_undelete_thread - undelete all messages in thread - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_undelete_thread(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (!assert_mailbox_writable(shared->mailbox))
    return IR_NO_ACTION;
  /* L10N: CHECK_ACL */
  /* L10N: Due to the implementation details we do not know whether we
     undelete zero, 1, 12, ... messages. So in English we use
     "messages". Your language might have other means to express this. */
  if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_DELETE, _("Can't undelete messages")))
  {
    return IR_ERROR;
  }

  int r = mutt_thread_set_flag(shared->mailbox, shared->email, MUTT_DELETE,
                               false, (op != OP_UNDELETE_THREAD));
  if (r != -1)
  {
    r = mutt_thread_set_flag(shared->mailbox, shared->email, MUTT_PURGE, false,
                             (op != OP_UNDELETE_THREAD));
  }
  if (r != -1)
  {
    const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
    if (c_resolve)
    {
      priv->rc = (op == OP_DELETE_THREAD) ? OP_MAIN_NEXT_THREAD : OP_MAIN_NEXT_SUBTHREAD;
    }

    if (!c_resolve && (cs_subset_number(NeoMutt->sub, "pager_index_lines") != 0))
      pager_queue_redraw(priv, MENU_REDRAW_FULL);
    else
      pager_queue_redraw(priv, MENU_REDRAW_INDEX);

    if (c_resolve)
      return IR_DONE;
  }
  return IR_SUCCESS;
}

/**
 * op_version - show the NeoMutt version number and date - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_version(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  mutt_message(mutt_make_version());
  return IR_SUCCESS;
}

/**
 * op_view_attachments - show MIME attachments - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_view_attachments(struct IndexSharedData *shared,
                               struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (pview->flags & MUTT_PAGER_ATTACHMENT)
  {
    priv->rc = OP_ATTACH_COLLAPSE;
    return IR_DONE;
  }

  if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  dlg_select_attachment(NeoMutt->sub, shared->mailbox, shared->email,
                        pview->pdata->fp);
  if (shared->email->attach_del)
    shared->mailbox->changed = true;
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_what_key - display the keycode for a key press - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_what_key(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  mutt_what_key();
  return IR_SUCCESS;
}

// -----------------------------------------------------------------------------

#ifdef USE_NNTP
/**
 * op_followup - followup to newsgroup - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_followup(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH_E)))
  {
    return IR_NOT_IMPL;
  }
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;

  char *followup_to = NULL;
  if (pview->mode == PAGER_MODE_ATTACH_E)
    followup_to = pview->pdata->body->email->env->followup_to;
  else
    followup_to = shared->email->env->followup_to;

  const enum QuadOption c_followup_to_poster =
      cs_subset_quad(NeoMutt->sub, "followup_to_poster");
  if (!followup_to || !mutt_istr_equal(followup_to, "poster") ||
      (query_quadoption(c_followup_to_poster,
                        _("Reply by mail as poster prefers?")) != MUTT_YES))
  {
    const enum QuadOption c_post_moderated =
        cs_subset_quad(NeoMutt->sub, "post_moderated");
    if ((shared->mailbox->type == MUTT_NNTP) &&
        !((struct NntpMboxData *) shared->mailbox->mdata)->allowed && (query_quadoption(c_post_moderated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
    {
      return IR_ERROR;
    }
    if (pview->mode == PAGER_MODE_ATTACH_E)
    {
      mutt_attach_reply(pview->pdata->fp, shared->mailbox, shared->email,
                        pview->pdata->actx, pview->pdata->body, SEND_NEWS | SEND_REPLY);
    }
    else
    {
      struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
      emaillist_add_email(&el, shared->email);
      mutt_send_message(SEND_NEWS | SEND_REPLY, NULL, NULL, shared->mailbox,
                        &el, NeoMutt->sub);
      emaillist_clear(&el);
    }
    pager_queue_redraw(priv, MENU_REDRAW_FULL);
    return IR_SUCCESS;
  }

  return op_reply(shared, priv, op);
}

/**
 * op_forward_to_group - forward to newsgroup - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_forward_to_group(struct IndexSharedData *shared,
                               struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) || (pview->mode == PAGER_MODE_ATTACH_E)))
  {
    return IR_NOT_IMPL;
  }
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;
  const enum QuadOption c_post_moderated =
      cs_subset_quad(NeoMutt->sub, "post_moderated");
  if ((shared->mailbox->type == MUTT_NNTP) &&
      !((struct NntpMboxData *) shared->mailbox->mdata)->allowed && (query_quadoption(c_post_moderated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
  {
    return IR_ERROR;
  }
  if (pview->mode == PAGER_MODE_ATTACH_E)
  {
    mutt_attach_forward(pview->pdata->fp, shared->email, pview->pdata->actx,
                        pview->pdata->body, SEND_NEWS);
  }
  else
  {
    struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
    emaillist_add_email(&el, shared->email);

    mutt_send_message(SEND_NEWS | SEND_FORWARD, NULL, NULL, shared->mailbox,
                      &el, NeoMutt->sub);
    emaillist_clear(&el);
  }
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}

/**
 * op_post - post message to newsgroup - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_post(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (!assert_pager_mode(priv->pview->mode == PAGER_MODE_EMAIL))
    return IR_NOT_IMPL;
  if (assert_attach_msg_mode(OptAttachMsg))
    return IR_ERROR;
  const enum QuadOption c_post_moderated =
      cs_subset_quad(NeoMutt->sub, "post_moderated");
  if ((shared->mailbox->type == MUTT_NNTP) &&
      !((struct NntpMboxData *) shared->mailbox->mdata)->allowed && (query_quadoption(c_post_moderated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
  {
    return IR_ERROR;
  }

  mutt_send_message(SEND_NEWS, NULL, NULL, shared->mailbox, NULL, NeoMutt->sub);
  pager_queue_redraw(priv, MENU_REDRAW_FULL);
  return IR_SUCCESS;
}
#endif

#ifdef USE_SIDEBAR
/**
 * op_sidebar_move - move the sidebar highlight - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_sidebar_move(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  struct MuttWindow *dlg = dialog_find(priv->pview->win_pager);
  struct MuttWindow *win_sidebar = window_find_child(dlg, WT_SIDEBAR);
  if (!win_sidebar)
    return IR_NO_ACTION;
  sb_change_mailbox(win_sidebar, op);
  return IR_SUCCESS;
}

/**
 * op_sidebar_toggle_visible - make the sidebar (in)visible - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_sidebar_toggle_visible(struct IndexSharedData *shared,
                                     struct PagerPrivateData *priv, int op)
{
  bool_str_toggle(NeoMutt->sub, "sidebar_visible", NULL);
  struct MuttWindow *dlg = dialog_find(priv->pview->win_pager);
  mutt_window_reflow(dlg);
  return IR_SUCCESS;
}
#endif

// -----------------------------------------------------------------------------

/**
 * PagerFunctions - All the NeoMutt functions that the Pager supports
 */
struct PagerFunction PagerFunctions[] = {
  // clang-format off
  { OP_BOUNCE_MESSAGE,         op_bounce_message },
  { OP_CHECK_STATS,            op_check_stats },
  { OP_CHECK_TRADITIONAL,      op_check_traditional },
  { OP_COMPOSE_TO_SENDER,      op_compose_to_sender },
  { OP_COPY_MESSAGE,           op_copy_message },
  { OP_DECODE_COPY,            op_copy_message },
  { OP_DECODE_SAVE,            op_copy_message },
  { OP_DECRYPT_COPY,           op_copy_message },
  { OP_CREATE_ALIAS,           op_create_alias },
  { OP_DECRYPT_SAVE,           op_save },
  { OP_DELETE,                 op_delete },
  { OP_PURGE_MESSAGE,          op_delete },
  { OP_DELETE_SUBTHREAD,       op_delete_thread },
  { OP_DELETE_THREAD,          op_delete_thread },
  { OP_PURGE_THREAD,           op_delete_thread },
  { OP_DISPLAY_ADDRESS,        op_display_address },
  { OP_EDIT_LABEL,             op_edit_label },
  { OP_ENTER_COMMAND,          op_enter_command },
  { OP_EXIT,                   op_exit },
  { OP_EXTRACT_KEYS,           op_extract_keys },
  { OP_FLAG_MESSAGE,           op_flag_message },
#ifdef USE_NNTP
  { OP_FOLLOWUP,               op_followup },
#endif
  { OP_FORGET_PASSPHRASE,      op_forget_passphrase },
  { OP_FORWARD_MESSAGE,        op_forward_message },
#ifdef USE_NNTP
  { OP_FORWARD_TO_GROUP,       op_forward_to_group },
#endif
  { OP_HALF_DOWN,              op_half_down },
  { OP_HALF_UP,                op_half_up },
  { OP_HELP,                   op_help },
  { OP_MAIL,                   op_mail },
  { OP_MAILBOX_LIST,           op_mailbox_list },
  { OP_MAIL_KEY,               op_mail_key },
  { OP_MAIN_CLEAR_FLAG,        op_main_set_flag },
  { OP_MAIN_SET_FLAG,          op_main_set_flag },
  { OP_NEXT_LINE,              op_next_line },
  { OP_NEXT_PAGE,              op_next_page },
  { OP_PAGER_BOTTOM,           op_pager_bottom },
  { OP_PAGER_HIDE_QUOTED,      op_pager_hide_quoted },
  { OP_PAGER_LIST_SUBSCRIBE,   op_pager_list_subscribe },
  { OP_PAGER_LIST_UNSUBSCRIBE, op_pager_list_unsubscribe },
  { OP_PAGER_SKIP_HEADERS,     op_pager_skip_headers },
  { OP_PAGER_SKIP_QUOTED,      op_pager_skip_quoted },
  { OP_PAGER_TOP,              op_pager_top },
  { OP_PIPE,                   op_pipe },
#ifdef USE_NNTP
  { OP_POST,                   op_post },
#endif
  { OP_PREV_LINE,              op_prev_line },
  { OP_PREV_PAGE,              op_prev_page },
  { OP_PRINT,                  op_print },
  { OP_QUIT,                   op_quit },
  { OP_RECALL_MESSAGE,         op_recall_message },
  { OP_REDRAW,                 op_redraw },
  { OP_GROUP_CHAT_REPLY,       op_reply },
  { OP_GROUP_REPLY,            op_reply },
  { OP_LIST_REPLY,             op_reply },
  { OP_REPLY,                  op_reply },
  { OP_RESEND,                 op_resend },
  { OP_SAVE,                   op_save },
  { OP_SEARCH,                 op_search },
  { OP_SEARCH_REVERSE,         op_search },
  { OP_SEARCH_NEXT,            op_search_next },
  { OP_SEARCH_OPPOSITE,        op_search_next },
  { OP_SEARCH_TOGGLE,          op_search_toggle },
  { OP_SHELL_ESCAPE,           op_shell_escape },
  { OP_SIDEBAR_FIRST,          op_sidebar_move },
  { OP_SIDEBAR_LAST,           op_sidebar_move },
  { OP_SIDEBAR_NEXT,           op_sidebar_move },
  { OP_SIDEBAR_NEXT_NEW,       op_sidebar_move },
  { OP_SIDEBAR_PAGE_DOWN,      op_sidebar_move },
  { OP_SIDEBAR_PAGE_UP,        op_sidebar_move },
  { OP_SIDEBAR_PREV,           op_sidebar_move },
  { OP_SIDEBAR_PREV_NEW,       op_sidebar_move },
  { OP_SIDEBAR_TOGGLE_VISIBLE, op_sidebar_toggle_visible },
  { OP_SORT,                   op_sort },
  { OP_SORT_REVERSE,           op_sort },
  { OP_TAG,                    op_tag },
  { OP_TOGGLE_NEW,             op_toggle_new },
  { OP_UNDELETE,               op_undelete },
  { OP_UNDELETE_SUBTHREAD,     op_undelete_thread },
  { OP_UNDELETE_THREAD,        op_undelete_thread },
  { OP_VERSION,                op_version },
  { OP_VIEW_ATTACHMENTS,       op_view_attachments },
  { OP_WHAT_KEY,               op_what_key },
  { 0, NULL },
  // clang-format on
};

/**
 * pager_function_dispatcher - Perform a Pager function
 * @param win_pager Window for the Index
 * @param op        Operation to perform, e.g. OP_MAIN_LIMIT
 * @retval num #IndexRetval, e.g. #IR_SUCCESS
 */
int pager_function_dispatcher(struct MuttWindow *win_pager, int op)
{
  if (!win_pager)
  {
    mutt_error(_(Not_available_in_this_menu));
    return IR_ERROR;
  }

  struct PagerPrivateData *priv = win_pager->parent->wdata;
  if (!priv)
    return IR_ERROR;

  struct MuttWindow *dlg = dialog_find(win_pager);
  if (!dlg || !dlg->wdata)
    return IR_ERROR;

  int rc = IR_UNKNOWN;
  for (size_t i = 0; PagerFunctions[i].op != OP_NULL; i++)
  {
    const struct PagerFunction *fn = &PagerFunctions[i];
    if (fn->op == op)
    {
      struct IndexSharedData *shared = dlg->wdata;
      rc = fn->function(shared, priv, op);
      break;
    }
  }

  return rc;
}
