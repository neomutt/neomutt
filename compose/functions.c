/**
 * @file
 * Compose functions
 *
 * @authors
 * Copyright (C) 2021 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 David Purton <dcpurton@marshwiggle.net>
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
 * @page compose_functions Compose functions
 *
 * Compose functions
 */

#include "config.h"
#ifdef _MAKEDOC
#include "docs/makedoc_defs.h"
#else
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "attach/lib.h"
#include "browser/lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "imap/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "nntp/lib.h"
#include "pop/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "attach_data.h"
#include "external.h"
#include "functions.h"
#include "globals.h"
#include "hook.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "mview.h"
#include "mx.h"
#include "nntp/adata.h" // IWYU pragma: keep
#include "protos.h"
#include "rfc3676.h"
#include "shared_data.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#endif

// clang-format off
/**
 * OpCompose - Functions for the Compose Menu
 */
const struct MenuFuncOp OpCompose[] = { /* map: compose */
  { "attach-file",                   OP_ATTACHMENT_ATTACH_FILE },
  { "attach-key",                    OP_ATTACHMENT_ATTACH_KEY },
  { "attach-message",                OP_ATTACHMENT_ATTACH_MESSAGE },
  { "attach-news-message",           OP_ATTACHMENT_ATTACH_NEWS_MESSAGE },
#ifdef USE_AUTOCRYPT
  { "autocrypt-menu",                OP_COMPOSE_AUTOCRYPT_MENU },
#endif
  { "copy-file",                     OP_ATTACHMENT_SAVE },
  { "detach-file",                   OP_ATTACHMENT_DETACH },
  { "display-toggle-weed",           OP_DISPLAY_HEADERS },
  { "edit-bcc",                      OP_ENVELOPE_EDIT_BCC },
  { "edit-cc",                       OP_ENVELOPE_EDIT_CC },
  { "edit-content-id",               OP_ATTACHMENT_EDIT_CONTENT_ID },
  { "edit-description",              OP_ATTACHMENT_EDIT_DESCRIPTION },
  { "edit-encoding",                 OP_ATTACHMENT_EDIT_ENCODING },
  { "edit-fcc",                      OP_ENVELOPE_EDIT_FCC },
  { "edit-file",                     OP_COMPOSE_EDIT_FILE },
  { "edit-followup-to",              OP_ENVELOPE_EDIT_FOLLOWUP_TO },
  { "edit-from",                     OP_ENVELOPE_EDIT_FROM },
  { "edit-headers",                  OP_ENVELOPE_EDIT_HEADERS },
  { "edit-language",                 OP_ATTACHMENT_EDIT_LANGUAGE },
  { "edit-message",                  OP_COMPOSE_EDIT_MESSAGE },
  { "edit-mime",                     OP_ATTACHMENT_EDIT_MIME },
  { "edit-newsgroups",               OP_ENVELOPE_EDIT_NEWSGROUPS },
  { "edit-reply-to",                 OP_ENVELOPE_EDIT_REPLY_TO },
  { "edit-subject",                  OP_ENVELOPE_EDIT_SUBJECT },
  { "edit-to",                       OP_ENVELOPE_EDIT_TO },
  { "edit-type",                     OP_ATTACHMENT_EDIT_TYPE },
  { "edit-x-comment-to",             OP_ENVELOPE_EDIT_X_COMMENT_TO },
  { "exit",                          OP_EXIT },
  { "filter-entry",                  OP_ATTACHMENT_FILTER },
  { "forget-passphrase",             OP_FORGET_PASSPHRASE },
  { "get-attachment",                OP_ATTACHMENT_GET_ATTACHMENT },
  { "group-alternatives",            OP_ATTACHMENT_GROUP_ALTS },
  { "group-multilingual",            OP_ATTACHMENT_GROUP_LINGUAL },
  { "group-related",                 OP_ATTACHMENT_GROUP_RELATED },
  { "ispell",                        OP_COMPOSE_ISPELL },
  { "move-down",                     OP_ATTACHMENT_MOVE_DOWN },
  { "move-up",                       OP_ATTACHMENT_MOVE_UP },
  { "new-mime",                      OP_ATTACHMENT_NEW_MIME },
  { "pgp-menu",                      OP_COMPOSE_PGP_MENU },
  { "pipe-entry",                    OP_PIPE },
  { "pipe-message",                  OP_PIPE },
  { "postpone-message",              OP_COMPOSE_POSTPONE_MESSAGE },
  { "print-entry",                   OP_ATTACHMENT_PRINT },
  { "rename-attachment",             OP_ATTACHMENT_RENAME_ATTACHMENT },
  { "rename-file",                   OP_COMPOSE_RENAME_FILE },
  { "send-message",                  OP_COMPOSE_SEND_MESSAGE },
  { "smime-menu",                    OP_COMPOSE_SMIME_MENU },
  { "toggle-disposition",            OP_ATTACHMENT_TOGGLE_DISPOSITION },
  { "toggle-recode",                 OP_ATTACHMENT_TOGGLE_RECODE },
  { "toggle-unlink",                 OP_ATTACHMENT_TOGGLE_UNLINK },
  { "ungroup-attachment",            OP_ATTACHMENT_UNGROUP },
  { "update-encoding",               OP_ATTACHMENT_UPDATE_ENCODING },
  { "view-attach",                   OP_ATTACHMENT_VIEW },
  { "view-mailcap",                  OP_ATTACHMENT_VIEW_MAILCAP },
  { "view-pager",                    OP_ATTACHMENT_VIEW_PAGER },
  { "view-text",                     OP_ATTACHMENT_VIEW_TEXT },
  { "write-fcc",                     OP_COMPOSE_WRITE_MESSAGE },
  { NULL, 0 },
};

/**
 * ComposeDefaultBindings - Key bindings for the Compose Menu
 */
const struct MenuOpSeq ComposeDefaultBindings[] = { /* map: compose */
  { OP_ATTACHMENT_ATTACH_FILE,             "a" },
  { OP_ATTACHMENT_ATTACH_KEY,              "\033k" },          // <Alt-k>
  { OP_ATTACHMENT_ATTACH_MESSAGE,          "A" },
  { OP_ATTACHMENT_DETACH,                  "D" },
  { OP_ATTACHMENT_EDIT_CONTENT_ID,         "\033i" },          // <Alt-i>
  { OP_ATTACHMENT_EDIT_DESCRIPTION,        "d" },
  { OP_ATTACHMENT_EDIT_ENCODING,           "\005" },           // <Ctrl-E>
  { OP_ATTACHMENT_EDIT_LANGUAGE,           "\014" },           // <Ctrl-L>
  { OP_ATTACHMENT_EDIT_MIME,               "m" },
  { OP_ATTACHMENT_EDIT_TYPE,               "\024" },           // <Ctrl-T>
  { OP_ATTACHMENT_FILTER,                  "F" },
  { OP_ATTACHMENT_GET_ATTACHMENT,          "G" },
  { OP_ATTACHMENT_GROUP_ALTS,              "&" },
  { OP_ATTACHMENT_GROUP_LINGUAL,           "^" },
  { OP_ATTACHMENT_GROUP_RELATED,           "%" },
  { OP_ATTACHMENT_MOVE_DOWN,               "+" },
  { OP_ATTACHMENT_MOVE_UP,                 "-" },
  { OP_ATTACHMENT_NEW_MIME,                "n" },
  { OP_EXIT,                               "q" },
  { OP_PIPE,                               "|" },
  { OP_ATTACHMENT_PRINT,                   "l" },
  { OP_ATTACHMENT_RENAME_ATTACHMENT,       "\017" },           // <Ctrl-O>
  { OP_ATTACHMENT_SAVE,                    "C" },
  { OP_ATTACHMENT_TOGGLE_DISPOSITION,      "\004" },           // <Ctrl-D>
  { OP_ATTACHMENT_TOGGLE_UNLINK,           "u" },
  { OP_ATTACHMENT_UNGROUP,                 "#" },
  { OP_ATTACHMENT_UPDATE_ENCODING,         "U" },
  { OP_ATTACHMENT_VIEW,                    "<keypadenter>" },
  { OP_ATTACHMENT_VIEW,                    "\n" },             // <Enter>
  { OP_ATTACHMENT_VIEW,                    "\r" },             // <Return>
#ifdef USE_AUTOCRYPT
  { OP_COMPOSE_AUTOCRYPT_MENU,             "o" },
#endif
  { OP_COMPOSE_EDIT_FILE,                  "\033e" },          // <Alt-e>
  { OP_COMPOSE_EDIT_MESSAGE,               "e" },
  { OP_COMPOSE_ISPELL,                     "i" },
  { OP_COMPOSE_PGP_MENU,                   "p" },
  { OP_COMPOSE_POSTPONE_MESSAGE,           "P" },
  { OP_COMPOSE_RENAME_FILE,                "R" },
  { OP_COMPOSE_SEND_MESSAGE,               "y" },
  { OP_COMPOSE_SMIME_MENU,                 "S" },
  { OP_COMPOSE_WRITE_MESSAGE,              "w" },
  { OP_DISPLAY_HEADERS,                    "h" },
  { OP_ENVELOPE_EDIT_BCC,                  "b" },
  { OP_ENVELOPE_EDIT_CC,                   "c" },
  { OP_ENVELOPE_EDIT_FCC,                  "f" },
  { OP_ENVELOPE_EDIT_FROM,                 "\033f" },          // <Alt-f>
  { OP_ENVELOPE_EDIT_HEADERS,              "E" },
  { OP_ENVELOPE_EDIT_REPLY_TO,             "r" },
  { OP_ENVELOPE_EDIT_SUBJECT,              "s" },
  { OP_ENVELOPE_EDIT_TO,                   "t" },
  { OP_FORGET_PASSPHRASE,                  "\006" },           // <Ctrl-F>
  { OP_TAG,                                "T" },
  { 0, NULL },
};
// clang-format on

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
 * gen_cid - Generate a random Content ID
 * @retval ptr Content ID
 *
 * @note The caller should free the string
 */
static char *gen_cid(void)
{
  char rndid[MUTT_RANDTAG_LEN + 1];

  mutt_rand_base32(rndid, sizeof(rndid) - 1);
  rndid[MUTT_RANDTAG_LEN] = 0;

  return mutt_str_dup(rndid);
}

/**
 * check_cid - Check if a Content-ID is valid
 * @param  cid   Content-ID to check
 * @retval true  Content-ID is valid
 * @retval false Content-ID is not valid
 */
static bool check_cid(const char *cid)
{
  static const char *check = "^[-\\.0-9@A-Z_a-z]+$";

  struct Regex *check_cid_regex = mutt_regex_new(check, 0, NULL);

  const bool valid = mutt_regex_match(check_cid_regex, cid);

  mutt_regex_free(&check_cid_regex);

  return valid;
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
  struct stat st = { 0 };
  struct Buffer *pretty = NULL, *msg = NULL;

  for (int i = 0; i < actx->idxlen; i++)
  {
    if (actx->idx[i]->body->type == TYPE_MULTIPART)
      continue;
    if (stat(actx->idx[i]->body->filename, &st) != 0)
    {
      if (!pretty)
        pretty = buf_pool_get();
      buf_strcpy(pretty, actx->idx[i]->body->filename);
      buf_pretty_mailbox(pretty);
      /* L10N: This message is displayed in the compose menu when an attachment
         doesn't stat.  %d is the attachment number and %s is the attachment
         filename.  The filename is located last to avoid a long path hiding
         the error message.  */
      mutt_error(_("Attachment #%d no longer exists: %s"), i + 1, buf_string(pretty));
      goto cleanup;
    }

    if (actx->idx[i]->body->stamp < st.st_mtime)
    {
      if (!pretty)
        pretty = buf_pool_get();
      buf_strcpy(pretty, actx->idx[i]->body->filename);
      buf_pretty_mailbox(pretty);

      if (!msg)
        msg = buf_pool_get();
      /* L10N: This message is displayed in the compose menu when an attachment
         is modified behind the scenes.  %d is the attachment number and %s is
         the attachment filename.  The filename is located last to avoid a long
         path hiding the prompt question.  */
      buf_printf(msg, _("Attachment #%d modified. Update encoding for %s?"),
                 i + 1, buf_string(pretty));

      enum QuadOption ans = query_yesorno(buf_string(msg), MUTT_YES);
      if (ans == MUTT_YES)
        mutt_update_encoding(actx->idx[i]->body, sub);
      else if (ans == MUTT_ABORT)
        goto cleanup;
    }
  }

  rc = 0;

cleanup:
  buf_pool_release(&pretty);
  buf_pool_release(&msg);
  return rc;
}

/**
 * delete_attachment - Delete an attachment
 * @param actx Attachment context
 * @param aidx  Index number of attachment to delete
 * @retval  0 Success
 * @retval -1 Error
 */
static int delete_attachment(struct AttachCtx *actx, int aidx)
{
  if (!actx || (aidx < 0) || (aidx >= actx->idxlen))
    return -1;

  struct AttachPtr **idx = actx->idx;
  struct Body *b_previous = NULL;
  struct Body *b_parent = NULL;

  if (aidx == 0)
  {
    struct Body *b = actx->idx[0]->body;
    if (!b->next) // There's only one attachment left
    {
      mutt_error(_("You may not delete the only attachment"));
      return -1;
    }

    if (cs_subset_bool(NeoMutt->sub, "compose_confirm_detach_first"))
    {
      /* L10N: Prompt when trying to hit <detach-file> on the first entry in
         the compose menu.  This entry is most likely the message they just
         typed.  Hitting yes will remove the entry and unlink the file, so
         it's worth confirming they really meant to do it. */
      enum QuadOption ans = query_yesorno_help(_("Really delete the main message?"),
                                               MUTT_NO, NeoMutt->sub,
                                               "compose_confirm_detach_first");
      if (ans == MUTT_NO)
      {
        idx[aidx]->body->tagged = false;
        return -1;
      }
    }
  }

  if (idx[aidx]->level > 0)
  {
    if (attach_body_parent(idx[0]->body, NULL, idx[aidx]->body, &b_parent))
    {
      if (attach_body_count(b_parent->parts, false) < 3)
      {
        mutt_error(_("Can't leave group with only one attachment"));
        return -1;
      }
    }
  }

  // reorder body pointers
  if (aidx > 0)
  {
    if (attach_body_previous(idx[0]->body, idx[aidx]->body, &b_previous))
      b_previous->next = idx[aidx]->body->next;
    else if (attach_body_parent(idx[0]->body, NULL, idx[aidx]->body, &b_parent))
      b_parent->parts = idx[aidx]->body->next;
  }

  // free memory
  int part_count = 1;
  if (aidx < (actx->idxlen - 1))
  {
    if ((idx[aidx]->body->type == TYPE_MULTIPART) &&
        (idx[aidx + 1]->level > idx[aidx]->level))
    {
      part_count += attach_body_count(idx[aidx]->body->parts, true);
    }
  }
  idx[aidx]->body->next = NULL;
  mutt_body_free(&(idx[aidx]->body));
  for (int i = 0; i < part_count; i++)
  {
    FREE(&idx[aidx + i]->tree);
    FREE(&idx[aidx + i]);
  }

  // reorder attachment list
  for (int i = aidx; i < (actx->idxlen - part_count); i++)
    idx[i] = idx[i + part_count];
  for (int i = 0; i < part_count; i++)
    idx[actx->idxlen - i - 1] = NULL;
  actx->idxlen -= part_count;

  return 0;
}

/**
 * update_idx - Add a new attachment to the message
 * @param menu Current menu
 * @param actx Attachment context
 * @param ap   Attachment to add
 */
static void update_idx(struct Menu *menu, struct AttachCtx *actx, struct AttachPtr *ap)
{
  ap->level = 0;
  for (int i = actx->idxlen; i > 0; i--)
  {
    if (ap->level == actx->idx[i - 1]->level)
    {
      actx->idx[i - 1]->body->next = ap->body;
      break;
    }
  }

  ap->body->aptr = ap;
  mutt_actx_add_attach(actx, ap);
  update_menu(actx, menu, false);
  menu_set_index(menu, actx->vcount - 1);
}

/**
 * compose_attach_swap - Swap two adjacent entries in the attachment list
 * @param e      Email
 * @param actx   Attachment information
 * @param first  Index of first attachment to swap
 * @param second Index of second attachment to swap
 */
static void compose_attach_swap(struct Email *e, struct AttachCtx *actx, int first, int second)
{
  struct AttachPtr **idx = actx->idx;

  // check that attachments really are adjacent
  if (idx[first]->body->next != idx[second]->body)
    return;

  // reorder Body pointers
  if (first == 0)
  {
    // first attachment is the fundamental part
    idx[first]->body->next = idx[second]->body->next;
    idx[second]->body->next = idx[first]->body;
    e->body = idx[second]->body;
  }
  else
  {
    // find previous attachment
    struct Body *b_previous = NULL;
    struct Body *b_parent = NULL;
    if (attach_body_previous(e->body, idx[first]->body, &b_previous))
    {
      idx[first]->body->next = idx[second]->body->next;
      idx[second]->body->next = idx[first]->body;
      b_previous->next = idx[second]->body;
    }
    else if (attach_body_parent(e->body, NULL, idx[first]->body, &b_parent))
    {
      idx[first]->body->next = idx[second]->body->next;
      idx[second]->body->next = idx[first]->body;
      b_parent->parts = idx[second]->body;
    }
  }

  // reorder attachment list
  struct AttachPtr *saved = idx[second];
  for (int i = second; i > first; i--)
    idx[i] = idx[i - 1];
  idx[first] = saved;

  // if moved attachment is a group then move subparts too
  if ((idx[first]->body->type == TYPE_MULTIPART) && (second < actx->idxlen - 1))
  {
    int i = second + 1;
    while (idx[i]->level > idx[first]->level)
    {
      saved = idx[i];
      int destidx = i - second + first;
      for (int j = i; j > destidx; j--)
        idx[j] = idx[j - 1];
      idx[destidx] = saved;
      i++;
      if (i >= actx->idxlen)
        break;
    }
  }
}

/**
 * group_attachments - Group tagged attachments into a multipart group
 * @param  shared     Shared compose data
 * @param  subtype    MIME subtype
 * @retval FR_SUCCESS Success
 * @retval FR_ERROR   Failure
 */
static int group_attachments(struct ComposeSharedData *shared, char *subtype)
{
  struct AttachCtx *actx = shared->adata->actx;
  int group_level = -1;
  struct Body *bptr_parent = NULL;

  // Attachments to be grouped must have the same parent
  for (int i = 0; i < actx->idxlen; i++)
  {
    // check if all tagged attachments are at same level
    if (actx->idx[i]->body->tagged)
    {
      if (group_level == -1)
      {
        group_level = actx->idx[i]->level;
      }
      else
      {
        if (group_level != actx->idx[i]->level)
        {
          mutt_error(_("Attachments to be grouped must have the same parent"));
          return FR_ERROR;
        }
      }
      // if not at top level check if all tagged attachments have same parent
      if (group_level > 0)
      {
        if (bptr_parent)
        {
          struct Body *bptr_test = NULL;
          if (!attach_body_parent(actx->idx[0]->body, NULL, actx->idx[i]->body, &bptr_test))
            mutt_debug(LL_DEBUG5, "can't find parent\n");
          if (bptr_test != bptr_parent)
          {
            mutt_error(_("Attachments to be grouped must have the same parent"));
            return FR_ERROR;
          }
        }
        else
        {
          if (!attach_body_parent(actx->idx[0]->body, NULL, actx->idx[i]->body, &bptr_parent))
            mutt_debug(LL_DEBUG5, "can't find parent\n");
        }
      }
    }
  }

  // Can't group all attachments unless at top level
  if (bptr_parent)
  {
    if (shared->adata->menu->num_tagged == attach_body_count(bptr_parent->parts, false))
    {
      mutt_error(_("Can't leave group with only one attachment"));
      return FR_ERROR;
    }
  }

  struct Body *group = mutt_body_new();
  group->type = TYPE_MULTIPART;
  group->subtype = mutt_str_dup(subtype);
  group->encoding = ENC_7BIT;

  struct Body *bptr_first = NULL;     // first tagged attachment
  struct Body *bptr = NULL;           // current tagged attachment
  struct Body *group_parent = NULL;   // parent of group
  struct Body *group_previous = NULL; // previous body to group
  struct Body *group_part = NULL;     // current attachment in group
  int group_idx = 0; // index in attachment list where group will be inserted
  int group_last_idx = 0; // index of last part of previous found group
  int group_parent_type = TYPE_OTHER;

  for (int i = 0; i < actx->idxlen; i++)
  {
    bptr = actx->idx[i]->body;
    if (bptr->tagged)
    {
      // set group properties based on first tagged attachment
      if (!bptr_first)
      {
        group->disposition = bptr->disposition;
        if (bptr->language && !mutt_str_equal(subtype, "multilingual"))
          group->language = mutt_str_dup(bptr->language);
        group_parent_type = bptr->aptr->parent_type;
        bptr_first = bptr;
        if (i > 0)
        {
          if (!attach_body_previous(shared->email->body, bptr, &group_previous))
          {
            mutt_debug(LL_DEBUG5, "couldn't find previous\n");
          }
          if (!attach_body_parent(shared->email->body, NULL, bptr, &group_parent))
          {
            mutt_debug(LL_DEBUG5, "couldn't find parent\n");
          }
        }
      }

      shared->adata->menu->num_tagged--;
      bptr->tagged = false;
      bptr->aptr->level++;
      bptr->aptr->parent_type = TYPE_MULTIPART;

      // append bptr to the group parts list and remove from email body list
      struct Body *bptr_previous = NULL;
      if (attach_body_previous(shared->email->body, bptr, &bptr_previous))
        bptr_previous->next = bptr->next;
      else if (attach_body_parent(shared->email->body, NULL, bptr, &bptr_parent))
        bptr_parent->parts = bptr->next;
      else
        shared->email->body = bptr->next;

      if (group_part)
      {
        // add bptr to group parts list
        group_part->next = bptr;
        group_part = group_part->next;
        group_part->next = NULL;

        // reorder attachments and set levels
        int bptr_attachments = attach_body_count(bptr, true);
        for (int j = i + 1; j < (i + bptr_attachments); j++)
          actx->idx[j]->level++;
        if (i > (group_last_idx + 1))
        {
          for (int j = 0; j < bptr_attachments; j++)
          {
            struct AttachPtr *saved = actx->idx[i + bptr_attachments - 1];
            for (int k = i + bptr_attachments - 1; k > (group_last_idx + 1); k--)
              actx->idx[k] = actx->idx[k - 1];
            actx->idx[group_last_idx + 1] = saved;
          }
        }
        i += bptr_attachments - 1;
        group_last_idx += bptr_attachments;
      }
      else
      {
        group_idx = i;
        group->parts = bptr;
        group_part = bptr;
        group_part->next = NULL;
        int bptr_attachments = attach_body_count(bptr, true);
        for (int j = i + 1; j < (i + bptr_attachments); j++)
          actx->idx[j]->level++;
        i += bptr_attachments - 1;
        group_last_idx = i;
      }
    }
  }

  if (!bptr_first)
  {
    mutt_body_free(&group);
    return FR_ERROR;
  }

  // set group->next
  int next_aidx = group_idx + attach_body_count(group->parts, true);
  if (group_parent)
  {
    // find next attachment with the same parent as the group
    struct Body *b = NULL;
    struct Body *b_parent = NULL;
    while (next_aidx < actx->idxlen)
    {
      b = actx->idx[next_aidx]->body;
      b_parent = NULL;
      if (attach_body_parent(shared->email->body, NULL, b, &b_parent))
      {
        if (group_parent == b_parent)
        {
          group->next = b;
          break;
        }
      }
      next_aidx++;
    }
  }
  else if (next_aidx < actx->idxlen)
  {
    // group is at top level
    group->next = actx->idx[next_aidx]->body;
  }

  // set previous or parent for group
  if (group_previous)
    group_previous->next = group;
  else if (group_parent)
    group_parent->parts = group;

  mutt_generate_boundary(&group->parameter);

  struct AttachPtr *group_ap = mutt_aptr_new();
  group_ap->body = group;
  group_ap->body->aptr = group_ap;
  group_ap->level = group_level;
  group_ap->parent_type = group_parent_type;

  // insert group into attachment list
  mutt_actx_ins_attach(actx, group_ap, group_idx);

  // update email body and last attachment pointers
  shared->email->body = actx->idx[0]->body;
  actx->idx[actx->idxlen - 1]->body->next = NULL;

  update_menu(actx, shared->adata->menu, false);
  shared->adata->menu->current = group_idx;
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * op_attachment_attach_file - Attach files to this message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_attach_file(struct ComposeSharedData *shared, int op)
{
  char *prompt = _("Attach file");
  int numfiles = 0;
  char **files = NULL;

  struct Buffer *fname = buf_pool_get();
  if ((mw_enter_fname(prompt, fname, false, NULL, true, &files, &numfiles,
                      MUTT_SEL_MULTI) == -1) ||
      buf_is_empty(fname))
  {
    for (int i = 0; i < numfiles; i++)
      FREE(&files[i]);

    FREE(&files);
    buf_pool_release(&fname);
    return FR_NO_ACTION;
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
    if (!att)
      continue;

    struct AttachPtr *ap = mutt_aptr_new();
    ap->unowned = true;
    ap->body = mutt_make_file_attach(att, shared->sub);
    if (ap->body)
    {
      added_attachment = true;
      update_idx(shared->adata->menu, shared->adata->actx, ap);
    }
    else
    {
      error = true;
      mutt_error(_("Unable to attach %s"), att);
      mutt_aptr_free(&ap);
    }
    FREE(&files[i]);
  }

  FREE(&files);
  buf_pool_release(&fname);

  if (!error)
    mutt_clear_error();

  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
  if (added_attachment)
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

/**
 * op_attachment_attach_key - Attach a PGP public key - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_attach_key(struct ComposeSharedData *shared, int op)
{
  if (!(WithCrypto & APPLICATION_PGP))
    return FR_NOT_IMPL;
  struct AttachPtr *ap = mutt_aptr_new();
  ap->body = crypt_pgp_make_key_attachment();
  if (ap->body)
  {
    update_idx(shared->adata->menu, shared->adata->actx, ap);
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  }
  else
  {
    mutt_aptr_free(&ap);
  }

  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
  return FR_SUCCESS;
}

/**
 * op_attachment_attach_message - Attach messages to this message - Implements ::compose_function_t - @ingroup compose_function_api
 *
 * This function handles:
 * - OP_ATTACHMENT_ATTACH_MESSAGE
 * - OP_ATTACHMENT_ATTACH_NEWS_MESSAGE
 */
static int op_attachment_attach_message(struct ComposeSharedData *shared, int op)
{
  char *prompt = _("Open mailbox to attach message from");

  OptNews = false;
  if (shared->mailbox && (op == OP_ATTACHMENT_ATTACH_NEWS_MESSAGE))
  {
    const char *const c_news_server = cs_subset_string(shared->sub, "news_server");
    CurrentNewsSrv = nntp_select_server(shared->mailbox, c_news_server, false);
    if (!CurrentNewsSrv)
      return FR_NO_ACTION;

    prompt = _("Open newsgroup to attach message from");
    OptNews = true;
  }

  struct Buffer *fname = buf_pool_get();
  if (shared->mailbox)
  {
    if ((op == OP_ATTACHMENT_ATTACH_MESSAGE) ^ (shared->mailbox->type == MUTT_NNTP))
    {
      buf_strcpy(fname, mailbox_path(shared->mailbox));
      buf_pretty_mailbox(fname);
    }
  }

  if ((mw_enter_fname(prompt, fname, true, shared->mailbox, false, NULL, NULL,
                      MUTT_SEL_NO_FLAGS) == -1) ||
      buf_is_empty(fname))
  {
    buf_pool_release(&fname);
    return FR_NO_ACTION;
  }

  if (OptNews)
    nntp_expand_path(fname->data, fname->dsize, &CurrentNewsSrv->conn->account);
  else
    buf_expand_path(fname);

  if (imap_path_probe(buf_string(fname), NULL) != MUTT_IMAP)
  {
    if (pop_path_probe(buf_string(fname), NULL) != MUTT_POP)
    {
      if (!OptNews && (nntp_path_probe(buf_string(fname), NULL) != MUTT_NNTP))
      {
        if (mx_path_probe(buf_string(fname)) != MUTT_NOTMUCH)
        {
          /* check to make sure the file exists and is readable */
          if (access(buf_string(fname), R_OK) == -1)
          {
            mutt_perror("%s", buf_string(fname));
            buf_pool_release(&fname);
            return FR_ERROR;
          }
        }
      }
    }
  }

  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);

  struct Mailbox *m_attach = mx_path_resolve(buf_string(fname));
  const bool old_readonly = m_attach->readonly;
  if (!mx_mbox_open(m_attach, MUTT_READONLY))
  {
    mutt_error(_("Unable to open mailbox %s"), buf_string(fname));
    mx_fastclose_mailbox(m_attach, false);
    m_attach = NULL;
    buf_pool_release(&fname);
    return FR_ERROR;
  }
  buf_pool_release(&fname);

  if (m_attach->msg_count == 0)
  {
    mx_mbox_close(m_attach);
    mutt_error(_("No messages in that folder"));
    return FR_NO_ACTION;
  }

  /* `$sort`, `$sort_aux`, `$use_threads` could be changed in dlg_index() */
  const enum SortType old_sort = cs_subset_sort(shared->sub, "sort");
  const enum SortType old_sort_aux = cs_subset_sort(shared->sub, "sort_aux");
  const unsigned char old_use_threads = cs_subset_enum(shared->sub, "use_threads");

  mutt_message(_("Tag the messages you want to attach"));
  struct MuttWindow *dlg = index_pager_init();
  struct IndexSharedData *index_shared = dlg->wdata;
  index_shared->attach_msg = true;
  dialog_push(dlg);
  struct Mailbox *m_attach_new = dlg_index(dlg, m_attach);
  dialog_pop();
  mutt_window_free(&dlg);

  if (!shared->mailbox)
  {
    /* Restore old $sort variables */
    cs_subset_str_native_set(shared->sub, "sort", old_sort, NULL);
    cs_subset_str_native_set(shared->sub, "sort_aux", old_sort_aux, NULL);
    cs_subset_str_native_set(shared->sub, "use_threads", old_use_threads, NULL);
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
    notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
    return FR_SUCCESS;
  }

  bool added_attachment = false;
  for (int i = 0; i < m_attach_new->msg_count; i++)
  {
    if (!m_attach_new->emails[i])
      break;
    if (!message_is_tagged(m_attach_new->emails[i]))
      continue;

    struct AttachPtr *ap = mutt_aptr_new();
    ap->body = mutt_make_message_attach(m_attach_new, m_attach_new->emails[i],
                                        true, shared->sub);
    if (ap->body)
    {
      added_attachment = true;
      update_idx(shared->adata->menu, shared->adata->actx, ap);
    }
    else
    {
      mutt_error(_("Unable to attach"));
      mutt_aptr_free(&ap);
    }
  }
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);

  if (m_attach_new == m_attach)
  {
    m_attach->readonly = old_readonly;
  }
  mx_fastclose_mailbox(m_attach_new, false);

  /* Restore old $sort variables */
  cs_subset_str_native_set(shared->sub, "sort", old_sort, NULL);
  cs_subset_str_native_set(shared->sub, "sort_aux", old_sort_aux, NULL);
  cs_subset_str_native_set(shared->sub, "use_threads", old_use_threads, NULL);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
  if (added_attachment)
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

/**
 * op_attachment_detach - Delete the current entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_detach(struct ComposeSharedData *shared, int op)
{
  struct AttachCtx *actx = shared->adata->actx;
  if (!check_count(actx))
    return FR_NO_ACTION;

  struct Menu *menu = shared->adata->menu;
  struct AttachPtr *cur_att = current_attachment(actx, menu);
  if (cur_att->unowned)
    cur_att->body->unlink = false;

  int index = menu_get_index(menu);
  if (delete_attachment(actx, index) == -1)
    return FR_ERROR;

  menu->num_tagged = 0;
  for (int i = 0; i < actx->idxlen; i++)
  {
    if (actx->idx[i]->body->tagged)
      menu->num_tagged++;
  }

  update_menu(actx, menu, false);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);

  index = menu_get_index(menu);
  if (index == 0)
    shared->email->body = actx->idx[0]->body;

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

/**
 * op_attachment_edit_content_id - Edit the 'Content-ID' of the attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_edit_content_id(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;

  int rc = FR_NO_ACTION;
  struct Buffer *buf = buf_pool_get();
  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);

  char *id = cur_att->body->content_id;
  if (id)
  {
    buf_strcpy(buf, id);
  }
  else
  {
    id = gen_cid();
    buf_strcpy(buf, id);
    FREE(&id);
  }

  if (mw_get_field("Content-ID: ", buf, MUTT_COMP_NO_FLAGS, HC_OTHER, NULL, NULL) == 0)
  {
    if (!mutt_str_equal(id, buf_string(buf)))
    {
      if (check_cid(buf_string(buf)))
      {
        mutt_str_replace(&cur_att->body->content_id, buf_string(buf));
        menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
        notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
        mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
        rc = FR_SUCCESS;
      }
      else
      {
        mutt_error(_("Content-ID can only contain the characters: -.0-9@A-Z_a-z"));
        rc = FR_ERROR;
      }
    }
  }

  buf_pool_release(&buf);

  if (rc != FR_ERROR)
    mutt_clear_error();

  return rc;
}

/**
 * op_attachment_edit_description - Edit attachment description - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_edit_description(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;

  int rc = FR_NO_ACTION;
  struct Buffer *buf = buf_pool_get();

  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  buf_strcpy(buf, cur_att->body->description);

  /* header names should not be translated */
  if (mw_get_field("Description: ", buf, MUTT_COMP_NO_FLAGS, HC_OTHER, NULL, NULL) == 0)
  {
    if (!mutt_str_equal(cur_att->body->description, buf_string(buf)))
    {
      mutt_str_replace(&cur_att->body->description, buf_string(buf));
      menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
      mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
      rc = FR_SUCCESS;
    }
  }

  buf_pool_release(&buf);
  return rc;
}

/**
 * op_attachment_edit_encoding - Edit attachment transfer-encoding - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_edit_encoding(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;

  int rc = FR_NO_ACTION;
  struct Buffer *buf = buf_pool_get();

  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  buf_strcpy(buf, ENCODING(cur_att->body->encoding));

  if ((mw_get_field("Content-Transfer-Encoding: ", buf, MUTT_COMP_NO_FLAGS,
                    HC_OTHER, NULL, NULL) == 0) &&
      !buf_is_empty(buf))
  {
    int enc = mutt_check_encoding(buf_string(buf));
    if ((enc != ENC_OTHER) && (enc != ENC_UUENCODED))
    {
      if (enc != cur_att->body->encoding)
      {
        cur_att->body->encoding = enc;
        menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
        notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
        mutt_clear_error();
        mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
        rc = FR_SUCCESS;
      }
    }
    else
    {
      mutt_error(_("Invalid encoding"));
      rc = FR_ERROR;
    }
  }

  buf_pool_release(&buf);
  return rc;
}

/**
 * op_attachment_edit_language - Edit the 'Content-Language' of the attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_edit_language(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;

  int rc = FR_NO_ACTION;
  struct Buffer *buf = buf_pool_get();
  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);

  buf_strcpy(buf, cur_att->body->language);
  if (mw_get_field("Content-Language: ", buf, MUTT_COMP_NO_FLAGS, HC_OTHER, NULL, NULL) == 0)
  {
    if (!mutt_str_equal(cur_att->body->language, buf_string(buf)))
    {
      mutt_str_replace(&cur_att->body->language, buf_string(buf));
      menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
      notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
      mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
      rc = FR_SUCCESS;
    }
    mutt_clear_error();
  }
  else
  {
    mutt_warning(_("Empty 'Content-Language'"));
    rc = FR_ERROR;
  }

  buf_pool_release(&buf);
  return rc;
}

/**
 * op_attachment_edit_mime - Edit attachment using mailcap entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_edit_mime(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;
  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  if (!mutt_edit_attachment(cur_att->body))
    return FR_NO_ACTION;

  mutt_update_encoding(cur_att->body, shared->sub);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

/**
 * op_attachment_edit_type - Edit attachment content type - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_edit_type(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;

  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  if (!mutt_edit_content_type(NULL, cur_att->body, NULL))
    return FR_NO_ACTION;

  /* this may have been a change to text/something */
  mutt_update_encoding(cur_att->body, shared->sub);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

/**
 * op_attachment_filter - Filter attachment through a shell command - Implements ::compose_function_t - @ingroup compose_function_api
 *
 * This function handles:
 * - OP_ATTACHMENT_FILTER
 * - OP_PIPE
 */
static int op_attachment_filter(struct ComposeSharedData *shared, int op)
{
  struct AttachCtx *actx = shared->adata->actx;
  if (!check_count(actx))
    return FR_NO_ACTION;

  struct Menu *menu = shared->adata->menu;
  struct AttachPtr *cur_att = current_attachment(actx, menu);
  if (cur_att->body->type == TYPE_MULTIPART)
  {
    mutt_error(_("Can't filter multipart attachments"));
    return FR_ERROR;
  }
  mutt_pipe_attachment_list(actx, NULL, menu->tag_prefix, cur_att->body,
                            (op == OP_ATTACHMENT_FILTER));
  if (op == OP_ATTACHMENT_FILTER) /* cte might have changed */
  {
    menu_queue_redraw(menu, menu->tag_prefix ? MENU_REDRAW_FULL : MENU_REDRAW_CURRENT);
  }
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

/**
 * op_attachment_get_attachment - Get a temporary copy of an attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_get_attachment(struct ComposeSharedData *shared, int op)
{
  struct AttachCtx *actx = shared->adata->actx;
  if (!check_count(actx))
    return FR_NO_ACTION;

  int rc = FR_ERROR;
  struct Menu *menu = shared->adata->menu;
  struct BodyArray ba = ARRAY_HEAD_INITIALIZER;
  ba_add_tagged(&ba, actx, menu);
  if (ARRAY_EMPTY(&ba))
    goto done;

  struct Body **bp = NULL;
  ARRAY_FOREACH(bp, &ba)
  {
    if ((*bp)->type == TYPE_MULTIPART)
    {
      mutt_warning(_("Can't get multipart attachments"));
      continue;
    }
    mutt_get_tmp_attachment(*bp);
  }

  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  rc = FR_SUCCESS;

done:
  ARRAY_FREE(&ba);
  /* No send2hook since this doesn't change the message. */
  return rc;
}

/**
 * op_attachment_group_alts - Group tagged attachments as 'multipart/alternative' - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_group_alts(struct ComposeSharedData *shared, int op)
{
  if (shared->adata->menu->num_tagged < 2)
  {
    mutt_error(_("Grouping 'alternatives' requires at least 2 tagged messages"));
    return FR_ERROR;
  }

  return group_attachments(shared, "alternative");
}

/**
 * op_attachment_group_lingual - Group tagged attachments as 'multipart/multilingual' - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_group_lingual(struct ComposeSharedData *shared, int op)
{
  if (shared->adata->menu->num_tagged < 2)
  {
    mutt_error(_("Grouping 'multilingual' requires at least 2 tagged messages"));
    return FR_ERROR;
  }

  /* traverse to see whether all the parts have Content-Language: set */
  int tagged_with_lang_num = 0;
  for (struct Body *b = shared->email->body; b; b = b->next)
    if (b->tagged && b->language && *b->language)
      tagged_with_lang_num++;

  if (shared->adata->menu->num_tagged != tagged_with_lang_num)
  {
    if (query_yesorno(_("Not all parts have 'Content-Language' set, continue?"),
                      MUTT_YES) != MUTT_YES)
    {
      mutt_message(_("Not sending this message"));
      return FR_ERROR;
    }
  }

  return group_attachments(shared, "multilingual");
}

/**
 * op_attachment_group_related - Group tagged attachments as 'multipart/related' - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_group_related(struct ComposeSharedData *shared, int op)
{
  if (shared->adata->menu->num_tagged < 2)
  {
    mutt_error(_("Grouping 'related' requires at least 2 tagged messages"));
    return FR_ERROR;
  }

  // ensure Content-ID is set for tagged attachments
  for (struct Body *b = shared->email->body; b; b = b->next)
  {
    if (!b->tagged || (b->type == TYPE_MULTIPART))
      continue;

    if (!b->content_id)
    {
      b->content_id = gen_cid();
    }
  }

  return group_attachments(shared, "related");
}

/**
 * op_attachment_move_down - Move an attachment down in the attachment list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_move_down(struct ComposeSharedData *shared, int op)
{
  int index = menu_get_index(shared->adata->menu);

  struct AttachCtx *actx = shared->adata->actx;

  if (index < 0)
    return FR_ERROR;

  if (index == (actx->idxlen - 1))
  {
    mutt_error(_("Attachment is already at bottom"));
    return FR_NO_ACTION;
  }
  if ((actx->idx[index]->parent_type == TYPE_MULTIPART) &&
      !actx->idx[index]->body->next)
  {
    mutt_error(_("Attachment can't be moved out of group"));
    return FR_ERROR;
  }

  // find next attachment at current level
  int nextidx = index + 1;
  while ((nextidx < actx->idxlen) &&
         (actx->idx[nextidx]->level > actx->idx[index]->level))
  {
    nextidx++;
  }
  if (nextidx == actx->idxlen)
  {
    mutt_error(_("Attachment is already at bottom"));
    return FR_NO_ACTION;
  }

  // find final position
  int finalidx = index + 1;
  if (nextidx < actx->idxlen - 1)
  {
    if ((actx->idx[nextidx]->body->type == TYPE_MULTIPART) &&
        (actx->idx[nextidx + 1]->level > actx->idx[nextidx]->level))
    {
      finalidx += attach_body_count(actx->idx[nextidx]->body->parts, true);
    }
  }

  compose_attach_swap(shared->email, shared->adata->actx, index, nextidx);
  mutt_update_tree(shared->adata->actx);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  menu_set_index(shared->adata->menu, finalidx);
  return FR_SUCCESS;
}

/**
 * op_attachment_move_up - Move an attachment up in the attachment list - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_move_up(struct ComposeSharedData *shared, int op)
{
  int index = menu_get_index(shared->adata->menu);
  if (index < 0)
    return FR_ERROR;

  struct AttachCtx *actx = shared->adata->actx;

  if (index == 0)
  {
    mutt_error(_("Attachment is already at top"));
    return FR_NO_ACTION;
  }
  if (actx->idx[index - 1]->level < actx->idx[index]->level)
  {
    mutt_error(_("Attachment can't be moved out of group"));
    return FR_ERROR;
  }

  // find previous attachment at current level
  int previdx = index - 1;
  while ((previdx > 0) && (actx->idx[previdx]->level > actx->idx[index]->level))
    previdx--;

  compose_attach_swap(shared->email, actx, previdx, index);
  mutt_update_tree(actx);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  menu_set_index(shared->adata->menu, previdx);
  return FR_SUCCESS;
}

/**
 * op_attachment_new_mime - Compose new attachment using mailcap entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_new_mime(struct ComposeSharedData *shared, int op)
{
  int rc = FR_NO_ACTION;
  struct Buffer *fname = buf_pool_get();
  struct Buffer *type = NULL;
  struct AttachPtr *ap = NULL;

  struct FileCompletionData cdata = { false, shared->mailbox, NULL, NULL };
  if ((mw_get_field(_("New file: "), fname, MUTT_COMP_NO_FLAGS, HC_FILE,
                    &CompleteFileOps, &cdata) != 0) ||
      buf_is_empty(fname))
  {
    goto done;
  }
  buf_expand_path(fname);

  /* Call to lookup_mime_type () ?  maybe later */
  type = buf_pool_get();
  if ((mw_get_field("Content-Type: ", type, MUTT_COMP_NO_FLAGS, HC_OTHER, NULL, NULL) != 0) ||
      buf_is_empty(type))
  {
    goto done;
  }

  rc = FR_ERROR;
  char *p = strchr(buf_string(type), '/');
  if (!p)
  {
    mutt_error(_("Content-Type is of the form base/sub"));
    goto done;
  }
  *p++ = 0;
  enum ContentType itype = mutt_check_mime_type(buf_string(type));
  if (itype == TYPE_OTHER)
  {
    mutt_error(_("Unknown Content-Type %s"), buf_string(type));
    goto done;
  }

  ap = mutt_aptr_new();
  /* Touch the file */
  FILE *fp = mutt_file_fopen(buf_string(fname), "w");
  if (!fp)
  {
    mutt_error(_("Can't create file %s"), buf_string(fname));
    goto done;
  }
  mutt_file_fclose(&fp);

  ap->body = mutt_make_file_attach(buf_string(fname), shared->sub);
  if (!ap->body)
  {
    mutt_error(_("Error attaching file"));
    goto done;
  }
  update_idx(shared->adata->menu, shared->adata->actx, ap);
  ap = NULL; // shared->adata->actx has taken ownership

  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  cur_att->body->type = itype;
  mutt_str_replace(&cur_att->body->subtype, p);
  cur_att->body->unlink = true;
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);

  if (mutt_compose_attachment(cur_att->body))
  {
    mutt_update_encoding(cur_att->body, shared->sub);
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
  }
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  rc = FR_SUCCESS;

done:
  mutt_aptr_free(&ap);
  buf_pool_release(&type);
  buf_pool_release(&fname);
  return rc;
}

/**
 * op_attachment_print - Print the current entry - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_print(struct ComposeSharedData *shared, int op)
{
  struct AttachCtx *actx = shared->adata->actx;
  if (!check_count(actx))
    return FR_NO_ACTION;

  struct Menu *menu = shared->adata->menu;
  struct AttachPtr *cur_att = current_attachment(actx, menu);
  if (cur_att->body->type == TYPE_MULTIPART)
  {
    mutt_error(_("Can't print multipart attachments"));
    return FR_ERROR;
  }

  mutt_print_attachment_list(actx, NULL, menu->tag_prefix, cur_att->body);
  /* no send2hook, since this doesn't modify the message */
  return FR_SUCCESS;
}

/**
 * op_attachment_rename_attachment - Send attachment with a different name - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_rename_attachment(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;
  char *src = NULL;
  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  if (cur_att->body->d_filename)
    src = cur_att->body->d_filename;
  else
    src = cur_att->body->filename;
  struct Buffer *fname = buf_pool_get();
  buf_strcpy(fname, mutt_path_basename(NONULL(src)));
  struct FileCompletionData cdata = { false, shared->mailbox, NULL, NULL };
  int rc = mw_get_field(_("Send attachment with name: "), fname,
                        MUTT_COMP_NO_FLAGS, HC_FILE, &CompleteFileOps, &cdata);
  if (rc == 0)
  {
    // It's valid to set an empty string here, to erase what was set
    mutt_str_replace(&cur_att->body->d_filename, buf_string(fname));
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
  }
  buf_pool_release(&fname);
  return FR_SUCCESS;
}

/**
 * op_attachment_save - Save message/attachment to a mailbox/file - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_save(struct ComposeSharedData *shared, int op)
{
  struct AttachCtx *actx = shared->adata->actx;
  if (!check_count(actx))
    return FR_NO_ACTION;

  struct Menu *menu = shared->adata->menu;
  struct AttachPtr *cur_att = current_attachment(actx, menu);
  if (cur_att->body->type == TYPE_MULTIPART)
  {
    mutt_error(_("Can't save multipart attachments"));
    return FR_ERROR;
  }

  mutt_save_attachment_list(actx, NULL, menu->tag_prefix, cur_att->body, NULL, menu);
  /* no send2hook, since this doesn't modify the message */
  return FR_SUCCESS;
}

/**
 * op_attachment_toggle_disposition - Toggle disposition between inline/attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_toggle_disposition(struct ComposeSharedData *shared, int op)
{
  /* toggle the content-disposition between inline/attachment */
  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  cur_att->body->disposition = (cur_att->body->disposition == DISP_INLINE) ?
                                   DISP_ATTACH :
                                   DISP_INLINE;
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
  return FR_SUCCESS;
}

/**
 * op_attachment_toggle_recode - Toggle recoding of this attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_toggle_recode(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;
  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  if (!mutt_is_text_part(cur_att->body))
  {
    mutt_error(_("Recoding only affects text attachments"));
    return FR_ERROR;
  }
  cur_att->body->noconv = !cur_att->body->noconv;
  if (cur_att->body->noconv)
    mutt_message(_("The current attachment won't be converted"));
  else
    mutt_message(_("The current attachment will be converted"));
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

/**
 * op_attachment_toggle_unlink - Toggle whether to delete file after sending it - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_toggle_unlink(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;
  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  cur_att->body->unlink = !cur_att->body->unlink;

  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_INDEX);
  /* No send2hook since this doesn't change the message. */
  return FR_SUCCESS;
}

/**
 * op_attachment_ungroup - Ungroup a 'multipart' attachment - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_ungroup(struct ComposeSharedData *shared, int op)
{
  if (shared->adata->actx->idx[shared->adata->menu->current]->body->type != TYPE_MULTIPART)
  {
    mutt_error(_("Attachment is not 'multipart'"));
    return FR_ERROR;
  }

  int aidx = shared->adata->menu->current;
  struct AttachCtx *actx = shared->adata->actx;
  struct Body *b = actx->idx[aidx]->body;
  struct Body *b_next = b->next;
  struct Body *b_previous = NULL;
  struct Body *b_parent = NULL;
  int parent_type = actx->idx[aidx]->parent_type;
  int level = actx->idx[aidx]->level;

  // reorder body pointers
  if (attach_body_previous(shared->email->body, b, &b_previous))
    b_previous->next = b->parts;
  else if (attach_body_parent(shared->email->body, NULL, b, &b_parent))
    b_parent->parts = b->parts;
  else
    shared->email->body = b->parts;

  // update attachment list
  int i = aidx + 1;
  while (actx->idx[i]->level > level)
  {
    actx->idx[i]->level--;
    if (actx->idx[i]->level == level)
    {
      actx->idx[i]->parent_type = parent_type;
      // set body->next for final attachment in group
      if (!actx->idx[i]->body->next)
        actx->idx[i]->body->next = b_next;
    }
    i++;
    if (i == actx->idxlen)
      break;
  }

  // free memory
  actx->idx[aidx]->body->parts = NULL;
  actx->idx[aidx]->body->next = NULL;
  actx->idx[aidx]->body->email = NULL;
  mutt_body_free(&actx->idx[aidx]->body);
  FREE(&actx->idx[aidx]->tree);
  FREE(&actx->idx[aidx]);

  // reorder attachment list
  for (int j = aidx; j < (actx->idxlen - 1); j++)
    actx->idx[j] = actx->idx[j + 1];
  actx->idx[actx->idxlen - 1] = NULL;
  actx->idxlen--;
  update_menu(actx, shared->adata->menu, false);

  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

/**
 * op_attachment_update_encoding - Update an attachment's encoding info - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_attachment_update_encoding(struct ComposeSharedData *shared, int op)
{
  struct AttachCtx *actx = shared->adata->actx;
  if (!check_count(actx))
    return FR_NO_ACTION;

  int rc = FR_NO_ACTION;
  struct Menu *menu = shared->adata->menu;
  struct BodyArray ba = ARRAY_HEAD_INITIALIZER;
  ba_add_tagged(&ba, actx, menu);
  if (ARRAY_EMPTY(&ba))
    goto done;

  struct Body **bp = NULL;
  ARRAY_FOREACH(bp, &ba)
  {
    mutt_update_encoding(*bp, shared->sub);
  }

  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  rc = FR_SUCCESS;

done:
  ARRAY_FREE(&ba);
  return rc;
}

// -----------------------------------------------------------------------------

/**
 * op_envelope_edit_headers - Edit the message with headers - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_envelope_edit_headers(struct ComposeSharedData *shared, int op)
{
  mutt_rfc3676_space_unstuff(shared->email);
  const char *tag = NULL;
  char *err = NULL;
  mutt_env_to_local(shared->email->env);
  const char *const c_editor = cs_subset_string(shared->sub, "editor");
  if (shared->email->body->type == TYPE_MULTIPART)
  {
    struct Body *b = shared->email->body->parts;
    while (b->parts)
      b = b->parts;
    mutt_edit_headers(NONULL(c_editor), b->filename, shared->email, shared->fcc);
  }
  else
  {
    mutt_edit_headers(NONULL(c_editor), shared->email->body->filename,
                      shared->email, shared->fcc);
  }

  if (mutt_env_to_intl(shared->email->env, &tag, &err))
  {
    mutt_error(_("Bad IDN in '%s': '%s'"), tag, err);
    FREE(&err);
  }
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ENVELOPE, NULL);

  mutt_rfc3676_space_stuff(shared->email);
  mutt_update_encoding(shared->email->body, shared->sub);

  /* attachments may have been added */
  if (shared->adata->actx->idxlen &&
      shared->adata->actx->idx[shared->adata->actx->idxlen - 1]->body->next)
  {
    mutt_actx_entries_free(shared->adata->actx);
    update_menu(shared->adata->actx, shared->adata->menu, true);
  }

  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
  /* Unconditional hook since editor was invoked */
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

/**
 * op_compose_edit_file - Edit the file to be attached - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_file(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;
  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  if (cur_att->body->type == TYPE_MULTIPART)
  {
    mutt_error(_("Can't edit multipart attachments"));
    return FR_ERROR;
  }
  const char *const c_editor = cs_subset_string(shared->sub, "editor");
  mutt_edit_file(NONULL(c_editor), cur_att->body->filename);
  mutt_update_encoding(cur_att->body, shared->sub);
  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
  /* Unconditional hook since editor was invoked */
  mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  return FR_SUCCESS;
}

/**
 * op_compose_edit_message - Edit the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_edit_message(struct ComposeSharedData *shared, int op)
{
  const bool c_edit_headers = cs_subset_bool(shared->sub, "edit_headers");
  if (!c_edit_headers)
  {
    mutt_rfc3676_space_unstuff(shared->email);
    const char *const c_editor = cs_subset_string(shared->sub, "editor");
    mutt_edit_file(c_editor, shared->email->body->filename);
    mutt_rfc3676_space_stuff(shared->email);
    mutt_update_encoding(shared->email->body, shared->sub);
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
    /* Unconditional hook since editor was invoked */
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
    return FR_SUCCESS;
  }

  return op_envelope_edit_headers(shared, op);
}

/**
 * op_compose_ispell - Run ispell on the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_ispell(struct ComposeSharedData *shared, int op)
{
  endwin();
  const char *const c_ispell = cs_subset_string(shared->sub, "ispell");
  char buf[PATH_MAX] = { 0 };
  snprintf(buf, sizeof(buf), "%s -x %s", NONULL(c_ispell), shared->email->body->filename);
  if (mutt_system(buf) == -1)
  {
    mutt_error(_("Error running \"%s\""), buf);
    return FR_ERROR;
  }

  mutt_update_encoding(shared->email->body, shared->sub);
  notify_send(shared->email->notify, NT_EMAIL, NT_EMAIL_CHANGE_ATTACH, NULL);
  return FR_SUCCESS;
}

/**
 * op_compose_postpone_message - Save this message to send later - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_postpone_message(struct ComposeSharedData *shared, int op)
{
  if (check_attachments(shared->adata->actx, shared->sub) != 0)
  {
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
    return FR_ERROR;
  }

  shared->rc = 1;
  return FR_DONE;
}

/**
 * op_compose_rename_file - Rename/move an attached file - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_rename_file(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;
  struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                 shared->adata->menu);
  if (cur_att->body->type == TYPE_MULTIPART)
  {
    mutt_error(_("Can't rename multipart attachments"));
    return FR_ERROR;
  }
  struct Buffer *fname = buf_pool_get();
  buf_strcpy(fname, cur_att->body->filename);
  buf_pretty_mailbox(fname);
  struct FileCompletionData cdata = { false, shared->mailbox, NULL, NULL };
  if ((mw_get_field(_("Rename to: "), fname, MUTT_COMP_NO_FLAGS, HC_FILE,
                    &CompleteFileOps, &cdata) == 0) &&
      !buf_is_empty(fname))
  {
    struct stat st = { 0 };
    if (stat(cur_att->body->filename, &st) == -1)
    {
      /* L10N: "stat" is a system call. Do "man 2 stat" for more information. */
      mutt_error(_("Can't stat %s: %s"), buf_string(fname), strerror(errno));
      buf_pool_release(&fname);
      return FR_ERROR;
    }

    buf_expand_path(fname);
    if (mutt_file_rename(cur_att->body->filename, buf_string(fname)))
    {
      buf_pool_release(&fname);
      return FR_ERROR;
    }

    mutt_str_replace(&cur_att->body->filename, buf_string(fname));
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_CURRENT);

    if (cur_att->body->stamp >= st.st_mtime)
      mutt_stamp_attachment(cur_att->body);
    mutt_message_hook(NULL, shared->email, MUTT_SEND2_HOOK);
  }
  buf_pool_release(&fname);
  return FR_SUCCESS;
}

/**
 * op_compose_send_message - Send the message - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_send_message(struct ComposeSharedData *shared, int op)
{
  /* Note: We don't invoke send2-hook here, since we want to leave
   * users an opportunity to change settings from the ":" prompt.  */
  if (check_attachments(shared->adata->actx, shared->sub) != 0)
  {
    menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
    return FR_NO_ACTION;
  }

  if (!shared->fcc_set && !buf_is_empty(shared->fcc))
  {
    enum QuadOption ans = query_quadoption(_("Save a copy of this message?"),
                                           shared->sub, "copy");
    if (ans == MUTT_ABORT)
      return FR_NO_ACTION;
    else if (ans == MUTT_NO)
      buf_reset(shared->fcc);
  }

  shared->rc = 0;
  return FR_DONE;
}

/**
 * op_compose_write_message - Write the message to a folder - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_compose_write_message(struct ComposeSharedData *shared, int op)
{
  int rc = FR_NO_ACTION;
  struct Buffer *fname = buf_pool_get();
  if (shared->mailbox)
  {
    buf_strcpy(fname, mailbox_path(shared->mailbox));
    buf_pretty_mailbox(fname);
  }
  if (shared->adata->actx->idxlen)
    shared->email->body = shared->adata->actx->idx[0]->body;
  if ((mw_enter_fname(_("Write message to mailbox"), fname, true, shared->mailbox,
                      false, NULL, NULL, MUTT_SEL_NO_FLAGS) != -1) &&
      !buf_is_empty(fname))
  {
    mutt_message(_("Writing message to %s ..."), buf_string(fname));
    buf_expand_path(fname);

    if (shared->email->body->next)
      shared->email->body = mutt_make_multipart(shared->email->body);

    if (mutt_write_fcc(buf_string(fname), shared->email, NULL, false, NULL,
                       NULL, shared->sub) == 0)
      mutt_message(_("Message written"));

    shared->email->body = mutt_remove_multipart(shared->email->body);
    rc = FR_SUCCESS;
  }
  buf_pool_release(&fname);
  return rc;
}

/**
 * op_display_headers - Display message and toggle header weeding - Implements ::compose_function_t - @ingroup compose_function_api
 *
 * This function handles:
 * - OP_ATTACHMENT_VIEW
 * - OP_ATTACHMENT_VIEW_MAILCAP
 * - OP_ATTACHMENT_VIEW_PAGER
 * - OP_ATTACHMENT_VIEW_TEXT
 * - OP_DISPLAY_HEADERS
 */
static int op_display_headers(struct ComposeSharedData *shared, int op)
{
  if (!check_count(shared->adata->actx))
    return FR_NO_ACTION;

  enum ViewAttachMode mode = MUTT_VA_REGULAR;

  switch (op)
  {
    case OP_ATTACHMENT_VIEW:
    case OP_DISPLAY_HEADERS:
      break;

    case OP_ATTACHMENT_VIEW_MAILCAP:
      mode = MUTT_VA_MAILCAP;
      break;

    case OP_ATTACHMENT_VIEW_PAGER:
      mode = MUTT_VA_PAGER;
      break;

    case OP_ATTACHMENT_VIEW_TEXT:
      mode = MUTT_VA_AS_TEXT;
      break;
  }

  if (mode == MUTT_VA_REGULAR)
  {
    mutt_attach_display_loop(shared->sub, shared->adata->menu, op,
                             shared->email, shared->adata->actx, false);
  }
  else
  {
    struct AttachPtr *cur_att = current_attachment(shared->adata->actx,
                                                   shared->adata->menu);
    mutt_view_attachment(NULL, cur_att->body, mode, shared->email,
                         shared->adata->actx, shared->adata->menu->win);
  }

  menu_queue_redraw(shared->adata->menu, MENU_REDRAW_FULL);
  /* no send2hook, since this doesn't modify the message */
  return FR_SUCCESS;
}

/**
 * op_exit - Exit this menu - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_exit(struct ComposeSharedData *shared, int op)
{
  enum QuadOption ans = query_quadoption(_("Save (postpone) draft message?"),
                                         shared->sub, "postpone");
  if (ans == MUTT_NO)
  {
    for (int i = 0; i < shared->adata->actx->idxlen; i++)
      if (shared->adata->actx->idx[i]->unowned)
        shared->adata->actx->idx[i]->body->unlink = false;

    if (!(shared->flags & MUTT_COMPOSE_NOFREEHEADER))
    {
      for (int i = 0; i < shared->adata->actx->idxlen; i++)
      {
        /* avoid freeing other attachments */
        shared->adata->actx->idx[i]->body->next = NULL;
        if (!shared->adata->actx->idx[i]->body->email)
          shared->adata->actx->idx[i]->body->parts = NULL;
        mutt_body_free(&shared->adata->actx->idx[i]->body);
      }
    }
    shared->rc = -1;
    return FR_DONE;
  }
  else if (ans == MUTT_ABORT)
  {
    return FR_NO_ACTION;
  }

  return op_compose_postpone_message(shared, op);
}

/**
 * op_forget_passphrase - Wipe passphrases from memory - Implements ::compose_function_t - @ingroup compose_function_api
 */
static int op_forget_passphrase(struct ComposeSharedData *shared, int op)
{
  crypt_forget_passphrase();
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * ComposeFunctions - All the NeoMutt functions that the Compose supports
 */
static const struct ComposeFunction ComposeFunctions[] = {
  // clang-format off
  { OP_ATTACHMENT_ATTACH_FILE,            op_attachment_attach_file },
  { OP_ATTACHMENT_ATTACH_KEY,             op_attachment_attach_key },
  { OP_ATTACHMENT_ATTACH_MESSAGE,         op_attachment_attach_message },
  { OP_ATTACHMENT_ATTACH_NEWS_MESSAGE,    op_attachment_attach_message },
  { OP_ATTACHMENT_DETACH,                 op_attachment_detach },
  { OP_ATTACHMENT_EDIT_CONTENT_ID,        op_attachment_edit_content_id },
  { OP_ATTACHMENT_EDIT_DESCRIPTION,       op_attachment_edit_description },
  { OP_ATTACHMENT_EDIT_ENCODING,          op_attachment_edit_encoding },
  { OP_ATTACHMENT_EDIT_LANGUAGE,          op_attachment_edit_language },
  { OP_ATTACHMENT_EDIT_MIME,              op_attachment_edit_mime },
  { OP_ATTACHMENT_EDIT_TYPE,              op_attachment_edit_type },
  { OP_ATTACHMENT_FILTER,                 op_attachment_filter },
  { OP_ATTACHMENT_GET_ATTACHMENT,         op_attachment_get_attachment },
  { OP_ATTACHMENT_GROUP_ALTS,             op_attachment_group_alts },
  { OP_ATTACHMENT_GROUP_LINGUAL,          op_attachment_group_lingual },
  { OP_ATTACHMENT_GROUP_RELATED,          op_attachment_group_related },
  { OP_ATTACHMENT_MOVE_DOWN,              op_attachment_move_down },
  { OP_ATTACHMENT_MOVE_UP,                op_attachment_move_up },
  { OP_ATTACHMENT_NEW_MIME,               op_attachment_new_mime },
  { OP_PIPE,                              op_attachment_filter },
  { OP_ATTACHMENT_PRINT,                  op_attachment_print },
  { OP_ATTACHMENT_RENAME_ATTACHMENT,      op_attachment_rename_attachment },
  { OP_ATTACHMENT_SAVE,                   op_attachment_save },
  { OP_ATTACHMENT_TOGGLE_DISPOSITION,     op_attachment_toggle_disposition },
  { OP_ATTACHMENT_TOGGLE_RECODE,          op_attachment_toggle_recode },
  { OP_ATTACHMENT_TOGGLE_UNLINK,          op_attachment_toggle_unlink },
  { OP_ATTACHMENT_UNGROUP,                op_attachment_ungroup },
  { OP_ATTACHMENT_UPDATE_ENCODING,        op_attachment_update_encoding },
  { OP_ATTACHMENT_VIEW,                   op_display_headers },
  { OP_ATTACHMENT_VIEW_MAILCAP,           op_display_headers },
  { OP_ATTACHMENT_VIEW_PAGER,             op_display_headers },
  { OP_ATTACHMENT_VIEW_TEXT,              op_display_headers },
  { OP_COMPOSE_EDIT_FILE,                 op_compose_edit_file },
  { OP_COMPOSE_EDIT_MESSAGE,              op_compose_edit_message },
  { OP_COMPOSE_ISPELL,                    op_compose_ispell },
  { OP_COMPOSE_POSTPONE_MESSAGE,          op_compose_postpone_message },
  { OP_COMPOSE_RENAME_FILE,               op_compose_rename_file },
  { OP_COMPOSE_SEND_MESSAGE,              op_compose_send_message },
  { OP_COMPOSE_WRITE_MESSAGE,             op_compose_write_message },
  { OP_DISPLAY_HEADERS,                   op_display_headers },
  { OP_ENVELOPE_EDIT_HEADERS,             op_envelope_edit_headers },
  { OP_EXIT,                              op_exit },
  { OP_FORGET_PASSPHRASE,                 op_forget_passphrase },
  { 0, NULL },
  // clang-format on
};

/**
 * compose_function_dispatcher - Perform a Compose function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int compose_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win)
    return FR_UNKNOWN;

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg || !dlg->wdata)
    return FR_UNKNOWN;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; ComposeFunctions[i].op != OP_NULL; i++)
  {
    const struct ComposeFunction *fn = &ComposeFunctions[i];
    if (fn->op == op)
    {
      struct ComposeSharedData *shared = dlg->wdata;
      rc = fn->function(shared, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
