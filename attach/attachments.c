/**
 * @file
 * Handle the attachments command
 *
 * @authors
 * Copyright (C) 1996-2000,2012-2013 Michael R. Elkins <me@mutt.org>
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
 * @page attach_attachments Attachment commands
 *
 * Handle the attachments command
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "attachments.h"
#include "ncrypt/lib.h"
#include "parse/lib.h"
#include "mview.h"

/**
 * struct AttachMatch - An attachment matching a regex for attachment counter
 */
struct AttachMatch
{
  const char *major;          ///< Major mime type, e.g. "text"
  enum ContentType major_int; ///< Major mime type, e.g. #TYPE_TEXT
  const char *minor;          ///< Minor mime type, e.g. "html"
  regex_t minor_regex;        ///< Minor mime type regex
};

static struct ListHead AttachAllow = STAILQ_HEAD_INITIALIZER(AttachAllow); ///< List of attachment types to be counted
static struct ListHead AttachExclude = STAILQ_HEAD_INITIALIZER(AttachExclude); ///< List of attachment types to be ignored
static struct ListHead InlineAllow = STAILQ_HEAD_INITIALIZER(InlineAllow); ///< List of inline types to counted
static struct ListHead InlineExclude = STAILQ_HEAD_INITIALIZER(InlineExclude); ///< List of inline types to ignore
static struct Notify *AttachmentsNotify = NULL; ///< Notifications: #NotifyAttach

/**
 * attachmatch_free - Free an AttachMatch - Implements ::list_free_t - @ingroup list_free_api
 * @param ptr AttachMatch to free
 *
 * @note We don't free minor because it is either a pointer into major,
 *       or a static string.
 */
static void attachmatch_free(struct AttachMatch **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct AttachMatch *am = *ptr;
  regfree(&am->minor_regex);
  FREE(&am->major);
  FREE(ptr);
}

/**
 * attachmatch_new - Create a new AttachMatch
 * @retval ptr New AttachMatch
 */
static struct AttachMatch *attachmatch_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct AttachMatch));
}

/**
 * attach_cleanup - Free the attachments lists
 */
void attach_cleanup(void)
{
  notify_free(&AttachmentsNotify);

  /* Lists of AttachMatch */
  mutt_list_free_type(&AttachAllow, (list_free_t) attachmatch_free);
  mutt_list_free_type(&AttachExclude, (list_free_t) attachmatch_free);
  mutt_list_free_type(&InlineAllow, (list_free_t) attachmatch_free);
  mutt_list_free_type(&InlineExclude, (list_free_t) attachmatch_free);
}

/**
 * attach_init - Set up the attachments lists
 */
void attach_init(void)
{
  if (AttachmentsNotify)
    return;

  AttachmentsNotify = notify_new();
  notify_set_parent(AttachmentsNotify, NeoMutt->notify);
}

/**
 * count_body_parts_check - Compares mime types to the ok and except lists
 * @param checklist List of AttachMatch
 * @param b         Email Body
 * @param dflt      Log whether the matches are OK, or Excluded
 * @retval true Attachment should be counted
 */
static bool count_body_parts_check(struct ListHead *checklist, struct Body *b, bool dflt)
{
  /* If list is null, use default behavior. */
  if (!checklist || STAILQ_EMPTY(checklist))
  {
    return false;
  }

  struct AttachMatch *a = NULL;
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, checklist, entries)
  {
    a = (struct AttachMatch *) np->data;
    mutt_debug(LL_DEBUG3, "%s %d/%s ?? %s/%s [%d]... ", dflt ? "[OK]   " : "[EXCL] ",
               b->type, b->subtype ? b->subtype : "*", a->major, a->minor, a->major_int);
    if (((a->major_int == TYPE_ANY) || (a->major_int == b->type)) &&
        (!b->subtype || (regexec(&a->minor_regex, b->subtype, 0, NULL, 0) == 0)))
    {
      mutt_debug(LL_DEBUG3, "yes\n");
      return true;
    }
    else
    {
      mutt_debug(LL_DEBUG3, "no\n");
    }
  }

  return false;
}

/**
 * count_body_parts - Count the MIME Body parts
 * @param b Body of email
 * @retval num Number of MIME Body parts
 */
static int count_body_parts(struct Body *b)
{
  if (!b)
    return 0;

  int count = 0;

  for (struct Body *bp = b; bp; bp = bp->next)
  {
    /* Initial disposition is to count and not to recurse this part. */
    bool shallcount = true; /* default */
    bool shallrecurse = false;

    mutt_debug(LL_DEBUG5, "desc=\"%s\"; fn=\"%s\", type=\"%d/%s\"\n",
               bp->description ? bp->description : ("none"),
               bp->filename   ? bp->filename :
               bp->d_filename ? bp->d_filename :
                                "(none)",
               bp->type, bp->subtype ? bp->subtype : "*");

    if (bp->type == TYPE_MESSAGE)
    {
      shallrecurse = true;

      /* If it's an external body pointer, don't recurse it. */
      if (mutt_istr_equal(bp->subtype, "external-body"))
        shallrecurse = false;
    }
    else if (bp->type == TYPE_MULTIPART)
    {
      /* Always recurse multiparts, except multipart/alternative. */
      shallrecurse = true;
      if (mutt_istr_equal(bp->subtype, "alternative"))
      {
        const bool c_count_alternatives = cs_subset_bool(NeoMutt->sub, "count_alternatives");
        shallrecurse = c_count_alternatives;
      }
    }

    if ((bp->disposition == DISP_INLINE) && (bp->type != TYPE_MULTIPART) &&
        (bp->type != TYPE_MESSAGE) && (bp == b))
    {
      shallcount = false; /* ignore fundamental inlines */
    }

    /* If this body isn't scheduled for enumeration already, don't bother
     * profiling it further.  */
    if (shallcount)
    {
      /* Turn off shallcount if message type is not in ok list,
       * or if it is in except list. Check is done separately for
       * inlines vs. attachments.  */

      if (bp->disposition == DISP_ATTACH)
      {
        if (!count_body_parts_check(&AttachAllow, bp, true))
          shallcount = false; /* attach not allowed */
        if (count_body_parts_check(&AttachExclude, bp, false))
          shallcount = false; /* attach excluded */
      }
      else
      {
        if (!count_body_parts_check(&InlineAllow, bp, true))
          shallcount = false; /* inline not allowed */
        if (count_body_parts_check(&InlineExclude, bp, false))
          shallcount = false; /* excluded */
      }
    }

    if (shallcount)
      count++;
    bp->attach_qualifies = shallcount;

    mutt_debug(LL_DEBUG3, "%p shallcount = %d\n", (void *) bp, shallcount);

    if (shallrecurse)
    {
      mutt_debug(LL_DEBUG3, "%p pre count = %d\n", (void *) bp, count);
      bp->attach_count = count_body_parts(bp->parts);
      count += bp->attach_count;
      mutt_debug(LL_DEBUG3, "%p post count = %d\n", (void *) bp, count);
    }
  }

  mutt_debug(LL_DEBUG3, "return %d\n", (count < 0) ? 0 : count);
  return (count < 0) ? 0 : count;
}

/**
 * mutt_count_body_parts - Count the MIME Body parts
 * @param m Mailbox
 * @param e Email
 * @param fp File to parse
 * @retval num Number of MIME Body parts
 */
int mutt_count_body_parts(const struct Mailbox *m, struct Email *e, FILE *fp)
{
  if (!m || !e)
    return 0;

  bool keep_parts = false;

  if (e->attach_valid)
    return e->attach_total;

  if (e->body->parts)
    keep_parts = true;
  else
    mutt_parse_mime_message(e, fp);

  if (!STAILQ_EMPTY(&AttachAllow) || !STAILQ_EMPTY(&AttachExclude) ||
      !STAILQ_EMPTY(&InlineAllow) || !STAILQ_EMPTY(&InlineExclude))
  {
    e->attach_total = count_body_parts(e->body);
  }
  else
  {
    e->attach_total = 0;
  }

  e->attach_valid = true;

  if (!keep_parts)
    mutt_body_free(&e->body->parts);

  return e->attach_total;
}

/**
 * mutt_attachments_reset - Reset the attachment count for all Emails
 * @param mv Mailbox view
 */
void mutt_attachments_reset(struct MailboxView *mv)
{
  if (!mv || !mv->mailbox)
    return;

  struct Mailbox *m = mv->mailbox;

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;
    e->attach_valid = false;
    e->attach_total = 0;
  }
}

/**
 * parse_attach_list - Parse the "attachments" command
 * @param buf  Buffer for temporary storage
 * @param s    Buffer containing the attachments command
 * @param head List of AttachMatch to add to
 * @param err  Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
static enum CommandResult parse_attach_list(struct Buffer *buf, struct Buffer *s,
                                            struct ListHead *head, struct Buffer *err)
{
  struct AttachMatch *a = NULL;
  char *p = NULL;
  char *tmpminor = NULL;
  size_t len;
  int rc;

  do
  {
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);

    if (!buf->data || (*buf->data == '\0'))
      continue;

    a = attachmatch_new();

    /* some cheap hacks that I expect to remove */
    if (mutt_istr_equal(buf->data, "any"))
      a->major = mutt_str_dup("*/.*");
    else if (mutt_istr_equal(buf->data, "none"))
      a->major = mutt_str_dup("cheap_hack/this_should_never_match");
    else
      a->major = mutt_str_dup(buf->data);

    p = strchr(a->major, '/');
    if (p)
    {
      *p = '\0';
      p++;
      a->minor = p;
    }
    else
    {
      a->minor = "unknown";
    }

    len = strlen(a->minor);
    tmpminor = mutt_mem_malloc(len + 3);
    strcpy(&tmpminor[1], a->minor);
    tmpminor[0] = '^';
    tmpminor[len + 1] = '$';
    tmpminor[len + 2] = '\0';

    a->major_int = mutt_check_mime_type(a->major);
    rc = REG_COMP(&a->minor_regex, tmpminor, REG_ICASE);

    FREE(&tmpminor);

    if (rc != 0)
    {
      regerror(rc, &a->minor_regex, err->data, err->dsize);
      FREE(&a->major);
      FREE(&a);
      return MUTT_CMD_ERROR;
    }

    mutt_debug(LL_DEBUG3, "added %s/%s [%d]\n", a->major, a->minor, a->major_int);

    mutt_list_insert_tail(head, (char *) a);
  } while (MoreArgs(s));

  if (!a)
    return MUTT_CMD_ERROR;

  mutt_debug(LL_NOTIFY, "NT_ATTACH_ADD: %s/%s\n", a->major, a->minor);
  notify_send(AttachmentsNotify, NT_ATTACH, NT_ATTACH_ADD, NULL);

  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unattach_list - Parse the "unattachments" command
 * @param buf  Buffer for temporary storage
 * @param s    Buffer containing the unattachments command
 * @param head List of AttachMatch to remove from
 * @param err  Buffer for error messages
 * @retval #MUTT_CMD_SUCCESS Always
 */
static enum CommandResult parse_unattach_list(struct Buffer *buf, struct Buffer *s,
                                              struct ListHead *head, struct Buffer *err)
{
  struct AttachMatch *a = NULL;
  char *tmp = NULL;
  char *minor = NULL;

  do
  {
    parse_extract_token(buf, s, TOKEN_NO_FLAGS);
    FREE(&tmp);

    if (mutt_istr_equal(buf->data, "any"))
      tmp = mutt_str_dup("*/.*");
    else if (mutt_istr_equal(buf->data, "none"))
      tmp = mutt_str_dup("cheap_hack/this_should_never_match");
    else
      tmp = mutt_str_dup(buf->data);

    minor = strchr(tmp, '/');
    if (minor)
    {
      *minor = '\0';
      minor++;
    }
    else
    {
      minor = "unknown";
    }
    const enum ContentType major = mutt_check_mime_type(tmp);

    struct ListNode *np = NULL, *tmp2 = NULL;
    STAILQ_FOREACH_SAFE(np, head, entries, tmp2)
    {
      a = (struct AttachMatch *) np->data;
      mutt_debug(LL_DEBUG3, "check %s/%s [%d] : %s/%s [%d]\n", a->major,
                 a->minor, a->major_int, tmp, minor, major);
      if ((a->major_int == major) && mutt_istr_equal(minor, a->minor))
      {
        mutt_debug(LL_DEBUG3, "removed %s/%s [%d]\n", a->major, a->minor, a->major_int);
        mutt_debug(LL_NOTIFY, "NT_ATTACH_DELETE: %s/%s\n", a->major, a->minor);

        regfree(&a->minor_regex);
        FREE(&a->major);
        STAILQ_REMOVE(head, np, ListNode, entries);
        FREE(&np->data);
        FREE(&np);
      }
    }

  } while (MoreArgs(s));

  FREE(&tmp);

  notify_send(AttachmentsNotify, NT_ATTACH, NT_ATTACH_DELETE, NULL);

  return MUTT_CMD_SUCCESS;
}

/**
 * print_attach_list - Print a list of attachments
 * @param h    List of attachments
 * @param op   Operation, e.g. '+', '-'
 * @param name Attached/Inline, 'A', 'I'
 * @retval 0 Always
 */
static int print_attach_list(struct ListHead *h, const char op, const char *name)
{
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, h, entries)
  {
    printf("attachments %c%s %s/%s\n", op, name,
           ((struct AttachMatch *) np->data)->major,
           ((struct AttachMatch *) np->data)->minor);
  }

  return 0;
}

/**
 * parse_attachments - Parse the 'attachments' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult parse_attachments(struct Buffer *buf, struct Buffer *s,
                                     intptr_t data, struct Buffer *err)
{
  parse_extract_token(buf, s, TOKEN_NO_FLAGS);
  if (!buf->data || (*buf->data == '\0'))
  {
    buf_strcpy(err, _("attachments: no disposition"));
    return MUTT_CMD_WARNING;
  }

  char *category = buf->data;
  char op = *category++;

  if (op == '?')
  {
    mutt_endwin();
    fflush(stdout);
    printf("\n%s\n\n", _("Current attachments settings:"));
    print_attach_list(&AttachAllow, '+', "A");
    print_attach_list(&AttachExclude, '-', "A");
    print_attach_list(&InlineAllow, '+', "I");
    print_attach_list(&InlineExclude, '-', "I");
    mutt_any_key_to_continue(NULL);
    return MUTT_CMD_SUCCESS;
  }

  if ((op != '+') && (op != '-'))
  {
    op = '+';
    category--;
  }

  struct ListHead *head = NULL;
  if (mutt_istr_startswith("attachment", category))
  {
    if (op == '+')
      head = &AttachAllow;
    else
      head = &AttachExclude;
  }
  else if (mutt_istr_startswith("inline", category))
  {
    if (op == '+')
      head = &InlineAllow;
    else
      head = &InlineExclude;
  }
  else
  {
    buf_strcpy(err, _("attachments: invalid disposition"));
    return MUTT_CMD_ERROR;
  }

  return parse_attach_list(buf, s, head, err);
}

/**
 * parse_unattachments - Parse the 'unattachments' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult parse_unattachments(struct Buffer *buf, struct Buffer *s,
                                       intptr_t data, struct Buffer *err)
{
  char op;
  char *p = NULL;
  struct ListHead *head = NULL;

  parse_extract_token(buf, s, TOKEN_NO_FLAGS);
  if (!buf->data || (*buf->data == '\0'))
  {
    buf_strcpy(err, _("unattachments: no disposition"));
    return MUTT_CMD_WARNING;
  }

  p = buf->data;
  op = *p++;

  if (op == '*')
  {
    mutt_list_free_type(&AttachAllow, (list_free_t) attachmatch_free);
    mutt_list_free_type(&AttachExclude, (list_free_t) attachmatch_free);
    mutt_list_free_type(&InlineAllow, (list_free_t) attachmatch_free);
    mutt_list_free_type(&InlineExclude, (list_free_t) attachmatch_free);

    mutt_debug(LL_NOTIFY, "NT_ATTACH_DELETE_ALL\n");
    notify_send(AttachmentsNotify, NT_ATTACH, NT_ATTACH_DELETE_ALL, NULL);
    return 0;
  }

  if ((op != '+') && (op != '-'))
  {
    op = '+';
    p--;
  }
  if (mutt_istr_startswith("attachment", p))
  {
    if (op == '+')
      head = &AttachAllow;
    else
      head = &AttachExclude;
  }
  else if (mutt_istr_startswith("inline", p))
  {
    if (op == '+')
      head = &InlineAllow;
    else
      head = &InlineExclude;
  }
  else
  {
    buf_strcpy(err, _("unattachments: invalid disposition"));
    return MUTT_CMD_ERROR;
  }

  return parse_unattach_list(buf, s, head, err);
}

/**
 * mutt_parse_mime_message - Parse a MIME email
 * @param e Email
 * @param fp File to parse
 */
void mutt_parse_mime_message(struct Email *e, FILE *fp)
{
  const bool right_type = (e->body->type == TYPE_MESSAGE) ||
                          (e->body->type == TYPE_MULTIPART);
  const bool not_parsed = (e->body->parts == NULL);

  if (right_type && fp && not_parsed)
  {
    mutt_parse_part(fp, e->body);
    if (WithCrypto)
    {
      e->security = crypt_query(e->body);
    }
  }

  e->attach_valid = false;
}
