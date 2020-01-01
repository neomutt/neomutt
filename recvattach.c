/**
 * @file
 * Routines for managing attachments
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2007,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2006 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page recvattach Routines for managing attachments
 *
 * Routines for managing attachments
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "recvattach.h"
#include "commands.h"
#include "context.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "handler.h"
#include "hdrline.h"
#include "hook.h"
#include "keymap.h"
#include "mailcap.h"
#include "mutt_attach.h"
#include "mutt_menu.h"
#include "mutt_parse.h"
#include "muttlib.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "recvcmd.h"
#include "send.h"
#include "sendlib.h"
#include "state.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/* These Config Variables are only used in recvattach.c */
char *C_AttachSaveDir; ///< Config: Default directory where attachments are saved
char *C_AttachSaveWithoutPrompting; ///< Config: If true, then don't prompt to save
char *C_AttachSep; ///< Config: Separator to add between saved/printed/piped attachments
bool C_AttachSplit;    ///< Config: Save/print/pipe tagged messages individually
bool C_DigestCollapse; ///< Config: Hide the subparts of a multipart/digest
char *C_MessageFormat; ///< Config: printf-like format string for listing attached messages

static void mutt_update_recvattach_menu(struct AttachCtx *actx, struct Menu *menu, bool init);

static const char *Mailbox_is_read_only = N_("Mailbox is read-only");

#define CHECK_READONLY                                                         \
  if (!Context || !Context->mailbox || Context->mailbox->readonly)             \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(Mailbox_is_read_only));                                       \
    break;                                                                     \
  }

#define CUR_ATTACH actx->idx[actx->v2r[menu->current]]

static const struct Mapping AttachHelp[] = {
  { N_("Exit"), OP_EXIT },   { N_("Save"), OP_SAVE }, { N_("Pipe"), OP_PIPE },
  { N_("Print"), OP_PRINT }, { N_("Help"), OP_HELP }, { NULL, 0 },
};

static const char *Function_not_permitted =
    N_("Function not permitted in attach-message mode");

#define CHECK_ATTACH                                                           \
  if (OptAttachMsg)                                                            \
  {                                                                            \
    mutt_flushinp();                                                           \
    mutt_error(_(Function_not_permitted));                                     \
    break;                                                                     \
  }

/**
 * mutt_update_v2r - Update the virtual list of attachments
 * @param actx Attachment context
 *
 * Update the record of the number of attachments and the status of the tree.
 */
static void mutt_update_v2r(struct AttachCtx *actx)
{
  int vindex, rindex, curlevel;

  vindex = 0;
  rindex = 0;

  while (rindex < actx->idxlen)
  {
    actx->v2r[vindex++] = rindex;
    if (actx->idx[rindex]->content->collapsed)
    {
      curlevel = actx->idx[rindex]->level;
      do
      {
        rindex++;
      } while ((rindex < actx->idxlen) && (actx->idx[rindex]->level > curlevel));
    }
    else
      rindex++;
  }

  actx->vcount = vindex;
}

/**
 * mutt_update_tree - Refresh the list of attachments
 * @param actx Attachment context
 */
void mutt_update_tree(struct AttachCtx *actx)
{
  char buf[256];
  char *s = NULL;

  mutt_update_v2r(actx);

  for (int vindex = 0; vindex < actx->vcount; vindex++)
  {
    const int rindex = actx->v2r[vindex];
    actx->idx[rindex]->num = vindex;
    if ((2 * (actx->idx[rindex]->level + 2)) < sizeof(buf))
    {
      if (actx->idx[rindex]->level)
      {
        s = buf + 2 * (actx->idx[rindex]->level - 1);
        *s++ = (actx->idx[rindex]->content->next) ? MUTT_TREE_LTEE : MUTT_TREE_LLCORNER;
        *s++ = MUTT_TREE_HLINE;
        *s++ = MUTT_TREE_RARROW;
      }
      else
        s = buf;
      *s = '\0';
    }

    if (actx->idx[rindex]->tree)
    {
      if (mutt_str_strcmp(actx->idx[rindex]->tree, buf) != 0)
        mutt_str_replace(&actx->idx[rindex]->tree, buf);
    }
    else
      actx->idx[rindex]->tree = mutt_str_strdup(buf);

    if (((2 * (actx->idx[rindex]->level + 2)) < sizeof(buf)) &&
        actx->idx[rindex]->level)
    {
      s = buf + 2 * (actx->idx[rindex]->level - 1);
      *s++ = (actx->idx[rindex]->content->next) ? '\005' : '\006';
      *s++ = '\006';
    }
  }
}

/**
 * attach_format_str - Format a string for the attachment menu - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%C     | Character set
 * | \%c     | Character set: convert?
 * | \%D     | Deleted flag
 * | \%d     | Description
 * | \%e     | MIME content-transfer-encoding
 * | \%f     | Filename
 * | \%F     | Filename for content-disposition header
 * | \%I     | Content-disposition, either I (inline) or A (attachment)
 * | \%m     | Major MIME type
 * | \%M     | MIME subtype
 * | \%n     | Attachment number
 * | \%Q     | 'Q', if MIME part qualifies for attachment counting
 * | \%s     | Size
 * | \%t     | Tagged flag
 * | \%T     | Tree chars
 * | \%u     | Unlink
 * | \%X     | Number of qualifying MIME parts in this part and its children
 */
const char *attach_format_str(char *buf, size_t buflen, size_t col, int cols,
                              char op, const char *src, const char *prec,
                              const char *if_str, const char *else_str,
                              unsigned long data, MuttFormatFlags flags)
{
  char fmt[128];
  char charset[128];
  struct AttachPtr *aptr = (struct AttachPtr *) data;
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'C':
      if (!optional)
      {
        if (mutt_is_text_part(aptr->content) &&
            mutt_body_get_charset(aptr->content, charset, sizeof(charset)))
        {
          mutt_format_s(buf, buflen, prec, charset);
        }
        else
          mutt_format_s(buf, buflen, prec, "");
      }
      else if (!mutt_is_text_part(aptr->content) ||
               !mutt_body_get_charset(aptr->content, charset, sizeof(charset)))
      {
        optional = false;
      }
      break;
    case 'c':
      /* XXX */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        snprintf(buf, buflen, fmt,
                 ((aptr->content->type != TYPE_TEXT) || aptr->content->noconv) ? 'n' : 'c');
      }
      else if ((aptr->content->type != TYPE_TEXT) || aptr->content->noconv)
        optional = false;
      break;
    case 'd':
      if (!optional)
      {
        if (aptr->content->description)
        {
          mutt_format_s(buf, buflen, prec, aptr->content->description);
          break;
        }
        if (mutt_is_message_type(aptr->content->type, aptr->content->subtype) &&
            C_MessageFormat && aptr->content->email)
        {
          char s[128];
          mutt_make_string_flags(s, sizeof(s), cols, C_MessageFormat, NULL,
                                 NULL, aptr->content->email,
                                 MUTT_FORMAT_FORCESUBJ | MUTT_FORMAT_ARROWCURSOR);
          if (*s)
          {
            mutt_format_s(buf, buflen, prec, s);
            break;
          }
        }
        if (!aptr->content->d_filename && !aptr->content->filename)
        {
          mutt_format_s(buf, buflen, prec, "<no description>");
          break;
        }
      }
      else if (aptr->content->description ||
               (mutt_is_message_type(aptr->content->type, aptr->content->subtype) &&
                C_MessageFormat && aptr->content->email))
      {
        break;
      }
    /* fallthrough */
    case 'F':
      if (!optional)
      {
        if (aptr->content->d_filename)
        {
          mutt_format_s(buf, buflen, prec, aptr->content->d_filename);
          break;
        }
      }
      else if (!aptr->content->d_filename && !aptr->content->filename)
      {
        optional = false;
        break;
      }
    /* fallthrough */
    case 'f':
      if (!optional)
      {
        if (aptr->content->filename && (*aptr->content->filename == '/'))
        {
          struct Buffer *path = mutt_buffer_pool_get();

          mutt_buffer_strcpy(path, aptr->content->filename);
          mutt_buffer_pretty_mailbox(path);
          mutt_format_s(buf, buflen, prec, mutt_b2s(path));
          mutt_buffer_pool_release(&path);
        }
        else
          mutt_format_s(buf, buflen, prec, NONULL(aptr->content->filename));
      }
      else if (!aptr->content->filename)
        optional = false;
      break;
    case 'D':
      if (!optional)
        snprintf(buf, buflen, "%c", aptr->content->deleted ? 'D' : ' ');
      else if (!aptr->content->deleted)
        optional = false;
      break;
    case 'e':
      if (!optional)
        mutt_format_s(buf, buflen, prec, ENCODING(aptr->content->encoding));
      break;
    case 'I':
      if (!optional)
      {
        const char dispchar[] = { 'I', 'A', 'F', '-' };
        char ch;

        if (aptr->content->disposition < sizeof(dispchar))
          ch = dispchar[aptr->content->disposition];
        else
        {
          mutt_debug(LL_DEBUG1, "ERROR: invalid content-disposition %d\n",
                     aptr->content->disposition);
          ch = '!';
        }
        snprintf(buf, buflen, "%c", ch);
      }
      break;
    case 'm':
      if (!optional)
        mutt_format_s(buf, buflen, prec, TYPE(aptr->content));
      break;
    case 'M':
      if (!optional)
        mutt_format_s(buf, buflen, prec, aptr->content->subtype);
      else if (!aptr->content->subtype)
        optional = false;
      break;
    case 'n':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, aptr->num + 1);
      }
      break;
    case 'Q':
      if (optional)
        optional = aptr->content->attach_qualifies;
      else
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        mutt_format_s(buf, buflen, fmt, "Q");
      }
      break;
    case 's':
    {
      size_t l;
      if (flags & MUTT_FORMAT_STAT_FILE)
      {
        struct stat st;
        stat(aptr->content->filename, &st);
        l = st.st_size;
      }
      else
        l = aptr->content->length;

      if (!optional)
      {
        char tmp[128];
        mutt_str_pretty_size(tmp, sizeof(tmp), l);
        mutt_format_s(buf, buflen, prec, tmp);
      }
      else if (l == 0)
        optional = false;

      break;
    }
    case 't':
      if (!optional)
        snprintf(buf, buflen, "%c", aptr->content->tagged ? '*' : ' ');
      else if (!aptr->content->tagged)
        optional = false;
      break;
    case 'T':
      if (!optional)
        mutt_format_s_tree(buf, buflen, prec, NONULL(aptr->tree));
      else if (!aptr->tree)
        optional = false;
      break;
    case 'u':
      if (!optional)
        snprintf(buf, buflen, "%c", aptr->content->unlink ? '-' : ' ');
      else if (!aptr->content->unlink)
        optional = false;
      break;
    case 'X':
      if (optional)
        optional = ((aptr->content->attach_count + aptr->content->attach_qualifies) != 0);
      else
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, aptr->content->attach_count + aptr->content->attach_qualifies);
      }
      break;
    default:
      *buf = '\0';
  }

  if (optional)
    mutt_expando_format(buf, buflen, col, cols, if_str, attach_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(buf, buflen, col, cols, else_str, attach_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);
  return src;
}

/**
 * attach_make_entry - Format a menu item for the attachment list - Implements Menu::menu_make_entry()
 */
static void attach_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  struct AttachCtx *actx = menu->data;

  mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols,
                      NONULL(C_AttachFormat), attach_format_str,
                      (unsigned long) (actx->idx[actx->v2r[line]]), MUTT_FORMAT_ARROWCURSOR);
}

/**
 * attach_tag - Tag an attachment - Implements Menu::menu_tag()
 */
int attach_tag(struct Menu *menu, int sel, int act)
{
  struct AttachCtx *actx = menu->data;
  struct Body *cur = actx->idx[actx->v2r[sel]]->content;
  bool ot = cur->tagged;

  cur->tagged = ((act >= 0) ? act : !cur->tagged);
  return cur->tagged - ot;
}

/**
 * prepend_savedir - Add #C_AttachSaveDir to the beginning of a path
 * @param buf Buffer for the result
 */
static void prepend_savedir(struct Buffer *buf)
{
  if (!buf || !buf->data || (buf->data[0] == '/'))
    return;

  struct Buffer *tmp = mutt_buffer_pool_get();
  if (C_AttachSaveDir)
  {
    mutt_buffer_addstr(tmp, C_AttachSaveDir);
    if (tmp->dptr[-1] != '/')
      mutt_buffer_addch(tmp, '/');
  }
  else
    mutt_buffer_addstr(tmp, "./");

  mutt_buffer_addstr(tmp, mutt_b2s(buf));
  mutt_buffer_copy(buf, tmp);
  mutt_buffer_pool_release(&tmp);
}

/**
 * has_a_message - Determine if the Body has a message (to save)
 * @param[in]  body Body of the message
 * @retval true if suitable for saving
 */
static bool has_a_message(struct Body *body)
{
  return (body->email && (body->encoding != ENC_BASE64) &&
          (body->encoding != ENC_QUOTED_PRINTABLE) &&
          mutt_is_message_type(body->type, body->subtype));
}

/**
 * query_save_attachment - Ask the user if we should save the attachment
 * @param[in]  fp        File handle to the attachment (OPTIONAL)
 * @param[in]  body      Attachment
 * @param[in]  e       Email
 * @param[out] directory Where the attachment was saved
 * @retval  0 Success
 * @retval -1 Failure
 */
static int query_save_attachment(FILE *fp, struct Body *body, struct Email *e, char **directory)
{
  char *prompt = NULL;
  enum SaveAttach opt = MUTT_SAVE_NO_FLAGS;
  int rc = -1;

  struct Buffer *buf = mutt_buffer_pool_get();
  struct Buffer *tfile = mutt_buffer_pool_get();

  if (body->filename)
  {
    if (directory && *directory)
    {
      mutt_buffer_concat_path(buf, *directory, mutt_path_basename(body->filename));
    }
    else
      mutt_buffer_strcpy(buf, body->filename);
  }
  else if (has_a_message(body))
  {
    mutt_default_save(buf->data, buf->dsize, body->email);
    mutt_buffer_fix_dptr(buf);
  }

  prepend_savedir(buf);

  prompt = _("Save to file: ");
  while (prompt)
  {
    if ((mutt_buffer_get_field(prompt, buf, MUTT_FILE | MUTT_CLEAR) != 0) ||
        mutt_buffer_is_empty(buf))
    {
      goto cleanup;
    }

    prompt = NULL;
    mutt_buffer_expand_path(buf);

    bool is_message = (fp && has_a_message(body));

    if (is_message)
    {
      struct stat st;

      /* check to make sure that this file is really the one the user wants */
      rc = mutt_save_confirm(mutt_b2s(buf), &st);
      if (rc == 1)
      {
        prompt = _("Save to file: ");
        continue;
      }
      else if (rc == -1)
        goto cleanup;
      mutt_buffer_copy(tfile, buf);
    }
    else
    {
      rc = mutt_check_overwrite(body->filename, mutt_b2s(buf), tfile, &opt, directory);
      if (rc == -1)
        goto cleanup;
      else if (rc == 1)
      {
        prompt = _("Save to file: ");
        continue;
      }
    }

    mutt_message(_("Saving..."));
    if (mutt_save_attachment(fp, body, mutt_b2s(tfile), opt,
                             (e || !is_message) ? e : body->email) == 0)
    {
      mutt_message(_("Attachment saved"));
      rc = 0;
      goto cleanup;
    }
    else
    {
      prompt = _("Save to file: ");
      continue;
    }
  }

cleanup:
  mutt_buffer_pool_release(&buf);
  mutt_buffer_pool_release(&tfile);
  return rc;
}

/**
 * save_without_prompting - Save the attachment, without prompting each time.
 * @param[in]  fp   File handle to the attachment (OPTIONAL)
 * @param[in]  body Attachment
 * @param[in]  e    Email
 * @retval  0 Success
 * @retval -1 Failure
 */
static int save_without_prompting(FILE *fp, struct Body *body, struct Email *e)
{
  enum SaveAttach opt = MUTT_SAVE_NO_FLAGS;
  int rc = -1;
  struct Buffer *buf = mutt_buffer_pool_get();
  struct Buffer *tfile = mutt_buffer_pool_get();

  if (body->filename)
  {
    mutt_buffer_strcpy(buf, body->filename);
  }
  else if (has_a_message(body))
  {
    mutt_default_save(buf->data, buf->dsize, body->email);
  }

  prepend_savedir(buf);
  mutt_buffer_expand_path(buf);

  bool is_message = (fp && has_a_message(body));

  if (is_message)
  {
    mutt_buffer_copy(tfile, buf);
  }
  else
  {
    rc = mutt_check_overwrite(body->filename, mutt_b2s(buf), tfile, &opt, NULL);
    if (rc == -1) // abort or cancel
      goto cleanup;
  }

  rc = mutt_save_attachment(fp, body, mutt_b2s(tfile), opt,
                            (e || !is_message) ? e : body->email);

cleanup:
  mutt_buffer_pool_release(&buf);
  mutt_buffer_pool_release(&tfile);
  return rc;
}

/**
 * mutt_save_attachment_list - Save a list of attachments
 * @param actx Attachment context
 * @param fp   File handle for the attachment (OPTIONAL)
 * @param tag  If true, only save the tagged attachments
 * @param top  First Attachment
 * @param e  Email
 * @param menu Menu listing attachments
 */
void mutt_save_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag,
                               struct Body *top, struct Email *e, struct Menu *menu)
{
  char *directory = NULL;
  int rc = 1;
  int last = menu ? menu->current : -1;
  FILE *fp_out = NULL;
  int saved_attachments = 0;

  struct Buffer *buf = mutt_buffer_pool_get();
  struct Buffer *tfile = mutt_buffer_pool_get();

  for (int i = 0; !tag || (i < actx->idxlen); i++)
  {
    if (tag)
    {
      fp = actx->idx[i]->fp;
      top = actx->idx[i]->content;
    }
    if (!tag || top->tagged)
    {
      if (!C_AttachSplit)
      {
        if (mutt_buffer_is_empty(buf))
        {
          enum SaveAttach opt = MUTT_SAVE_NO_FLAGS;

          mutt_buffer_strcpy(buf, mutt_path_basename(NONULL(top->filename)));
          prepend_savedir(buf);

          if ((mutt_buffer_get_field(_("Save to file: "), buf, MUTT_FILE | MUTT_CLEAR) != 0) ||
              mutt_buffer_is_empty(buf))
          {
            goto cleanup;
          }
          mutt_buffer_expand_path(buf);
          if (mutt_check_overwrite(top->filename, mutt_b2s(buf), tfile, &opt, NULL))
            goto cleanup;
          rc = mutt_save_attachment(fp, top, mutt_b2s(tfile), opt, e);
          if ((rc == 0) && C_AttachSep && (fp_out = fopen(mutt_b2s(tfile), "a")))
          {
            fprintf(fp_out, "%s", C_AttachSep);
            mutt_file_fclose(&fp_out);
          }
        }
        else
        {
          rc = mutt_save_attachment(fp, top, mutt_b2s(tfile), MUTT_SAVE_APPEND, e);
          if ((rc == 0) && C_AttachSep && (fp_out = fopen(mutt_b2s(tfile), "a")))
          {
            fprintf(fp_out, "%s", C_AttachSep);
            mutt_file_fclose(&fp_out);
          }
        }
      }
      else
      {
        if (tag && menu && top->aptr)
        {
          menu->oldcurrent = menu->current;
          menu->current = top->aptr->num;
          menu_check_recenter(menu);
          menu->redraw |= REDRAW_MOTION;

          menu_redraw(menu);
        }
        if (C_AttachSaveWithoutPrompting)
        {
          // Save each file, with no prompting, using the configured 'AttachSaveDir'
          rc = save_without_prompting(fp, top, e);
          if (rc == 0)
            saved_attachments++;
        }
        else
        {
          // Save each file, prompting the user for the location each time.
          if (query_save_attachment(fp, top, e, &directory) == -1)
            break;
        }
      }
    }
    if (!tag)
      break;
  }

  FREE(&directory);

  if (tag && menu)
  {
    menu->oldcurrent = menu->current;
    menu->current = last;
    menu_check_recenter(menu);
    menu->redraw |= REDRAW_MOTION;
  }

  if (!C_AttachSplit && (rc == 0))
    mutt_message(_("Attachment saved"));

  if (C_AttachSaveWithoutPrompting && (rc == 0))
  {
    mutt_message(ngettext("Attachment saved", "%d attachments saved", saved_attachments),
                 saved_attachments);
  }

cleanup:
  mutt_buffer_pool_release(&buf);
  mutt_buffer_pool_release(&tfile);
}

/**
 * query_pipe_attachment - Ask the user if we should pipe the attachment
 * @param command Command to pipe the attachment to
 * @param fp      File handle to the attachment (OPTIONAL)
 * @param body    Attachment
 * @param filter  Is this command a filter?
 */
static void query_pipe_attachment(char *command, FILE *fp, struct Body *body, bool filter)
{
  char tfile[PATH_MAX];

  if (filter)
  {
    char warning[PATH_MAX + 256];
    snprintf(warning, sizeof(warning),
             _("WARNING!  You are about to overwrite %s, continue?"), body->filename);
    if (mutt_yesorno(warning, MUTT_NO) != MUTT_YES)
    {
      mutt_window_clearline(MuttMessageWindow, 0);
      return;
    }
    mutt_mktemp(tfile, sizeof(tfile));
  }
  else
    tfile[0] = '\0';

  if (mutt_pipe_attachment(fp, body, command, tfile))
  {
    if (filter)
    {
      mutt_file_unlink(body->filename);
      mutt_file_rename(tfile, body->filename);
      mutt_update_encoding(body);
      mutt_message(_("Attachment filtered"));
    }
  }
  else
  {
    if (filter && tfile[0])
      mutt_file_unlink(tfile);
  }
}

/**
 * pipe_attachment - Pipe the attachment to a command
 * @param fp    File handle to the attachment (OPTIONAL)
 * @param b     Attachment
 * @param state File state for decoding the attachment
 */
static void pipe_attachment(FILE *fp, struct Body *b, struct State *state)
{
  if (!state || !state->fp_out)
    return;

  if (fp)
  {
    state->fp_in = fp;
    mutt_decode_attachment(b, state);
    if (C_AttachSep)
      state_puts(state, C_AttachSep);
  }
  else
  {
    FILE *fp_in = fopen(b->filename, "r");
    if (!fp_in)
    {
      mutt_perror("fopen");
      return;
    }
    mutt_file_copy_stream(fp_in, state->fp_out);
    mutt_file_fclose(&fp_in);
    if (C_AttachSep)
      state_puts(state, C_AttachSep);
  }
}

/**
 * pipe_attachment_list - Pipe a list of attachments to a command
 * @param command Command to pipe the attachment to
 * @param actx    Attachment context
 * @param fp      File handle to the attachment (OPTIONAL)
 * @param tag     If true, only save the tagged attachments
 * @param top     First Attachment
 * @param filter  Is this command a filter?
 * @param state   File state for decoding the attachments
 */
static void pipe_attachment_list(char *command, struct AttachCtx *actx, FILE *fp, bool tag,
                                 struct Body *top, bool filter, struct State *state)
{
  for (int i = 0; !tag || (i < actx->idxlen); i++)
  {
    if (tag)
    {
      fp = actx->idx[i]->fp;
      top = actx->idx[i]->content;
    }
    if (!tag || top->tagged)
    {
      if (!filter && !C_AttachSplit)
        pipe_attachment(fp, top, state);
      else
        query_pipe_attachment(command, fp, top, filter);
    }
    if (!tag)
      break;
  }
}

/**
 * mutt_pipe_attachment_list - Pipe a list of attachments to a command
 * @param actx   Attachment context
 * @param fp     File handle to the attachment (OPTIONAL)
 * @param tag    If true, only save the tagged attachments
 * @param top    First Attachment
 * @param filter Is this command a filter?
 */
void mutt_pipe_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag,
                               struct Body *top, bool filter)
{
  struct State state = { 0 };
  char buf[PATH_MAX];

  if (fp)
    filter = false; /* sanity check: we can't filter in the recv case yet */

  buf[0] = '\0';
  /* perform charset conversion on text attachments when piping */
  state.flags = MUTT_CHARCONV;

  if ((mutt_get_field((filter ? _("Filter through: ") : _("Pipe to: ")), buf,
                      sizeof(buf), MUTT_CMD) != 0) ||
      (buf[0] == '\0'))
  {
    return;
  }

  mutt_expand_path(buf, sizeof(buf));

  if (!filter && !C_AttachSplit)
  {
    mutt_endwin();
    pid_t pid = mutt_create_filter(buf, &state.fp_out, NULL, NULL);
    pipe_attachment_list(buf, actx, fp, tag, top, filter, &state);
    mutt_file_fclose(&state.fp_out);
    if ((mutt_wait_filter(pid) != 0) || C_WaitKey)
      mutt_any_key_to_continue(NULL);
  }
  else
    pipe_attachment_list(buf, actx, fp, tag, top, filter, &state);
}

/**
 * can_print - Do we know how to print this attachment type?
 * @param actx Attachment
 * @param top  Body of email
 * @param tag  Apply to all tagged Attachments
 * @retval true If (all) the Attachment(s) are printable
 */
static bool can_print(struct AttachCtx *actx, struct Body *top, bool tag)
{
  char type[256];

  for (int i = 0; !tag || (i < actx->idxlen); i++)
  {
    if (tag)
      top = actx->idx[i]->content;
    snprintf(type, sizeof(type), "%s/%s", TYPE(top), top->subtype);
    if (!tag || top->tagged)
    {
      if (!mailcap_lookup(top, type, sizeof(type), NULL, MUTT_MC_PRINT))
      {
        if ((mutt_str_strcasecmp("text/plain", top->subtype) != 0) &&
            (mutt_str_strcasecmp("application/postscript", top->subtype) != 0))
        {
          if (!mutt_can_decode(top))
          {
            /* L10N: s gets replaced by a MIME type, e.g. "text/plain" or
               application/octet-stream.  */
            mutt_error(_("I don't know how to print %s attachments"), type);
            return false;
          }
        }
      }
    }
    if (!tag)
      break;
  }
  return true;
}

/**
 * print_attachment_list - Print a list of Attachments
 * @param actx  Attachment context
 * @param fp    File handle to the attachment (OPTIONAL)
 * @param tag   Apply to all tagged Attachments
 * @param top   First Attachment
 * @param state File state for decoding the attachments
 */
static void print_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag,
                                  struct Body *top, struct State *state)
{
  char type[256];

  for (int i = 0; !tag || (i < actx->idxlen); i++)
  {
    if (tag)
    {
      fp = actx->idx[i]->fp;
      top = actx->idx[i]->content;
    }
    if (!tag || top->tagged)
    {
      snprintf(type, sizeof(type), "%s/%s", TYPE(top), top->subtype);
      if (!C_AttachSplit && !mailcap_lookup(top, type, sizeof(type), NULL, MUTT_MC_PRINT))
      {
        if ((mutt_str_strcasecmp("text/plain", top->subtype) == 0) ||
            (mutt_str_strcasecmp("application/postscript", top->subtype) == 0))
        {
          pipe_attachment(fp, top, state);
        }
        else if (mutt_can_decode(top))
        {
          /* decode and print */

          FILE *fp_in = NULL;
          struct Buffer *newfile = mutt_buffer_pool_get();

          mutt_buffer_mktemp(newfile);
          if (mutt_decode_save_attachment(fp, top, mutt_b2s(newfile),
                                          MUTT_PRINTING, MUTT_SAVE_NO_FLAGS) == 0)
          {
            if (!state->fp_out)
            {
              mutt_error(
                  "BUG in print_attachment_list().  Please report this. ");
              return;
            }

            fp_in = fopen(mutt_b2s(newfile), "r");
            if (fp_in)
            {
              mutt_file_copy_stream(fp_in, state->fp_out);
              mutt_file_fclose(&fp_in);
              if (C_AttachSep)
                state_puts(state, C_AttachSep);
            }
          }
          mutt_file_unlink(mutt_b2s(newfile));
          mutt_buffer_pool_release(&newfile);
        }
      }
      else
        mutt_print_attachment(fp, top);
    }
    if (!tag)
      break;
  }
}

/**
 * mutt_print_attachment_list - Print a list of Attachments
 * @param actx Attachment context
 * @param fp   File handle to the attachment (OPTIONAL)
 * @param tag  Apply to all tagged Attachments
 * @param top  First Attachment
 */
void mutt_print_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag, struct Body *top)
{
  char prompt[128];
  struct State state = { 0 };
  int tagmsgcount = 0;

  if (tag)
    for (int i = 0; i < actx->idxlen; i++)
      if (actx->idx[i]->content->tagged)
        tagmsgcount++;

  snprintf(prompt, sizeof(prompt),
           /* L10N: Although we now the precise number of tagged messages, we
              do not show it to the user.  So feel free to use a "generic
              plural" as plural translation if your language has one. */
           tag ? ngettext("Print tagged attachment?", "Print %d tagged attachments?", tagmsgcount) :
                 _("Print attachment?"),
           tagmsgcount);
  if (query_quadoption(C_Print, prompt) != MUTT_YES)
    return;

  if (C_AttachSplit)
  {
    print_attachment_list(actx, fp, tag, top, &state);
  }
  else
  {
    if (!can_print(actx, top, tag))
      return;
    mutt_endwin();
    pid_t pid = mutt_create_filter(NONULL(C_PrintCommand), &state.fp_out, NULL, NULL);
    print_attachment_list(actx, fp, tag, top, &state);
    mutt_file_fclose(&state.fp_out);
    if ((mutt_wait_filter(pid) != 0) || C_WaitKey)
      mutt_any_key_to_continue(NULL);
  }
}

/**
 * recvattach_extract_pgp_keys - Extract PGP keys from attachments
 * @param actx Attachment context
 * @param menu Menu listing attachments
 */
static void recvattach_extract_pgp_keys(struct AttachCtx *actx, struct Menu *menu)
{
  if (!menu->tagprefix)
    crypt_pgp_extract_key_from_attachment(CUR_ATTACH->fp, CUR_ATTACH->content);
  else
  {
    for (int i = 0; i < actx->idxlen; i++)
    {
      if (actx->idx[i]->content->tagged)
      {
        crypt_pgp_extract_key_from_attachment(actx->idx[i]->fp, actx->idx[i]->content);
      }
    }
  }
}

/**
 * recvattach_pgp_check_traditional - Is the Attachment inline PGP?
 * @param actx Attachment to check
 * @param menu Menu listing Attachments
 * @retval 1 If the (tagged) Attachment(s) are inline PGP
 *
 * @note If the menu->tagprefix is set, all the tagged attachments will be checked.
 */
static int recvattach_pgp_check_traditional(struct AttachCtx *actx, struct Menu *menu)
{
  int rc = 0;

  if (!menu->tagprefix)
    rc = crypt_pgp_check_traditional(CUR_ATTACH->fp, CUR_ATTACH->content, true);
  else
  {
    for (int i = 0; i < actx->idxlen; i++)
      if (actx->idx[i]->content->tagged)
        rc = rc || crypt_pgp_check_traditional(actx->idx[i]->fp, actx->idx[i]->content, true);
  }

  return rc;
}

/**
 * recvattach_edit_content_type - Edit the content type of an attachment
 * @param actx Attachment context
 * @param menu Menu listing Attachments
 * @param e  Email
 */
static void recvattach_edit_content_type(struct AttachCtx *actx,
                                         struct Menu *menu, struct Email *e)
{
  if (!mutt_edit_content_type(e, CUR_ATTACH->content, CUR_ATTACH->fp))
    return;

  /* The mutt_update_recvattach_menu() will overwrite any changes
   * made to a decrypted CUR_ATTACH->content, so warn the user. */
  if (CUR_ATTACH->decrypted)
  {
    mutt_message(
        _("Structural changes to decrypted attachments are not supported"));
    mutt_sleep(1);
  }
  /* Editing the content type can rewrite the body structure. */
  for (int i = 0; i < actx->idxlen; i++)
    actx->idx[i]->content = NULL;
  mutt_actx_entries_free(actx);
  mutt_update_recvattach_menu(actx, menu, true);
}

/**
 * mutt_attach_display_loop - Event loop for the Attachment menu
 * @param menu Menu listing Attachments
 * @param op   Operation, e.g. OP_VIEW_ATTACH
 * @param e  Email
 * @param actx Attachment context
 * @param recv true if these are received attachments (rather than in compose)
 * @retval num Operation performed
 */
int mutt_attach_display_loop(struct Menu *menu, int op, struct Email *e,
                             struct AttachCtx *actx, bool recv)
{
  do
  {
    switch (op)
    {
      case OP_DISPLAY_HEADERS:
        bool_str_toggle(NeoMutt->sub, "weed", NULL);
        /* fallthrough */

      case OP_VIEW_ATTACH:
        op = mutt_view_attachment(CUR_ATTACH->fp, CUR_ATTACH->content,
                                  MUTT_VA_REGULAR, e, actx, menu->win_index);
        break;

      case OP_NEXT_ENTRY:
      case OP_MAIN_NEXT_UNDELETED: /* hack */
        if (menu->current < menu->max - 1)
        {
          menu->current++;
          op = OP_VIEW_ATTACH;
        }
        else
          op = OP_NULL;
        break;
      case OP_PREV_ENTRY:
      case OP_MAIN_PREV_UNDELETED: /* hack */
        if (menu->current > 0)
        {
          menu->current--;
          op = OP_VIEW_ATTACH;
        }
        else
          op = OP_NULL;
        break;
      case OP_EDIT_TYPE:
        /* when we edit the content-type, we should redisplay the attachment
         * immediately */
        mutt_edit_content_type(e, CUR_ATTACH->content, CUR_ATTACH->fp);
        if (recv)
          recvattach_edit_content_type(actx, menu, e);
        else
          mutt_edit_content_type(e, CUR_ATTACH->content, CUR_ATTACH->fp);

        menu->redraw |= REDRAW_INDEX;
        op = OP_VIEW_ATTACH;
        break;
      /* functions which are passed through from the pager */
      case OP_CHECK_TRADITIONAL:
        if (!(WithCrypto & APPLICATION_PGP) || (e && e->security & PGP_TRADITIONAL_CHECKED))
        {
          op = OP_NULL;
          break;
        }
      /* fallthrough */
      case OP_ATTACH_COLLAPSE:
        if (recv)
          return op;
      /* fallthrough */
      default:
        op = OP_NULL;
    }
  } while (op != OP_NULL);

  return op;
}

/**
 * mutt_generate_recvattach_list - Create a list of attachments
 * @param actx        Attachment context
 * @param e           Email
 * @param parts       Body of email
 * @param fp          File to read from
 * @param parent_type Type, e.g. #TYPE_MULTIPART
 * @param level       Attachment depth
 * @param decrypted   True if attachment has been decrypted
 */
void mutt_generate_recvattach_list(struct AttachCtx *actx, struct Email *e,
                                   struct Body *parts, FILE *fp,
                                   int parent_type, int level, bool decrypted)
{
  struct Body *m = NULL;
  struct Body *new_body = NULL;
  FILE *fp_new = NULL;
  SecurityFlags type;
  int need_secured, secured;

  for (m = parts; m; m = m->next)
  {
    need_secured = 0;
    secured = 0;

    if (((WithCrypto & APPLICATION_SMIME) != 0) && (type = mutt_is_application_smime(m)))
    {
      need_secured = 1;

      if (type & SEC_ENCRYPT)
      {
        if (!crypt_valid_passphrase(APPLICATION_SMIME))
          goto decrypt_failed;

        if (e->env)
          crypt_smime_getkeys(e->env);
      }

      secured = !crypt_smime_decrypt_mime(fp, &fp_new, m, &new_body);
      /* If the decrypt/verify-opaque doesn't generate mime output, an empty
       * text/plain type will still be returned by mutt_read_mime_header().
       * We can't distinguish an actual part from a failure, so only use a
       * text/plain that results from a single top-level part. */
      if (secured && (new_body->type == TYPE_TEXT) &&
          (mutt_str_strcasecmp("plain", new_body->subtype) == 0) &&
          ((parts != m) || m->next))
      {
        mutt_body_free(&new_body);
        mutt_file_fclose(&fp_new);
        goto decrypt_failed;
      }

      if (secured && (type & SEC_ENCRYPT))
        e->security |= SMIME_ENCRYPT;
    }

    if (((WithCrypto & APPLICATION_PGP) != 0) &&
        (mutt_is_multipart_encrypted(m) || mutt_is_malformed_multipart_pgp_encrypted(m)))
    {
      need_secured = 1;

      if (!crypt_valid_passphrase(APPLICATION_PGP))
        goto decrypt_failed;

      secured = !crypt_pgp_decrypt_mime(fp, &fp_new, m, &new_body);

      if (secured)
        e->security |= PGP_ENCRYPT;
    }

    if (need_secured && secured)
    {
      mutt_actx_add_fp(actx, fp_new);
      mutt_actx_add_body(actx, new_body);
      mutt_generate_recvattach_list(actx, e, new_body, fp_new, parent_type, level, 1);
      continue;
    }

  decrypt_failed:
    /* Fall through and show the original parts if decryption fails */
    if (need_secured && !secured)
      mutt_error(_("Can't decrypt encrypted message"));

    /* Strip out the top level multipart */
    if ((m->type == TYPE_MULTIPART) && m->parts && !need_secured &&
        ((parent_type == -1) && mutt_str_strcasecmp("alternative", m->subtype)))
    {
      mutt_generate_recvattach_list(actx, e, m->parts, fp, m->type, level, decrypted);
    }
    else
    {
      struct AttachPtr *ap = mutt_mem_calloc(1, sizeof(struct AttachPtr));
      mutt_actx_add_attach(actx, ap);

      ap->content = m;
      ap->fp = fp;
      m->aptr = ap;
      ap->parent_type = parent_type;
      ap->level = level;
      ap->decrypted = decrypted;

      if (m->type == TYPE_MULTIPART)
        mutt_generate_recvattach_list(actx, e, m->parts, fp, m->type, level + 1, decrypted);
      else if (mutt_is_message_type(m->type, m->subtype))
      {
        mutt_generate_recvattach_list(actx, m->email, m->parts, fp, m->type,
                                      level + 1, decrypted);
        e->security |= m->email->security;
      }
    }
  }
}

/**
 * mutt_attach_init - Create a new Attachment context
 * @param actx Attachment context
 */
void mutt_attach_init(struct AttachCtx *actx)
{
  /* Collapse the attachments if '$digest_collapse' is set AND if...
   * the outer container is of type 'multipart/digest' */
  bool digest = (mutt_str_strcasecmp(actx->email->content->subtype, "digest") == 0);

  for (int i = 0; i < actx->idxlen; i++)
  {
    actx->idx[i]->content->tagged = false;

    /* OR an inner container is of type 'multipart/digest' */
    actx->idx[i]->content->collapsed =
        (C_DigestCollapse &&
         (digest ||
          ((actx->idx[i]->content->type == TYPE_MULTIPART) &&
           (mutt_str_strcasecmp(actx->idx[i]->content->subtype, "digest") == 0))));
  }
}

/**
 * mutt_update_recvattach_menu - Update the Attachment Menu
 * @param actx Attachment context
 * @param menu Menu listing Attachments
 * @param init If true, create a new Attachments context
 */
static void mutt_update_recvattach_menu(struct AttachCtx *actx, struct Menu *menu, bool init)
{
  if (init)
  {
    mutt_generate_recvattach_list(actx, actx->email, actx->email->content,
                                  actx->fp_root, -1, 0, 0);
    mutt_attach_init(actx);
    menu->data = actx;
  }

  mutt_update_tree(actx);

  menu->max = actx->vcount;

  if (menu->current >= menu->max)
    menu->current = menu->max - 1;
  menu_check_recenter(menu);
  menu->redraw |= REDRAW_INDEX;
}

/**
 * attach_collapse - Close the tree of the current attachment
 * @param actx Attachment context
 * @param menu Menu listing Attachments
 */
static void attach_collapse(struct AttachCtx *actx, struct Menu *menu)
{
  int rindex, curlevel;

  CUR_ATTACH->content->collapsed = !CUR_ATTACH->content->collapsed;
  /* When expanding, expand all the children too */
  if (CUR_ATTACH->content->collapsed)
    return;

  curlevel = CUR_ATTACH->level;
  rindex = actx->v2r[menu->current] + 1;

  while ((rindex < actx->idxlen) && (actx->idx[rindex]->level > curlevel))
  {
    if (C_DigestCollapse && (actx->idx[rindex]->content->type == TYPE_MULTIPART) &&
        !mutt_str_strcasecmp(actx->idx[rindex]->content->subtype, "digest"))
    {
      actx->idx[rindex]->content->collapsed = true;
    }
    else
    {
      actx->idx[rindex]->content->collapsed = false;
    }
    rindex++;
  }
}

/**
 * mutt_view_attachments - Show the attachments in a Menu
 * @param e Email
 */
void mutt_view_attachments(struct Email *e)
{
  char helpstr[1024];
  int op = OP_NULL;

  struct Mailbox *m = Context ? Context->mailbox : NULL;

  /* make sure we have parsed this message */
  mutt_parse_mime_message(m, e);

  mutt_message_hook(m, e, MUTT_MESSAGE_HOOK);

  struct Message *msg = mx_msg_open(m, e->msgno);
  if (!msg)
    return;

  struct MuttWindow *dlg =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  dlg->type = WT_DIALOG;
  struct MuttWindow *index =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  index->type = WT_INDEX;
  struct MuttWindow *ibar = mutt_window_new(
      MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 1, MUTT_WIN_SIZE_UNLIMITED);
  ibar->type = WT_INDEX_BAR;

  if (C_StatusOnTop)
  {
    mutt_window_add_child(dlg, ibar);
    mutt_window_add_child(dlg, index);
  }
  else
  {
    mutt_window_add_child(dlg, index);
    mutt_window_add_child(dlg, ibar);
  }

  dialog_push(dlg);

  struct Menu *menu = mutt_menu_new(MENU_ATTACH);

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  menu->title = _("Attachments");
  menu->menu_make_entry = attach_make_entry;
  menu->menu_tag = attach_tag;
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_ATTACH, AttachHelp);
  mutt_menu_push_current(menu);

  struct AttachCtx *actx = mutt_actx_new();
  actx->email = e;
  actx->fp_root = msg->fp;
  mutt_update_recvattach_menu(actx, menu, true);

  while (true)
  {
    if (op == OP_NULL)
      op = mutt_menu_loop(menu);
    if (!Context)
      return;
    switch (op)
    {
      case OP_ATTACH_VIEW_MAILCAP:
        mutt_view_attachment(CUR_ATTACH->fp, CUR_ATTACH->content,
                             MUTT_VA_MAILCAP, e, actx, menu->win_index);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_ATTACH_VIEW_TEXT:
        mutt_view_attachment(CUR_ATTACH->fp, CUR_ATTACH->content,
                             MUTT_VA_AS_TEXT, e, actx, menu->win_index);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_DISPLAY_HEADERS:
      case OP_VIEW_ATTACH:
        op = mutt_attach_display_loop(menu, op, e, actx, true);
        menu->redraw = REDRAW_FULL;
        continue;

      case OP_ATTACH_COLLAPSE:
        if (!CUR_ATTACH->content->parts)
        {
          mutt_error(_("There are no subparts to show"));
          break;
        }
        attach_collapse(actx, menu);
        mutt_update_recvattach_menu(actx, menu, false);
        break;

      case OP_FORGET_PASSPHRASE:
        crypt_forget_passphrase();
        break;

      case OP_EXTRACT_KEYS:
        if (WithCrypto & APPLICATION_PGP)
        {
          recvattach_extract_pgp_keys(actx, menu);
          menu->redraw = REDRAW_FULL;
        }
        break;

      case OP_CHECK_TRADITIONAL:
        if (((WithCrypto & APPLICATION_PGP) != 0) &&
            recvattach_pgp_check_traditional(actx, menu))
        {
          e->security = crypt_query(NULL);
          menu->redraw = REDRAW_FULL;
        }
        break;

      case OP_PRINT:
        mutt_print_attachment_list(actx, CUR_ATTACH->fp, menu->tagprefix,
                                   CUR_ATTACH->content);
        break;

      case OP_PIPE:
        mutt_pipe_attachment_list(actx, CUR_ATTACH->fp, menu->tagprefix,
                                  CUR_ATTACH->content, false);
        break;

      case OP_SAVE:
        mutt_save_attachment_list(actx, CUR_ATTACH->fp, menu->tagprefix,
                                  CUR_ATTACH->content, e, menu);

        if (!menu->tagprefix && C_Resolve && (menu->current < menu->max - 1))
          menu->current++;

        menu->redraw = REDRAW_MOTION_RESYNC | REDRAW_FULL;
        break;

      case OP_DELETE:
        CHECK_READONLY;

#ifdef USE_POP
        if (m->magic == MUTT_POP)
        {
          mutt_flushinp();
          mutt_error(_("Can't delete attachment from POP server"));
          break;
        }
#endif

#ifdef USE_NNTP
        if (m->magic == MUTT_NNTP)
        {
          mutt_flushinp();
          mutt_error(_("Can't delete attachment from news server"));
          break;
        }
#endif

        if ((WithCrypto != 0) && (e->security & SEC_ENCRYPT))
        {
          mutt_message(_("Deletion of attachments from encrypted messages is "
                         "unsupported"));
          break;
        }
        if ((WithCrypto != 0) && (e->security & (SEC_SIGN | SEC_PARTSIGN)))
        {
          mutt_message(_("Deletion of attachments from signed messages may "
                         "invalidate the signature"));
        }
        if (!menu->tagprefix)
        {
          if (CUR_ATTACH->parent_type == TYPE_MULTIPART)
          {
            CUR_ATTACH->content->deleted = true;
            if (C_Resolve && (menu->current < menu->max - 1))
            {
              menu->current++;
              menu->redraw = REDRAW_MOTION_RESYNC;
            }
            else
              menu->redraw = REDRAW_CURRENT;
          }
          else
          {
            mutt_message(
                _("Only deletion of multipart attachments is supported"));
          }
        }
        else
        {
          for (int i = 0; i < menu->max; i++)
          {
            if (actx->idx[i]->content->tagged)
            {
              if (actx->idx[i]->parent_type == TYPE_MULTIPART)
              {
                actx->idx[i]->content->deleted = true;
                menu->redraw = REDRAW_INDEX;
              }
              else
              {
                mutt_message(
                    _("Only deletion of multipart attachments is supported"));
              }
            }
          }
        }
        break;

      case OP_UNDELETE:
        CHECK_READONLY;
        if (!menu->tagprefix)
        {
          CUR_ATTACH->content->deleted = false;
          if (C_Resolve && (menu->current < menu->max - 1))
          {
            menu->current++;
            menu->redraw = REDRAW_MOTION_RESYNC;
          }
          else
            menu->redraw = REDRAW_CURRENT;
        }
        else
        {
          for (int i = 0; i < menu->max; i++)
          {
            if (actx->idx[i]->content->tagged)
            {
              actx->idx[i]->content->deleted = false;
              menu->redraw = REDRAW_INDEX;
            }
          }
        }
        break;

      case OP_RESEND:
        CHECK_ATTACH;
        mutt_attach_resend(CUR_ATTACH->fp, actx,
                           menu->tagprefix ? NULL : CUR_ATTACH->content);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_BOUNCE_MESSAGE:
        CHECK_ATTACH;
        mutt_attach_bounce(m, CUR_ATTACH->fp, actx,
                           menu->tagprefix ? NULL : CUR_ATTACH->content);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_FORWARD_MESSAGE:
        CHECK_ATTACH;
        mutt_attach_forward(CUR_ATTACH->fp, e, actx,
                            menu->tagprefix ? NULL : CUR_ATTACH->content, SEND_NO_FLAGS);
        menu->redraw = REDRAW_FULL;
        break;

#ifdef USE_NNTP
      case OP_FORWARD_TO_GROUP:
        CHECK_ATTACH;
        mutt_attach_forward(CUR_ATTACH->fp, e, actx,
                            menu->tagprefix ? NULL : CUR_ATTACH->content, SEND_NEWS);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_FOLLOWUP:
        CHECK_ATTACH;

        if (!CUR_ATTACH->content->email->env->followup_to ||
            (mutt_str_strcasecmp(CUR_ATTACH->content->email->env->followup_to, "poster") != 0) ||
            (query_quadoption(C_FollowupToPoster,
                              _("Reply by mail as poster prefers?")) != MUTT_YES))
        {
          mutt_attach_reply(CUR_ATTACH->fp, e, actx,
                            menu->tagprefix ? NULL : CUR_ATTACH->content,
                            SEND_NEWS | SEND_REPLY);
          menu->redraw = REDRAW_FULL;
          break;
        }
#endif
      /* fallthrough */
      case OP_REPLY:
      case OP_GROUP_REPLY:
      case OP_GROUP_CHAT_REPLY:
      case OP_LIST_REPLY:
      {
        CHECK_ATTACH;

        SendFlags flags = SEND_REPLY;
        if (op == OP_GROUP_REPLY)
          flags |= SEND_GROUP_REPLY;
        else if (op == OP_GROUP_CHAT_REPLY)
          flags |= SEND_GROUP_CHAT_REPLY;
        else if (op == OP_LIST_REPLY)
          flags |= SEND_LIST_REPLY;

        mutt_attach_reply(CUR_ATTACH->fp, e, actx,
                          menu->tagprefix ? NULL : CUR_ATTACH->content, flags);
        menu->redraw = REDRAW_FULL;
        break;
      }

      case OP_COMPOSE_TO_SENDER:
        CHECK_ATTACH;
        mutt_attach_mail_sender(CUR_ATTACH->fp, e, actx,
                                menu->tagprefix ? NULL : CUR_ATTACH->content);
        menu->redraw = REDRAW_FULL;
        break;

      case OP_EDIT_TYPE:
        recvattach_edit_content_type(actx, menu, e);
        menu->redraw |= REDRAW_INDEX;
        break;

      case OP_EXIT:
        mx_msg_close(m, &msg);

        e->attach_del = false;
        for (int i = 0; i < actx->idxlen; i++)
        {
          if (actx->idx[i]->content && actx->idx[i]->content->deleted)
          {
            e->attach_del = true;
            break;
          }
        }
        if (e->attach_del)
          e->changed = true;

        mutt_actx_free(&actx);

        mutt_menu_pop_current(menu);
        mutt_menu_free(&menu);
        dialog_pop();
        mutt_window_free(&dlg);
        return;
    }

    op = OP_NULL;
  }

  /* not reached */
}
