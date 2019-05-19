/**
 * @file
 * GUI editor for an email's headers
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2007,2010,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page compose GUI editor for an email's headers
 *
 * GUI editor for an email's headers
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "conn/conn.h"
#include "mutt.h"
#include "compose.h"
#include "alias.h"
#include "browser.h"
#include "commands.h"
#include "context.h"
#include "curs_lib.h"
#include "edit.h"
#include "format_flags.h"
#include "globals.h"
#include "hook.h"
#include "index.h"
#include "keymap.h"
#include "mailbox.h"
#include "mutt_attach.h"
#include "mutt_curses.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "muttlib.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "recvattach.h"
#include "sendlib.h"
#include "sort.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef MIXMASTER
#include "remailer.h"
#endif
#ifdef USE_NNTP
#include "nntp/nntp.h"
#endif
#ifdef USE_POP
#include "pop/pop.h"
#endif
#ifdef USE_IMAP
#include "imap/imap.h"
#endif

/* These Config Variables are only used in compose.c */
char *C_ComposeFormat; ///< Config: printf-like format string for the Compose panel's status bar
char *C_Ispell; ///< Config: External command to perform spell-checking
unsigned char C_Postpone; ///< Config: Save messages to the #C_Postponed folder

static const char *There_are_no_attachments = N_("There are no attachments");

#define CHECK_COUNT                                                            \
  if (actx->idxlen == 0)                                                       \
  {                                                                            \
    mutt_error(_(There_are_no_attachments));                                   \
    break;                                                                     \
  }

#define CUR_ATTACH actx->idx[actx->v2r[menu->current]]

/**
 * enum HeaderField - Ordered list of headers for the compose screen
 *
 * The position of various fields on the compose screen.
 */
enum HeaderField
{
  HDR_FROM = 0, ///< "From:" field
  HDR_TO,       ///< "To:" field
  HDR_CC,       ///< "Cc:" field
  HDR_BCC,      ///< "Bcc:" field
  HDR_SUBJECT,  ///< "Subject:" field
  HDR_REPLYTO,  ///< "Reply-To:" field
  HDR_FCC,      ///< "Fcc:" (save folder) field
#ifdef MIXMASTER
  HDR_MIX, ///< "Mix:" field (Mixmaster chain)
#endif
  HDR_CRYPT,     ///< "Security:" field (encryption/signing info)
  HDR_CRYPTINFO, ///< "Sign as:" field (encryption/signing info)
#ifdef USE_NNTP
  HDR_NEWSGROUPS, ///< "Newsgroups:" field
  HDR_FOLLOWUPTO, ///< "Followup-To:" field
  HDR_XCOMMENTTO, ///< "X-Comment-To:" field
#endif
  HDR_ATTACH = (HDR_FCC + 5), ///< Position to start printing the attachments
};

int HeaderPadding[HDR_XCOMMENTTO + 1] = { 0 };
int MaxHeaderWidth = 0;

#define HDR_XOFFSET MaxHeaderWidth
#define W (MuttIndexWindow->cols - MaxHeaderWidth)

static const char *const Prompts[] = {
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
  /* L10N:
     This string is used by the compose menu.
     Since it is hidden by default, it does not increase the
     indentation of other compose menu fields.  However, if possible,
     it should not be longer than the other compose menu fields.
     Since it shares the row with "Encrypt with:", it should not be longer
     than 15-20 character cells.  */
  N_("Sign as: "),
#ifdef USE_NNTP
  /* L10N: Compose menu field.  May not want to translate. */
  N_("Newsgroups: "),
  /* L10N: Compose menu field.  May not want to translate. */
  N_("Followup-To: "),
  /* L10N: Compose menu field.  May not want to translate. */
  N_("X-Comment-To: "),
#endif
};

static const struct Mapping ComposeHelp[] = {
  { N_("Send"), OP_COMPOSE_SEND_MESSAGE },
  { N_("Abort"), OP_EXIT },
  /* L10N: compose menu help line entry */
  { N_("To"), OP_COMPOSE_EDIT_TO },
  /* L10N: compose menu help line entry */
  { N_("CC"), OP_COMPOSE_EDIT_CC },
  /* L10N: compose menu help line entry */
  { N_("Subj"), OP_COMPOSE_EDIT_SUBJECT },
  { N_("Attach file"), OP_COMPOSE_ATTACH_FILE },
  { N_("Descrip"), OP_COMPOSE_EDIT_DESCRIPTION },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};

#ifdef USE_NNTP
static struct Mapping ComposeNewsHelp[] = {
  { N_("Send"), OP_COMPOSE_SEND_MESSAGE },
  { N_("Abort"), OP_EXIT },
  { N_("Newsgroups"), OP_COMPOSE_EDIT_NEWSGROUPS },
  { N_("Subj"), OP_COMPOSE_EDIT_SUBJECT },
  { N_("Attach file"), OP_COMPOSE_ATTACH_FILE },
  { N_("Descrip"), OP_COMPOSE_EDIT_DESCRIPTION },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
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

  HeaderPadding[idx] = mutt_str_strlen(header);
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

  for (int i = 0; i <= HDR_XCOMMENTTO; i++)
    calc_header_width_padding(i, _(Prompts[i]), true);

  /* Don't include "Sign as: " in the MaxHeaderWidth calculation.  It
   * doesn't show up by default, and so can make the indentation of
   * the other fields look funny. */
  calc_header_width_padding(HDR_CRYPTINFO, _(Prompts[HDR_CRYPTINFO]), false);

  for (int i = 0; i <= HDR_XCOMMENTTO; i++)
  {
    HeaderPadding[i] += MaxHeaderWidth;
    if (HeaderPadding[i] < 0)
      HeaderPadding[i] = 0;
  }
}

/**
 * snd_make_entry - Format a menu item for the attachment list - Implements Menu::menu_make_entry()
 */
static void snd_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  struct AttachCtx *actx = menu->data;

  mutt_expando_format(buf, buflen, 0, MuttIndexWindow->cols, NONULL(C_AttachFormat),
                      attach_format_str, (unsigned long) (actx->idx[actx->v2r[line]]),
                      MUTT_FORMAT_STAT_FILE | MUTT_FORMAT_ARROWCURSOR);
}

/**
 * redraw_crypt_lines - Update the encryption info in the compose window
 * @param msg Header of message
 */
static void redraw_crypt_lines(struct Email *msg)
{
  SET_COLOR(MT_COLOR_COMPOSE_HEADER);
  mutt_window_mvprintw(MuttIndexWindow, HDR_CRYPT, 0, "%*s",
                       HeaderPadding[HDR_CRYPT], _(Prompts[HDR_CRYPT]));
  NORMAL_COLOR;

  if ((WithCrypto & (APPLICATION_PGP | APPLICATION_SMIME)) == 0)
  {
    addstr(_("Not supported"));
    return;
  }

  if ((msg->security & (SEC_ENCRYPT | SEC_SIGN)) == (SEC_ENCRYPT | SEC_SIGN))
  {
    SET_COLOR(MT_COLOR_COMPOSE_SECURITY_BOTH);
    addstr(_("Sign, Encrypt"));
  }
  else if (msg->security & SEC_ENCRYPT)
  {
    SET_COLOR(MT_COLOR_COMPOSE_SECURITY_ENCRYPT);
    addstr(_("Encrypt"));
  }
  else if (msg->security & SEC_SIGN)
  {
    SET_COLOR(MT_COLOR_COMPOSE_SECURITY_SIGN);
    addstr(_("Sign"));
  }
  else
  {
    /* L10N: This refers to the encryption of the email, e.g. "Security: None" */
    SET_COLOR(MT_COLOR_COMPOSE_SECURITY_NONE);
    addstr(_("None"));
  }
  NORMAL_COLOR;

  if ((msg->security & (SEC_ENCRYPT | SEC_SIGN)))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) && (msg->security & APPLICATION_PGP))
    {
      if ((msg->security & SEC_INLINE))
        addstr(_(" (inline PGP)"));
      else
        addstr(_(" (PGP/MIME)"));
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) && (msg->security & APPLICATION_SMIME))
      addstr(_(" (S/MIME)"));
  }

  if (C_CryptOpportunisticEncrypt && (msg->security & SEC_OPPENCRYPT))
    addstr(_(" (OppEnc mode)"));

  mutt_window_clrtoeol(MuttIndexWindow);
  mutt_window_move(MuttIndexWindow, HDR_CRYPTINFO, 0);
  mutt_window_clrtoeol(MuttIndexWindow);

  if (((WithCrypto & APPLICATION_PGP) != 0) &&
      (msg->security & APPLICATION_PGP) && (msg->security & SEC_SIGN))
  {
    SET_COLOR(MT_COLOR_COMPOSE_HEADER);
    printw("%*s", HeaderPadding[HDR_CRYPTINFO], _(Prompts[HDR_CRYPTINFO]));
    NORMAL_COLOR;
    printw("%s", C_PgpSignAs ? C_PgpSignAs : _("<default>"));
  }

  if (((WithCrypto & APPLICATION_SMIME) != 0) &&
      (msg->security & APPLICATION_SMIME) && (msg->security & SEC_SIGN))
  {
    SET_COLOR(MT_COLOR_COMPOSE_HEADER);
    printw("%*s", HeaderPadding[HDR_CRYPTINFO], _(Prompts[HDR_CRYPTINFO]));
    NORMAL_COLOR;
    printw("%s", C_SmimeSignAs ? C_SmimeSignAs : _("<default>"));
  }

  if (((WithCrypto & APPLICATION_SMIME) != 0) && (msg->security & APPLICATION_SMIME) &&
      (msg->security & SEC_ENCRYPT) && C_SmimeEncryptWith)
  {
    SET_COLOR(MT_COLOR_COMPOSE_HEADER);
    mutt_window_mvprintw(MuttIndexWindow, HDR_CRYPTINFO, 40, "%s", _("Encrypt with: "));
    NORMAL_COLOR;
    printw("%s", NONULL(C_SmimeEncryptWith));
  }
}

#ifdef MIXMASTER
/**
 * redraw_mix_line - Redraw the Mixmaster chain
 * @param chain List of chain links
 */
static void redraw_mix_line(struct ListHead *chain)
{
  char *t = NULL;

  SET_COLOR(MT_COLOR_COMPOSE_HEADER);
  mutt_window_mvprintw(MuttIndexWindow, HDR_MIX, 0, "%*s",
                       HeaderPadding[HDR_MIX], _(Prompts[HDR_MIX]));
  NORMAL_COLOR;

  if (STAILQ_EMPTY(chain))
  {
    addstr(_("<no chain defined>"));
    mutt_window_clrtoeol(MuttIndexWindow);
    return;
  }

  int c = 12;
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, chain, entries)
  {
    t = np->data;
    if (t && (t[0] == '0') && (t[1] == '\0'))
      t = "<random>";

    if (c + mutt_str_strlen(t) + 2 >= MuttIndexWindow->cols)
      break;

    addstr(NONULL(t));
    if (STAILQ_NEXT(np, entries))
      addstr(", ");

    c += mutt_str_strlen(t) + 2;
  }
}
#endif /* MIXMASTER */

/**
 * check_attachments - Check if any attachments have changed or been deleted
 * @param actx Attachment context
 * @retval  0 Success
 * @retval -1 Error
 */
static int check_attachments(struct AttachCtx *actx)
{
  struct stat st;
  char pretty[PATH_MAX], msg[PATH_MAX + 128];

  for (int i = 0; i < actx->idxlen; i++)
  {
    if (actx->idx[i]->content->type == TYPE_MULTIPART)
      continue;
    mutt_str_strfcpy(pretty, actx->idx[i]->content->filename, sizeof(pretty));
    if (stat(actx->idx[i]->content->filename, &st) != 0)
    {
      mutt_pretty_mailbox(pretty, sizeof(pretty));
      mutt_error(_("%s [#%d] no longer exists"), pretty, i + 1);
      return -1;
    }

    if (actx->idx[i]->content->stamp < st.st_mtime)
    {
      mutt_pretty_mailbox(pretty, sizeof(pretty));
      snprintf(msg, sizeof(msg), _("%s [#%d] modified. Update encoding?"), pretty, i + 1);

      enum QuadOption ans = mutt_yesorno(msg, MUTT_YES);
      if (ans == MUTT_YES)
        mutt_update_encoding(actx->idx[i]->content);
      else if (ans == MUTT_ABORT)
        return -1;
    }
  }

  return 0;
}

/**
 * draw_envelope_addr - Write addresses to the compose window
 * @param line Line to write to (index into Prompts)
 * @param al   Address list to write
 */
static void draw_envelope_addr(int line, struct AddressList *al)
{
  char buf[1024];

  buf[0] = '\0';
  mutt_addrlist_write(buf, sizeof(buf), al, true);
  SET_COLOR(MT_COLOR_COMPOSE_HEADER);
  mutt_window_mvprintw(MuttIndexWindow, line, 0, "%*s", HeaderPadding[line],
                       _(Prompts[line]));
  NORMAL_COLOR;
  mutt_paddstr(W, buf);
}

/**
 * draw_envelope - Write the email headers to the compose window
 * @param msg Header of the message
 * @param fcc Fcc field
 */
static void draw_envelope(struct Email *msg, char *fcc)
{
  draw_envelope_addr(HDR_FROM, &msg->env->from);
#ifdef USE_NNTP
  if (!OptNewsSend)
  {
#endif
    draw_envelope_addr(HDR_TO, &msg->env->to);
    draw_envelope_addr(HDR_CC, &msg->env->cc);
    draw_envelope_addr(HDR_BCC, &msg->env->bcc);
#ifdef USE_NNTP
  }
  else
  {
    mutt_window_mvprintw(MuttIndexWindow, HDR_TO, 0, "%*s",
                         HeaderPadding[HDR_NEWSGROUPS], Prompts[HDR_NEWSGROUPS]);
    mutt_paddstr(W, NONULL(msg->env->newsgroups));
    mutt_window_mvprintw(MuttIndexWindow, HDR_CC, 0, "%*s",
                         HeaderPadding[HDR_FOLLOWUPTO], Prompts[HDR_FOLLOWUPTO]);
    mutt_paddstr(W, NONULL(msg->env->followup_to));
    if (C_XCommentTo)
    {
      mutt_window_mvprintw(MuttIndexWindow, HDR_BCC, 0, "%*s",
                           HeaderPadding[HDR_XCOMMENTTO], Prompts[HDR_XCOMMENTTO]);
      mutt_paddstr(W, NONULL(msg->env->x_comment_to));
    }
  }
#endif

  SET_COLOR(MT_COLOR_COMPOSE_HEADER);
  mutt_window_mvprintw(MuttIndexWindow, HDR_SUBJECT, 0, "%*s",
                       HeaderPadding[HDR_SUBJECT], _(Prompts[HDR_SUBJECT]));
  NORMAL_COLOR;
  mutt_paddstr(W, NONULL(msg->env->subject));

  draw_envelope_addr(HDR_REPLYTO, &msg->env->reply_to);

  SET_COLOR(MT_COLOR_COMPOSE_HEADER);
  mutt_window_mvprintw(MuttIndexWindow, HDR_FCC, 0, "%*s",
                       HeaderPadding[HDR_FCC], _(Prompts[HDR_FCC]));
  NORMAL_COLOR;
  mutt_paddstr(W, fcc);

  if (WithCrypto)
    redraw_crypt_lines(msg);

#ifdef MIXMASTER
  redraw_mix_line(&msg->chain);
#endif

  SET_COLOR(MT_COLOR_STATUS);
  mutt_window_mvaddstr(MuttIndexWindow, HDR_ATTACH - 1, 0, _("-- Attachments"));
  mutt_window_clrtoeol(MuttIndexWindow);

  NORMAL_COLOR;
}

/**
 * edit_address_list - Let the user edit the address list
 * @param[in]     line Index into the Prompts lists
 * @param[in,out] al   AddressList to edit
 */
static void edit_address_list(int line, struct AddressList *al)
{
  char buf[8192] = { 0 }; /* needs to be large for alias expansion */
  char *err = NULL;

  mutt_addrlist_to_local(al);
  mutt_addrlist_write(buf, sizeof(buf), al, false);
  if (mutt_get_field(_(Prompts[line]), buf, sizeof(buf), MUTT_ALIAS) == 0)
  {
    mutt_addrlist_free_all(al);
    mutt_addrlist_parse2(al, buf);
    mutt_expand_aliases(al);
  }

  if (mutt_addrlist_to_intl(al, &err) != 0)
  {
    mutt_error(_("Bad IDN: '%s'"), err);
    mutt_refresh();
    FREE(&err);
  }

  /* redraw the expanded list so the user can see the result */
  buf[0] = '\0';
  mutt_addrlist_write(buf, sizeof(buf), al, true);
  mutt_window_move(MuttIndexWindow, line, HDR_XOFFSET);
  mutt_paddstr(W, buf);
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
    idx[rindex]->content->tagged = false;
    return -1;
  }

  for (int y = 0; y < actx->idxlen; y++)
  {
    if (idx[y]->content->next == idx[rindex]->content)
    {
      idx[y]->content->next = idx[rindex]->content->next;
      break;
    }
  }

  idx[rindex]->content->next = NULL;
  idx[rindex]->content->parts = NULL;
  mutt_body_free(&(idx[rindex]->content));
  FREE(&idx[rindex]->tree);
  FREE(&idx[rindex]);
  for (; rindex < actx->idxlen - 1; rindex++)
    idx[rindex] = idx[rindex + 1];
  idx[actx->idxlen - 1] = NULL;
  actx->idxlen--;

  return 0;
}

/**
 * mutt_gen_compose_attach_list - Generate the attachment list for the compose screen
 * @param actx        Attachment context
 * @param m           Attachment
 * @param parent_type Attachment type, e.g #TYPE_MULTIPART
 * @param level       Nesting depth of attachment
 */
static void mutt_gen_compose_attach_list(struct AttachCtx *actx, struct Body *m,
                                         int parent_type, int level)
{
  for (; m; m = m->next)
  {
    if ((m->type == TYPE_MULTIPART) && m->parts &&
        (!(WithCrypto & APPLICATION_PGP) || !mutt_is_multipart_encrypted(m)))
    {
      mutt_gen_compose_attach_list(actx, m->parts, m->type, level);
    }
    else
    {
      struct AttachPtr *new = mutt_mem_calloc(1, sizeof(struct AttachPtr));
      mutt_actx_add_attach(actx, new);
      new->content = m;
      m->aptr = new;
      new->parent_type = parent_type;
      new->level = level;

      /* We don't support multipart messages in the compose menu yet */
    }
  }
}

/**
 * mutt_update_compose_menu - Redraw the compose window
 * @param actx Attachment context
 * @param menu Current menu
 * @param init If true, initialise the attachment list
 */
static void mutt_update_compose_menu(struct AttachCtx *actx, struct Menu *menu, bool init)
{
  if (init)
  {
    mutt_gen_compose_attach_list(actx, actx->email->content, -1, 0);
    mutt_attach_init(actx);
    menu->data = actx;
  }

  mutt_update_tree(actx);

  menu->max = actx->vcount;
  if (menu->max)
  {
    if (menu->current >= menu->max)
      menu->current = menu->max - 1;
  }
  else
    menu->current = 0;

  menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
}

/**
 * update_idx - Add a new attchment to the message
 * @param menu Current menu
 * @param actx Attachment context
 * @param new  Attachment to add
 */
static void update_idx(struct Menu *menu, struct AttachCtx *actx, struct AttachPtr *new)
{
  new->level = (actx->idxlen > 0) ? actx->idx[actx->idxlen - 1]->level : 0;
  if (actx->idxlen)
    actx->idx[actx->idxlen - 1]->content->next = new->content;
  new->content->aptr = new;
  mutt_actx_add_attach(actx, new);
  mutt_update_compose_menu(actx, menu, false);
  menu->current = actx->vcount - 1;
}

/**
 * struct ComposeRedrawData - Keep track when the compose screen needs redrawing
 */
struct ComposeRedrawData
{
  struct Email *email;
  char *fcc;
};

static void compose_status_line(char *buf, size_t buflen, size_t col, int cols,
                                struct Menu *menu, const char *p);

/**
 * compose_custom_redraw - Redraw the compose menu - Implements Menu::menu_custom_redraw()
 */
static void compose_custom_redraw(struct Menu *menu)
{
  struct ComposeRedrawData *rd = menu->redraw_data;

  if (!rd)
    return;

  if (menu->redraw & REDRAW_FULL)
  {
    menu_redraw_full(menu);

    draw_envelope(rd->email, rd->fcc);
    menu->offset = HDR_ATTACH;
    menu->pagelen = MuttIndexWindow->rows - HDR_ATTACH;
  }

  menu_check_recenter(menu);

  if (menu->redraw & REDRAW_STATUS)
  {
    char buf[1024];
    compose_status_line(buf, sizeof(buf), 0, MuttStatusWindow->cols, menu,
                        NONULL(C_ComposeFormat));
    mutt_window_move(MuttStatusWindow, 0, 0);
    SET_COLOR(MT_COLOR_STATUS);
    mutt_paddstr(MuttStatusWindow->cols, buf);
    NORMAL_COLOR;
    menu->redraw &= ~REDRAW_STATUS;
  }

#ifdef USE_SIDEBAR
  if (menu->redraw & REDRAW_SIDEBAR)
    menu_redraw_sidebar(menu);
#endif

  if (menu->redraw & REDRAW_INDEX)
    menu_redraw_index(menu);
  else if (menu->redraw & (REDRAW_MOTION | REDRAW_MOTION_RESYNC))
    menu_redraw_motion(menu);
  else if (menu->redraw == REDRAW_CURRENT)
    menu_redraw_current(menu);
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
    if (part->next == idx[first]->content)
    {
      idx[first]->content->next = idx[first + 1]->content->next;
      idx[first + 1]->content->next = idx[first]->content;
      part->next = idx[first + 1]->content;
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
 * cum_attachs_size - Cumulative Attachments Size
 * @param menu Menu listing attachments
 * @retval num Bytes in attachments
 *
 * Returns the total number of bytes used by the attachments in the attachment
 * list _after_ content-transfer-encodings have been applied.
 */
static unsigned long cum_attachs_size(struct Menu *menu)
{
  size_t s = 0;
  struct AttachCtx *actx = menu->data;
  struct AttachPtr **idx = actx->idx;
  struct Content *info = NULL;
  struct Body *b = NULL;

  for (unsigned short i = 0; i < actx->idxlen; i++)
  {
    b = idx[i]->content;

    if (!b->content)
      b->content = mutt_get_content_info(b->filename, b);

    info = b->content;
    if (info)
    {
      switch (b->encoding)
      {
        case ENC_QUOTED_PRINTABLE:
          s += 3 * (info->lobin + info->hibin) + info->ascii + info->crlf;
          break;
        case ENC_BASE64:
          s += (4 * (info->lobin + info->hibin + info->ascii + info->crlf)) / 3;
          break;
        default:
          s += info->lobin + info->hibin + info->ascii + info->crlf;
          break;
      }
    }
  }

  return s;
}

/**
 * compose_format_str - Create the status bar string for compose mode - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%a     | Total number of attachments
 * | \%h     | Local hostname
 * | \%l     | Approximate size (in bytes) of the current message
 * | \%v     | NeoMutt version string
 */
static const char *compose_format_str(char *buf, size_t buflen, size_t col, int cols,
                                      char op, const char *src, const char *prec,
                                      const char *if_str, const char *else_str,
                                      unsigned long data, MuttFormatFlags flags)
{
  char fmt[128], tmp[128];
  int optional = (flags & MUTT_FORMAT_OPTIONAL);
  struct Menu *menu = (struct Menu *) data;

  *buf = '\0';
  switch (op)
  {
    case 'a': /* total number of attachments */
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, menu->max);
      break;

    case 'h': /* hostname */
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, NONULL(ShortHostname));
      break;

    case 'l': /* approx length of current message in bytes */
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      mutt_str_pretty_size(tmp, sizeof(tmp), menu ? cum_attachs_size(menu) : 0);
      snprintf(buf, buflen, fmt, tmp);
      break;

    case 'v':
      snprintf(buf, buflen, "%s", mutt_make_version());
      break;

    case 0:
      *buf = '\0';
      return src;

    default:
      snprintf(buf, buflen, "%%%s%c", prec, op);
      break;
  }

  if (optional)
    compose_status_line(buf, buflen, col, cols, menu, if_str);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    compose_status_line(buf, buflen, col, cols, menu, else_str);

  return src;
}

/**
 * compose_status_line - Compose the string for the status bar
 * @param[out] buf    Buffer in which to save string
 * @param[in]  buflen Buffer length
 * @param[in]  col    Starting column
 * @param[in]  cols   Number of screen columns
 * @param[in]  menu   Current menu
 * @param[in]  src    Printf-like format string
 */
static void compose_status_line(char *buf, size_t buflen, size_t col, int cols,
                                struct Menu *menu, const char *src)
{
  mutt_expando_format(buf, buflen, col, cols, src, compose_format_str,
                      (unsigned long) menu, MUTT_FORMAT_NO_FLAGS);
}

/**
 * mutt_compose_menu - Allow the user to edit the message envelope
 * @param msg    Message to fill
 * @param fcc    Buffer to save FCC
 * @param fcclen Length of FCC buffer
 * @param cur    Current message
 * @param flags  Flags, e.g. #MUTT_COMPOSE_NOFREEHEADER
 * @retval  1 Message should be postponed
 * @retval  0 Normal exit
 * @retval -1 Abort message
 */
int mutt_compose_menu(struct Email *msg, char *fcc, size_t fcclen, struct Email *cur, int flags)
{
  char helpstr[1024]; // This isn't copied by the help bar
  char buf[PATH_MAX];
  int op_close = OP_NULL;
  int rc = -1;
  bool loop = true;
  bool fcc_set = false; /* has the user edited the Fcc: field ? */
  struct ComposeRedrawData rd;
#ifdef USE_NNTP
  bool news = OptNewsSend; /* is it a news article ? */
#endif

  init_header_padding();

  rd.email = msg;
  rd.fcc = fcc;

  struct Menu *menu = mutt_menu_new(MENU_COMPOSE);
  menu->offset = HDR_ATTACH;
  menu->menu_make_entry = snd_make_entry;
  menu->menu_tag = attach_tag;
#ifdef USE_NNTP
  if (news)
    menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_COMPOSE, ComposeNewsHelp);
  else
#endif
    menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_COMPOSE, ComposeHelp);
  menu->menu_custom_redraw = compose_custom_redraw;
  menu->redraw_data = &rd;
  mutt_menu_push_current(menu);

  struct AttachCtx *actx = mutt_mem_calloc(sizeof(struct AttachCtx), 1);
  actx->email = msg;
  mutt_update_compose_menu(actx, menu, true);

  while (loop)
  {
#ifdef USE_NNTP
    OptNews = false; /* for any case */
#endif
    const int op = mutt_menu_loop(menu);
    switch (op)
    {
      case OP_COMPOSE_EDIT_FROM:
        edit_address_list(HDR_FROM, &msg->env->from);
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_EDIT_TO:
#ifdef USE_NNTP
        if (news)
          break;
#endif
        edit_address_list(HDR_TO, &msg->env->to);
        if (C_CryptOpportunisticEncrypt)
        {
          crypt_opportunistic_encrypt(msg);
          redraw_crypt_lines(msg);
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_EDIT_BCC:
#ifdef USE_NNTP
        if (news)
          break;
#endif
        edit_address_list(HDR_BCC, &msg->env->bcc);
        if (C_CryptOpportunisticEncrypt)
        {
          crypt_opportunistic_encrypt(msg);
          redraw_crypt_lines(msg);
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_EDIT_CC:
#ifdef USE_NNTP
        if (news)
          break;
#endif
        edit_address_list(HDR_CC, &msg->env->cc);
        if (C_CryptOpportunisticEncrypt)
        {
          crypt_opportunistic_encrypt(msg);
          redraw_crypt_lines(msg);
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;
#ifdef USE_NNTP
      case OP_COMPOSE_EDIT_NEWSGROUPS:
        if (!news)
          break;
        if (msg->env->newsgroups)
          mutt_str_strfcpy(buf, msg->env->newsgroups, sizeof(buf));
        else
          buf[0] = '\0';
        if (mutt_get_field("Newsgroups: ", buf, sizeof(buf), 0) == 0)
        {
          mutt_str_replace(&msg->env->newsgroups, buf);
          mutt_window_move(MuttIndexWindow, HDR_TO, HDR_XOFFSET);
          if (msg->env->newsgroups)
            mutt_paddstr(W, msg->env->newsgroups);
          else
            clrtoeol();
        }
        break;

      case OP_COMPOSE_EDIT_FOLLOWUP_TO:
        if (!news)
          break;
        if (msg->env->followup_to)
          mutt_str_strfcpy(buf, msg->env->followup_to, sizeof(buf));
        else
          buf[0] = '\0';
        if (mutt_get_field("Followup-To: ", buf, sizeof(buf), 0) == 0)
        {
          mutt_str_replace(&msg->env->followup_to, buf);
          mutt_window_move(MuttIndexWindow, HDR_CC, HDR_XOFFSET);
          if (msg->env->followup_to)
            mutt_paddstr(W, msg->env->followup_to);
          else
            clrtoeol();
        }
        break;

      case OP_COMPOSE_EDIT_X_COMMENT_TO:
        if (!(news && C_XCommentTo))
          break;
        if (msg->env->x_comment_to)
          mutt_str_strfcpy(buf, msg->env->x_comment_to, sizeof(buf));
        else
          buf[0] = '\0';
        if (mutt_get_field("X-Comment-To: ", buf, sizeof(buf), 0) == 0)
        {
          mutt_str_replace(&msg->env->x_comment_to, buf);
          mutt_window_move(MuttIndexWindow, HDR_BCC, HDR_XOFFSET);
          if (msg->env->x_comment_to)
            mutt_paddstr(W, msg->env->x_comment_to);
          else
            clrtoeol();
        }
        break;
#endif

      case OP_COMPOSE_EDIT_SUBJECT:
        if (msg->env->subject)
          mutt_str_strfcpy(buf, msg->env->subject, sizeof(buf));
        else
          buf[0] = '\0';
        if (mutt_get_field(_("Subject: "), buf, sizeof(buf), 0) == 0)
        {
          mutt_str_replace(&msg->env->subject, buf);
          mutt_window_move(MuttIndexWindow, HDR_SUBJECT, HDR_XOFFSET);
          if (msg->env->subject)
            mutt_paddstr(W, msg->env->subject);
          else
            mutt_window_clrtoeol(MuttIndexWindow);
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_EDIT_REPLY_TO:
        edit_address_list(HDR_REPLYTO, &msg->env->reply_to);
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_EDIT_FCC:
        mutt_str_strfcpy(buf, fcc, sizeof(buf));
        if (mutt_get_field(_("Fcc: "), buf, sizeof(buf), MUTT_FILE | MUTT_CLEAR) == 0)
        {
          mutt_str_strfcpy(fcc, buf, fcclen);
          mutt_pretty_mailbox(fcc, fcclen);
          mutt_window_move(MuttIndexWindow, HDR_FCC, HDR_XOFFSET);
          mutt_paddstr(W, fcc);
          fcc_set = true;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_EDIT_MESSAGE:
        if (C_Editor && (mutt_str_strcmp("builtin", C_Editor) != 0) && !C_EditHeaders)
        {
          mutt_edit_file(C_Editor, msg->content->filename);
          mutt_update_encoding(msg->content);
          menu->redraw = REDRAW_FULL;
          mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
          break;
        }
        /* fallthrough */

      case OP_COMPOSE_EDIT_HEADERS:
        if ((mutt_str_strcmp("builtin", C_Editor) != 0) &&
            ((op == OP_COMPOSE_EDIT_HEADERS) || ((op == OP_COMPOSE_EDIT_MESSAGE) && C_EditHeaders)))
        {
          const char *tag = NULL;
          char *err = NULL;
          mutt_env_to_local(msg->env);
          mutt_edit_headers(NONULL(C_Editor), msg->content->filename, msg, fcc, fcclen);
          if (mutt_env_to_intl(msg->env, &tag, &err))
          {
            mutt_error(_("Bad IDN in '%s': '%s'"), tag, err);
            FREE(&err);
          }
          if (C_CryptOpportunisticEncrypt)
            crypt_opportunistic_encrypt(msg);
        }
        else
        {
          /* this is grouped with OP_COMPOSE_EDIT_HEADERS because the
           * attachment list could change if the user invokes ~v to edit
           * the message with headers, in which we need to execute the
           * code below to regenerate the index array */
          mutt_builtin_editor(msg->content->filename, msg, cur);
        }
        mutt_update_encoding(msg->content);

        /* attachments may have been added */
        if (actx->idxlen && actx->idx[actx->idxlen - 1]->content->next)
        {
          mutt_actx_free_entries(actx);
          mutt_update_compose_menu(actx, menu, true);
        }

        menu->redraw = REDRAW_FULL;
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_ATTACH_KEY:
      {
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        struct AttachPtr *new = mutt_mem_calloc(1, sizeof(struct AttachPtr));
        new->content = crypt_pgp_make_key_attachment();
        if (new->content)
        {
          update_idx(menu, actx, new);
          menu->redraw |= REDRAW_INDEX;
        }
        else
          FREE(&new);

        menu->redraw |= REDRAW_STATUS;

        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_MOVE_UP:
        if (menu->current == 0)
        {
          mutt_error(_("Attachment is already at top"));
          break;
        }
        if (menu->current == 1)
        {
          mutt_error(_("The fundamental part can't be moved"));
          break;
        }
        compose_attach_swap(msg->content, actx->idx, menu->current - 1);
        menu->redraw = REDRAW_INDEX;
        menu->current--;
        break;

      case OP_COMPOSE_MOVE_DOWN:
        if (menu->current == (actx->idxlen - 1))
        {
          mutt_error(_("Attachment is already at bottom"));
          break;
        }
        if (menu->current == 0)
        {
          mutt_error(_("The fundamental part can't be moved"));
          break;
        }
        compose_attach_swap(msg->content, actx->idx, menu->current);
        menu->redraw = REDRAW_INDEX;
        menu->current++;
        break;

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
        group->subtype = mutt_str_strdup("alternative");
        group->disposition = DISP_INLINE;

        struct Body *alts = NULL;
        /* group tagged message into a multipart/alternative */
        struct Body *bptr = msg->content;
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

            /* append bptr to the alts list,
             * and remove from the msg->content list */
            if (!alts)
            {
              group->parts = bptr;
              alts = bptr;
              bptr = bptr->next;
              alts->next = NULL;
            }
            else
            {
              alts->next = bptr;
              bptr = bptr->next;
              alts = alts->next;
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
          group->description = mutt_str_strdup("unknown alternative group");

        struct AttachPtr *gptr = mutt_mem_calloc(1, sizeof(struct AttachPtr));
        gptr->content = group;
        update_idx(menu, actx, gptr);
        menu->redraw = REDRAW_INDEX;
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
        for (struct Body *b = msg->content; b; b = b->next)
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
        group->subtype = mutt_str_strdup("multilingual");
        group->disposition = DISP_INLINE;

        struct Body *alts = NULL;
        /* group tagged message into a multipart/multilingual */
        struct Body *bptr = msg->content;
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

            /* append bptr to the alts list,
             * and remove from the msg->content list */
            if (!alts)
            {
              group->parts = bptr;
              alts = bptr;
              bptr = bptr->next;
              alts->next = NULL;
            }
            else
            {
              alts->next = bptr;
              bptr = bptr->next;
              alts = alts->next;
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
          group->description = mutt_str_strdup("unknown multilingual group");

        struct AttachPtr *gptr = mutt_mem_calloc(1, sizeof(struct AttachPtr));
        gptr->content = group;
        update_idx(menu, actx, gptr);
        menu->redraw = REDRAW_INDEX;
        break;
      }

      case OP_COMPOSE_ATTACH_FILE:
      {
        char *prompt = _("Attach file");
        int numfiles = 0;
        char **files = NULL;
        buf[0] = '\0';

        if ((mutt_enter_fname_full(prompt, buf, sizeof(buf), false, true,
                                   &files, &numfiles, MUTT_SEL_MULTI) == -1) ||
            (*buf == '\0'))
        {
          break;
        }

        bool error = false;
        if (numfiles > 1)
        {
          mutt_message(ngettext("Attaching selected file...",
                                "Attaching selected files...", numfiles));
        }
        for (int i = 0; i < numfiles; i++)
        {
          char *att = files[i];
          struct AttachPtr *new = mutt_mem_calloc(1, sizeof(struct AttachPtr));
          new->unowned = true;
          new->content = mutt_make_file_attach(att);
          if (new->content)
            update_idx(menu, actx, new);
          else
          {
            error = true;
            mutt_error(_("Unable to attach %s"), att);
            FREE(&new);
          }
          FREE(&files[i]);
        }

        FREE(&files);
        if (!error)
          mutt_clear_error();

        menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_ATTACH_MESSAGE:
#ifdef USE_NNTP
      case OP_COMPOSE_ATTACH_NEWS_MESSAGE:
#endif
      {
        char *prompt = _("Open mailbox to attach message from");
        buf[0] = '\0';

#ifdef USE_NNTP
        OptNews = false;
        if (Context && (op == OP_COMPOSE_ATTACH_NEWS_MESSAGE))
        {
          CurrentNewsSrv = nntp_select_server(Context->mailbox, C_NewsServer, false);
          if (!CurrentNewsSrv)
            break;

          prompt = _("Open newsgroup to attach message from");
          OptNews = true;
        }
#endif

        if (Context)
#ifdef USE_NNTP
          if ((op == OP_COMPOSE_ATTACH_MESSAGE) ^ (Context->mailbox->magic == MUTT_NNTP))
#endif
          {
            mutt_str_strfcpy(buf, mutt_b2s(Context->mailbox->pathbuf), sizeof(buf));
            mutt_pretty_mailbox(buf, sizeof(buf));
          }

        if ((mutt_enter_fname(prompt, buf, sizeof(buf), true) == -1) || (buf[0] == '\0'))
          break;

#ifdef USE_NNTP
        if (OptNews)
          nntp_expand_path(buf, sizeof(buf), &CurrentNewsSrv->conn->account);
        else
#endif
          mutt_expand_path(buf, sizeof(buf));
#ifdef USE_IMAP
        if (imap_path_probe(buf, NULL) != MUTT_IMAP)
#endif
#ifdef USE_POP
          if (pop_path_probe(buf, NULL) != MUTT_POP)
#endif
#ifdef USE_NNTP
            if (!OptNews && (nntp_path_probe(buf, NULL) != MUTT_NNTP))
#endif
              /* check to make sure the file exists and is readable */
              if (access(buf, R_OK) == -1)
              {
                mutt_perror(buf);
                break;
              }

        menu->redraw = REDRAW_FULL;

        struct Mailbox *m = mx_path_resolve(buf);
        struct Context *ctx = mx_mbox_open(m, MUTT_READONLY);
        if (!ctx)
        {
          mutt_error(_("Unable to open mailbox %s"), buf);
          mailbox_free(&m);
          break;
        }

        if (ctx->mailbox->msg_count == 0)
        {
          mx_mbox_close(&ctx);
          mutt_error(_("No messages in that folder"));
          break;
        }

        struct Context *this = Context; /* remember current folder and sort methods */
        int old_sort = C_Sort; /* C_Sort, SortAux could be changed in mutt_index_menu() */
        int old_sort_aux = C_SortAux;

        Context = ctx;
        OptAttachMsg = true;
        mutt_message(_("Tag the messages you want to attach"));
        op_close = mutt_index_menu();
        OptAttachMsg = false;

        if (!Context)
        {
          /* go back to the folder we started from */
          Context = this;
          /* Restore old $sort and $sort_aux */
          C_Sort = old_sort;
          C_SortAux = old_sort_aux;
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
          break;
        }

        for (int i = 0; i < Context->mailbox->msg_count; i++)
        {
          if (!message_is_tagged(Context, i))
            continue;

          struct AttachPtr *new = mutt_mem_calloc(1, sizeof(struct AttachPtr));
          new->content = mutt_make_message_attach(Context->mailbox,
                                                  Context->mailbox->emails[i], true);
          if (new->content)
            update_idx(menu, actx, new);
          else
          {
            mutt_error(_("Unable to attach"));
            FREE(&new);
          }
        }
        menu->redraw |= REDRAW_FULL;

        if (op_close == OP_QUIT)
          mx_mbox_close(&Context);
        else
        {
          mx_fastclose_mailbox(Context->mailbox);
          ctx_free(&Context);
        }

        /* go back to the folder we started from */
        Context = this;
        /* Restore old $sort and $sort_aux */
        C_Sort = old_sort;
        C_SortAux = old_sort_aux;
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;
      }

      case OP_DELETE:
        CHECK_COUNT;
        if (CUR_ATTACH->unowned)
          CUR_ATTACH->content->unlink = false;
        if (delete_attachment(actx, menu->current) == -1)
          break;
        mutt_update_compose_menu(actx, menu, false);
        if (menu->current == 0)
          msg->content = actx->idx[0]->content;

        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_TOGGLE_RECODE:
      {
        CHECK_COUNT;
        if (!mutt_is_text_part(CUR_ATTACH->content))
        {
          mutt_error(_("Recoding only affects text attachments"));
          break;
        }
        CUR_ATTACH->content->noconv = !CUR_ATTACH->content->noconv;
        if (CUR_ATTACH->content->noconv)
          mutt_message(_("The current attachment won't be converted"));
        else
          mutt_message(_("The current attachment will be converted"));
        menu->redraw = REDRAW_CURRENT;
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_EDIT_DESCRIPTION:
        CHECK_COUNT;
        mutt_str_strfcpy(
            buf, CUR_ATTACH->content->description ? CUR_ATTACH->content->description : "",
            sizeof(buf));
        /* header names should not be translated */
        if (mutt_get_field("Description: ", buf, sizeof(buf), 0) == 0)
        {
          mutt_str_replace(&CUR_ATTACH->content->description, buf);
          menu->redraw = REDRAW_CURRENT;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_UPDATE_ENCODING:
        CHECK_COUNT;
        if (menu->tagprefix)
        {
          struct Body *top = NULL;
          for (top = msg->content; top; top = top->next)
          {
            if (top->tagged)
              mutt_update_encoding(top);
          }
          menu->redraw = REDRAW_FULL;
        }
        else
        {
          mutt_update_encoding(CUR_ATTACH->content);
          menu->redraw = REDRAW_CURRENT | REDRAW_STATUS;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_TOGGLE_DISPOSITION:
        /* toggle the content-disposition between inline/attachment */
        CUR_ATTACH->content->disposition =
            (CUR_ATTACH->content->disposition == DISP_INLINE) ? DISP_ATTACH : DISP_INLINE;
        menu->redraw = REDRAW_CURRENT;
        break;

      case OP_EDIT_TYPE:
        CHECK_COUNT;
        {
          mutt_edit_content_type(NULL, CUR_ATTACH->content, NULL);

          /* this may have been a change to text/something */
          mutt_update_encoding(CUR_ATTACH->content);

          menu->redraw = REDRAW_CURRENT;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_EDIT_LANGUAGE:
        CHECK_COUNT;
        buf[0] = '\0'; /* clear buffer first */
        if (CUR_ATTACH->content->language)
          mutt_str_strfcpy(buf, CUR_ATTACH->content->language, sizeof(buf));
        if (mutt_get_field("Content-Language: ", buf, sizeof(buf), 0) == 0)
        {
          CUR_ATTACH->content->language = mutt_str_strdup(buf);
          menu->redraw = REDRAW_CURRENT | REDRAW_STATUS;
          mutt_clear_error();
        }
        else
          mutt_warning(_("Empty 'Content-Language'"));
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_EDIT_ENCODING:
        CHECK_COUNT;
        mutt_str_strfcpy(buf, ENCODING(CUR_ATTACH->content->encoding), sizeof(buf));
        if ((mutt_get_field("Content-Transfer-Encoding: ", buf, sizeof(buf), 0) == 0) &&
            (buf[0] != '\0'))
        {
          int enc = mutt_check_encoding(buf);
          if ((enc != ENC_OTHER) && (enc != ENC_UUENCODED))
          {
            CUR_ATTACH->content->encoding = enc;
            menu->redraw = REDRAW_CURRENT | REDRAW_STATUS;
            mutt_clear_error();
          }
          else
            mutt_error(_("Invalid encoding"));
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_SEND_MESSAGE:
        /* Note: We don't invoke send2-hook here, since we want to leave
         * users an opportunity to change settings from the ":" prompt.  */
        if (check_attachments(actx) != 0)
        {
          menu->redraw = REDRAW_FULL;
          break;
        }

#ifdef MIXMASTER
        if (!STAILQ_EMPTY(&msg->chain) && (mix_check_message(msg) != 0))
          break;
#endif

        if (!fcc_set && *fcc)
        {
          enum QuadOption ans =
              query_quadoption(C_Copy, _("Save a copy of this message?"));
          if (ans == MUTT_ABORT)
            break;
          else if (ans == MUTT_NO)
            *fcc = '\0';
        }

        loop = false;
        rc = 0;
        break;

      case OP_COMPOSE_EDIT_FILE:
        CHECK_COUNT;
        mutt_edit_file(NONULL(C_Editor), CUR_ATTACH->content->filename);
        mutt_update_encoding(CUR_ATTACH->content);
        menu->redraw = REDRAW_CURRENT | REDRAW_STATUS;
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_TOGGLE_UNLINK:
        CHECK_COUNT;
        CUR_ATTACH->content->unlink = !CUR_ATTACH->content->unlink;

        menu->redraw = REDRAW_INDEX;
        /* No send2hook since this doesn't change the message. */
        break;

      case OP_COMPOSE_GET_ATTACHMENT:
        CHECK_COUNT;
        if (menu->tagprefix)
        {
          for (struct Body *top = msg->content; top; top = top->next)
          {
            if (top->tagged)
              mutt_get_tmp_attachment(top);
          }
          menu->redraw = REDRAW_FULL;
        }
        else if (mutt_get_tmp_attachment(CUR_ATTACH->content) == 0)
          menu->redraw = REDRAW_CURRENT;

        /* No send2hook since this doesn't change the message. */
        break;

      case OP_COMPOSE_RENAME_ATTACHMENT:
      {
        CHECK_COUNT;
        char *src = NULL;
        if (CUR_ATTACH->content->d_filename)
          src = CUR_ATTACH->content->d_filename;
        else
          src = CUR_ATTACH->content->filename;
        mutt_str_strfcpy(buf, mutt_path_basename(NONULL(src)), sizeof(buf));
        int ret = mutt_get_field(_("Send attachment with name: "), buf, sizeof(buf), MUTT_FILE);
        if (ret == 0)
        {
          /* As opposed to RENAME_FILE, we don't check buf[0] because it's
           * valid to set an empty string here, to erase what was set */
          mutt_str_replace(&CUR_ATTACH->content->d_filename, buf);
          menu->redraw = REDRAW_CURRENT;
        }
        break;
      }

      case OP_COMPOSE_RENAME_FILE:
        CHECK_COUNT;
        mutt_str_strfcpy(buf, CUR_ATTACH->content->filename, sizeof(buf));
        mutt_pretty_mailbox(buf, sizeof(buf));
        if ((mutt_get_field(_("Rename to: "), buf, sizeof(buf), MUTT_FILE) == 0) &&
            (buf[0] != '\0'))
        {
          struct stat st;
          if (stat(CUR_ATTACH->content->filename, &st) == -1)
          {
            /* L10N: "stat" is a system call. Do "man 2 stat" for more information. */
            mutt_error(_("Can't stat %s: %s"), buf, strerror(errno));
            break;
          }

          mutt_expand_path(buf, sizeof(buf));
          if (mutt_file_rename(CUR_ATTACH->content->filename, buf))
            break;

          mutt_str_replace(&CUR_ATTACH->content->filename, buf);
          menu->redraw = REDRAW_CURRENT;

          if (CUR_ATTACH->content->stamp >= st.st_mtime)
            mutt_stamp_attachment(CUR_ATTACH->content);
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_COMPOSE_NEW_MIME:
      {
        mutt_window_clearline(MuttMessageWindow, 0);
        buf[0] = '\0';
        if ((mutt_get_field(_("New file: "), buf, sizeof(buf), MUTT_FILE) != 0) ||
            (buf[0] == '\0'))
        {
          continue;
        }
        mutt_expand_path(buf, sizeof(buf));

        /* Call to lookup_mime_type () ?  maybe later */
        char type[256] = { 0 };
        if ((mutt_get_field("Content-Type: ", type, sizeof(type), 0) != 0) ||
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
        int itype = mutt_check_mime_type(type);
        if (itype == TYPE_OTHER)
        {
          mutt_error(_("Unknown Content-Type %s"), type);
          continue;
        }
        struct AttachPtr *new = mutt_mem_calloc(1, sizeof(struct AttachPtr));
        /* Touch the file */
        FILE *fp = mutt_file_fopen(buf, "w");
        if (!fp)
        {
          mutt_error(_("Can't create file %s"), buf);
          FREE(&new);
          continue;
        }
        mutt_file_fclose(&fp);

        new->content = mutt_make_file_attach(buf);
        if (!new->content)
        {
          mutt_error(_("What we have here is a failure to make an attachment"));
          FREE(&new);
          continue;
        }
        update_idx(menu, actx, new);

        CUR_ATTACH->content->type = itype;
        mutt_str_replace(&CUR_ATTACH->content->subtype, p);
        CUR_ATTACH->content->unlink = true;
        menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;

        if (mutt_compose_attachment(CUR_ATTACH->content))
        {
          mutt_update_encoding(CUR_ATTACH->content);
          menu->redraw = REDRAW_FULL;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;
      }

      case OP_COMPOSE_EDIT_MIME:
        CHECK_COUNT;
        if (mutt_edit_attachment(CUR_ATTACH->content))
        {
          mutt_update_encoding(CUR_ATTACH->content);
          menu->redraw = REDRAW_FULL;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_VIEW_ATTACH:
      case OP_DISPLAY_HEADERS:
        CHECK_COUNT;
        mutt_attach_display_loop(menu, op, NULL, actx, false);
        menu->redraw = REDRAW_FULL;
        /* no send2hook, since this doesn't modify the message */
        break;

      case OP_SAVE:
        CHECK_COUNT;
        mutt_save_attachment_list(actx, NULL, menu->tagprefix,
                                  CUR_ATTACH->content, NULL, menu);
        /* no send2hook, since this doesn't modify the message */
        break;

      case OP_PRINT:
        CHECK_COUNT;
        mutt_print_attachment_list(actx, NULL, menu->tagprefix, CUR_ATTACH->content);
        /* no send2hook, since this doesn't modify the message */
        break;

      case OP_PIPE:
      case OP_FILTER:
        CHECK_COUNT;
        mutt_pipe_attachment_list(actx, NULL, menu->tagprefix,
                                  CUR_ATTACH->content, (op == OP_FILTER));
        if (op == OP_FILTER) /* cte might have changed */
          menu->redraw = menu->tagprefix ? REDRAW_FULL : REDRAW_CURRENT;
        menu->redraw |= REDRAW_STATUS;
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_EXIT:
      {
        enum QuadOption ans =
            query_quadoption(C_Postpone, _("Save (postpone) draft message?"));
        if (ans == MUTT_NO)
        {
          for (int i = 0; i < actx->idxlen; i++)
            if (actx->idx[i]->unowned)
              actx->idx[i]->content->unlink = false;

          if (!(flags & MUTT_COMPOSE_NOFREEHEADER))
          {
            for (int i = 0; i < actx->idxlen; i++)
            {
              /* avoid freeing other attachments */
              actx->idx[i]->content->next = NULL;
              actx->idx[i]->content->parts = NULL;
              mutt_body_free(&actx->idx[i]->content);
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
        if (check_attachments(actx) != 0)
        {
          menu->redraw = REDRAW_FULL;
          break;
        }

        loop = false;
        rc = 1;
        break;

      case OP_COMPOSE_ISPELL:
        endwin();
        snprintf(buf, sizeof(buf), "%s -x %s", NONULL(C_Ispell), msg->content->filename);
        if (mutt_system(buf) == -1)
          mutt_error(_("Error running \"%s\""), buf);
        else
        {
          mutt_update_encoding(msg->content);
          menu->redraw |= REDRAW_STATUS;
        }
        break;

      case OP_COMPOSE_WRITE_MESSAGE:
        buf[0] = '\0';
        if (Context)
        {
          mutt_str_strfcpy(buf, mutt_b2s(Context->mailbox->pathbuf), sizeof(buf));
          mutt_pretty_mailbox(buf, sizeof(buf));
        }
        if (actx->idxlen)
          msg->content = actx->idx[0]->content;
        if ((mutt_enter_fname(_("Write message to mailbox"), buf, sizeof(buf), true) != -1) &&
            (buf[0] != '\0'))
        {
          mutt_message(_("Writing message to %s ..."), buf);
          mutt_expand_path(buf, sizeof(buf));

          if (msg->content->next)
            msg->content = mutt_make_multipart(msg->content);

          if (mutt_write_fcc(buf, msg, NULL, false, NULL, NULL) < 0)
            msg->content = mutt_remove_multipart(msg->content);
          else
            mutt_message(_("Message written"));
        }
        break;

      case OP_COMPOSE_PGP_MENU:
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        if (!crypt_has_module_backend(APPLICATION_PGP))
        {
          mutt_error(_("No PGP backend configured"));
          break;
        }
        if (((WithCrypto & APPLICATION_SMIME) != 0) && (msg->security & APPLICATION_SMIME))
        {
          if (msg->security & (SEC_ENCRYPT | SEC_SIGN))
          {
            if (mutt_yesorno(_("S/MIME already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
            {
              mutt_clear_error();
              break;
            }
            msg->security &= ~(SEC_ENCRYPT | SEC_SIGN);
          }
          msg->security &= ~APPLICATION_SMIME;
          msg->security |= APPLICATION_PGP;
          crypt_opportunistic_encrypt(msg);
          redraw_crypt_lines(msg);
        }
        msg->security = crypt_pgp_send_menu(msg);
        redraw_crypt_lines(msg);
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

      case OP_FORGET_PASSPHRASE:
        crypt_forget_passphrase();
        break;

      case OP_COMPOSE_SMIME_MENU:
        if (!(WithCrypto & APPLICATION_SMIME))
          break;
        if (!crypt_has_module_backend(APPLICATION_SMIME))
        {
          mutt_error(_("No S/MIME backend configured"));
          break;
        }

        if (((WithCrypto & APPLICATION_PGP) != 0) && (msg->security & APPLICATION_PGP))
        {
          if (msg->security & (SEC_ENCRYPT | SEC_SIGN))
          {
            if (mutt_yesorno(_("PGP already selected. Clear and continue?"), MUTT_YES) != MUTT_YES)
            {
              mutt_clear_error();
              break;
            }
            msg->security &= ~(SEC_ENCRYPT | SEC_SIGN);
          }
          msg->security &= ~APPLICATION_PGP;
          msg->security |= APPLICATION_SMIME;
          crypt_opportunistic_encrypt(msg);
          redraw_crypt_lines(msg);
        }
        msg->security = crypt_smime_send_menu(msg);
        redraw_crypt_lines(msg);
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;

#ifdef MIXMASTER
      case OP_COMPOSE_MIX:
        mix_make_chain(&msg->chain);
        mutt_message_hook(NULL, msg, MUTT_SEND2_HOOK);
        break;
#endif
    }
  }

  mutt_menu_pop_current(menu);
  mutt_menu_destroy(&menu);

  if (actx->idxlen)
    msg->content = actx->idx[0]->content;
  else
    msg->content = NULL;

  mutt_actx_free(&actx);

  return rc;
}
