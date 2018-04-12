/**
 * @file
 * GUI editor for an email's headers
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2007,2010,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
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

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "mutt.h"
#include "alias.h"
#include "attach.h"
#include "body.h"
#include "content.h"
#include "context.h"
#include "envelope.h"
#include "format_flags.h"
#include "globals.h"
#include "header.h"
#include "keymap.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "sort.h"
#ifdef MIXMASTER
#include "remailer.h"
#endif
#ifdef USE_NNTP
#include "nntp.h"
#endif

static const char *There_are_no_attachments = N_("There are no attachments.");

#define CHECK_COUNT                                                            \
  if (actx->idxlen == 0)                                                       \
  {                                                                            \
    mutt_error(_(There_are_no_attachments));                                   \
    break;                                                                     \
  }

#define CURATTACH actx->idx[actx->v2r[menu->current]]

/**
 * enum HeaderField - Ordered list of headers for the compose screen
 */
enum HeaderField
{
  HDR_FROM = 0,
  HDR_TO,
  HDR_CC,
  HDR_BCC,
  HDR_SUBJECT,
  HDR_REPLYTO,
  HDR_FCC,

#ifdef MIXMASTER
  HDR_MIX,
#endif

  HDR_CRYPT,
  HDR_CRYPTINFO,

#ifdef USE_NNTP
  HDR_NEWSGROUPS,
  HDR_FOLLOWUPTO,
  HDR_XCOMMENTTO,
#endif

  HDR_ATTACH = (HDR_FCC + 5) /* where to start printing the attachments */
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
   * This string is used by the compose menu.
   * Since it is hidden by default, it does not increase the
   * indentation of other compose menu fields.  However, if possible,
   * it should not be longer than the other compose menu fields.
   *
   * Since it shares the row with "Encrypt with:", it should not be longer
   * than 15-20 character cells.
   */
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

static void calc_header_width_padding(int idx, const char *header, int calc_max)
{
  int width;

  HeaderPadding[idx] = mutt_str_strlen(header);
  width = mutt_strwidth(header);
  if (calc_max && MaxHeaderWidth < width)
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
  static short done = 0;

  if (done)
    return;
  done = 1;

  for (int i = 0; i <= HDR_XCOMMENTTO; i++)
    calc_header_width_padding(i, _(Prompts[i]), 1);

  /* Don't include "Sign as: " in the MaxHeaderWidth calculation.  It
   * doesn't show up by default, and so can make the indentation of
   * the other fields look funny. */
  calc_header_width_padding(HDR_CRYPTINFO, _(Prompts[HDR_CRYPTINFO]), 0);

  for (int i = 0; i <= HDR_XCOMMENTTO; i++)
  {
    HeaderPadding[i] += MaxHeaderWidth;
    if (HeaderPadding[i] < 0)
      HeaderPadding[i] = 0;
  }
}

/**
 * snd_entry - Format a menu item for the attachment list
 * @param[out] buf    Buffer in which to save string
 * @param[in]  buflen Buffer length
 * @param[in]  menu   Menu containing aliases
 * @param[in]  num    Index into the menu
 */
static void snd_entry(char *buf, size_t buflen, struct Menu *menu, int num)
{
  struct AttachCtx *actx = (struct AttachCtx *) menu->data;

  mutt_expando_format(buf, buflen, 0, MuttIndexWindow->cols, NONULL(AttachFormat),
                      attach_format_str, (unsigned long) (actx->idx[actx->v2r[num]]),
                      MUTT_FORMAT_STAT_FILE | MUTT_FORMAT_ARROWCURSOR);
}

static void redraw_crypt_lines(struct Header *msg)
{
  SETCOLOR(MT_COLOR_COMPOSE_HEADER);
  mutt_window_mvprintw(MuttIndexWindow, HDR_CRYPT, 0, "%*s",
                       HeaderPadding[HDR_CRYPT], _(Prompts[HDR_CRYPT]));
  NORMAL_COLOR;

  if ((WithCrypto & (APPLICATION_PGP | APPLICATION_SMIME)) == 0)
  {
    addstr(_("Not supported"));
    return;
  }

  if ((msg->security & (ENCRYPT | SIGN)) == (ENCRYPT | SIGN))
  {
    SETCOLOR(MT_COLOR_COMPOSE_SECURITY_BOTH);
    addstr(_("Sign, Encrypt"));
  }
  else if (msg->security & ENCRYPT)
  {
    SETCOLOR(MT_COLOR_COMPOSE_SECURITY_ENCRYPT);
    addstr(_("Encrypt"));
  }
  else if (msg->security & SIGN)
  {
    SETCOLOR(MT_COLOR_COMPOSE_SECURITY_SIGN);
    addstr(_("Sign"));
  }
  else
  {
    /* L10N: This refers to the encryption of the email, e.g. "Security: None" */
    SETCOLOR(MT_COLOR_COMPOSE_SECURITY_NONE);
    addstr(_("None"));
  }
  NORMAL_COLOR;

  if ((msg->security & (ENCRYPT | SIGN)))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) && (msg->security & APPLICATION_PGP))
    {
      if ((msg->security & INLINE))
        addstr(_(" (inline PGP)"));
      else
        addstr(_(" (PGP/MIME)"));
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) && (msg->security & APPLICATION_SMIME))
      addstr(_(" (S/MIME)"));
  }

  if (CryptOpportunisticEncrypt && (msg->security & OPPENCRYPT))
    addstr(_(" (OppEnc mode)"));

  mutt_window_clrtoeol(MuttIndexWindow);
  mutt_window_move(MuttIndexWindow, HDR_CRYPTINFO, 0);
  mutt_window_clrtoeol(MuttIndexWindow);

  if (((WithCrypto & APPLICATION_PGP) != 0) &&
      (msg->security & APPLICATION_PGP) && (msg->security & SIGN))
  {
    SETCOLOR(MT_COLOR_COMPOSE_HEADER);
    printw("%*s", HeaderPadding[HDR_CRYPTINFO], _(Prompts[HDR_CRYPTINFO]));
    NORMAL_COLOR;
    printw("%s", PgpSignAs ? PgpSignAs : _("<default>"));
  }

  if (((WithCrypto & APPLICATION_SMIME) != 0) &&
      (msg->security & APPLICATION_SMIME) && (msg->security & SIGN))
  {
    SETCOLOR(MT_COLOR_COMPOSE_HEADER);
    printw("%*s", HeaderPadding[HDR_CRYPTINFO], _(Prompts[HDR_CRYPTINFO]));
    NORMAL_COLOR;
    printw("%s", SmimeSignAs ? SmimeSignAs : _("<default>"));
  }

  if (((WithCrypto & APPLICATION_SMIME) != 0) && (msg->security & APPLICATION_SMIME) &&
      (msg->security & ENCRYPT) && SmimeEncryptWith && *SmimeEncryptWith)
  {
    SETCOLOR(MT_COLOR_COMPOSE_HEADER);
    mutt_window_mvprintw(MuttIndexWindow, HDR_CRYPTINFO, 40, "%s", _("Encrypt with: "));
    NORMAL_COLOR;
    printw("%s", NONULL(SmimeEncryptWith));
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

  SETCOLOR(MT_COLOR_COMPOSE_HEADER);
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
  struct ListNode *np;
  STAILQ_FOREACH(np, chain, entries)
  {
    t = np->data;
    if (t && t[0] == '0' && t[1] == '\0')
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

static int check_attachments(struct AttachCtx *actx)
{
  int r;
  struct stat st;
  char pretty[_POSIX_PATH_MAX], msg[_POSIX_PATH_MAX + SHORT_STRING];

  for (int i = 0; i < actx->idxlen; i++)
  {
    mutt_str_strfcpy(pretty, actx->idx[i]->content->filename, sizeof(pretty));
    if (stat(actx->idx[i]->content->filename, &st) != 0)
    {
      mutt_pretty_mailbox(pretty, sizeof(pretty));
      mutt_error(_("%s [#%d] no longer exists!"), pretty, i + 1);
      return -1;
    }

    if (actx->idx[i]->content->stamp < st.st_mtime)
    {
      mutt_pretty_mailbox(pretty, sizeof(pretty));
      snprintf(msg, sizeof(msg), _("%s [#%d] modified. Update encoding?"), pretty, i + 1);

      r = mutt_yesorno(msg, MUTT_YES);
      if (r == MUTT_YES)
        mutt_update_encoding(actx->idx[i]->content);
      else if (r == MUTT_ABORT)
        return -1;
    }
  }

  return 0;
}

static void draw_envelope_addr(int line, struct Address *addr)
{
  char buf[LONG_STRING];

  buf[0] = 0;
  mutt_addr_write(buf, sizeof(buf), addr, true);
  SETCOLOR(MT_COLOR_COMPOSE_HEADER);
  mutt_window_mvprintw(MuttIndexWindow, line, 0, "%*s", HeaderPadding[line],
                       _(Prompts[line]));
  NORMAL_COLOR;
  mutt_paddstr(W, buf);
}

static void draw_envelope(struct Header *msg, char *fcc)
{
  draw_envelope_addr(HDR_FROM, msg->env->from);
#ifdef USE_NNTP
  if (!OptNewsSend)
  {
#endif
    draw_envelope_addr(HDR_TO, msg->env->to);
    draw_envelope_addr(HDR_CC, msg->env->cc);
    draw_envelope_addr(HDR_BCC, msg->env->bcc);
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
    if (XCommentTo)
    {
      mutt_window_mvprintw(MuttIndexWindow, HDR_BCC, 0, "%*s",
                           HeaderPadding[HDR_XCOMMENTTO], Prompts[HDR_XCOMMENTTO]);
      mutt_paddstr(W, NONULL(msg->env->x_comment_to));
    }
  }
#endif

  SETCOLOR(MT_COLOR_COMPOSE_HEADER);
  mutt_window_mvprintw(MuttIndexWindow, HDR_SUBJECT, 0, "%*s",
                       HeaderPadding[HDR_SUBJECT], _(Prompts[HDR_SUBJECT]));
  NORMAL_COLOR;
  mutt_paddstr(W, NONULL(msg->env->subject));

  draw_envelope_addr(HDR_REPLYTO, msg->env->reply_to);

  SETCOLOR(MT_COLOR_COMPOSE_HEADER);
  mutt_window_mvprintw(MuttIndexWindow, HDR_FCC, 0, "%*s",
                       HeaderPadding[HDR_FCC], _(Prompts[HDR_FCC]));
  NORMAL_COLOR;
  mutt_paddstr(W, fcc);

  if (WithCrypto)
    redraw_crypt_lines(msg);

#ifdef MIXMASTER
  redraw_mix_line(&msg->chain);
#endif

  SETCOLOR(MT_COLOR_STATUS);
  mutt_window_mvaddstr(MuttIndexWindow, HDR_ATTACH - 1, 0, _("-- Attachments"));
  mutt_window_clrtoeol(MuttIndexWindow);

  NORMAL_COLOR;
}

static void edit_address_list(int line, struct Address **addr)
{
  char buf[HUGE_STRING] = ""; /* needs to be large for alias expansion */
  char *err = NULL;

  mutt_addrlist_to_local(*addr);
  mutt_addr_write(buf, sizeof(buf), *addr, false);
  if (mutt_get_field(_(Prompts[line]), buf, sizeof(buf), MUTT_ALIAS) == 0)
  {
    mutt_addr_free(addr);
    *addr = mutt_addr_parse_list2(*addr, buf);
    *addr = mutt_expand_aliases(*addr);
  }

  if (mutt_addrlist_to_intl(*addr, &err) != 0)
  {
    mutt_error(_("Warning: '%s' is a bad IDN."), err);
    mutt_refresh();
    FREE(&err);
  }

  /* redraw the expanded list so the user can see the result */
  buf[0] = 0;
  mutt_addr_write(buf, sizeof(buf), *addr, true);
  mutt_window_move(MuttIndexWindow, line, HDR_XOFFSET);
  mutt_paddstr(W, buf);
}

static int delete_attachment(struct AttachCtx *actx, int x)
{
  struct AttachPtr **idx = actx->idx;
  int rindex = actx->v2r[x];

  if (rindex == 0 && actx->idxlen == 1)
  {
    mutt_error(_("You may not delete the only attachment."));
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

static void mutt_gen_compose_attach_list(struct AttachCtx *actx, struct Body *m,
                                         int parent_type, int level)
{
  struct AttachPtr *new = NULL;

  for (; m; m = m->next)
  {
    if (m->type == TYPEMULTIPART && m->parts &&
        (!(WithCrypto & APPLICATION_PGP) || !mutt_is_multipart_encrypted(m)))
    {
      mutt_gen_compose_attach_list(actx, m->parts, m->type, level);
    }
    else
    {
      new = (struct AttachPtr *) mutt_mem_calloc(1, sizeof(struct AttachPtr));
      mutt_actx_add_attach(actx, new);
      new->content = m;
      m->aptr = new;
      new->parent_type = parent_type;
      new->level = level;

      /* We don't support multipart messages in the compose menu yet */
    }
  }
}

static void mutt_update_compose_menu(struct AttachCtx *actx, struct Menu *menu, int init)
{
  if (init)
  {
    mutt_gen_compose_attach_list(actx, actx->hdr->content, -1, 0);
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

static void update_idx(struct Menu *menu, struct AttachCtx *actx, struct AttachPtr *new)
{
  new->level = (actx->idxlen > 0) ? actx->idx[actx->idxlen - 1]->level : 0;
  if (actx->idxlen)
    actx->idx[actx->idxlen - 1]->content->next = new->content;
  new->content->aptr = new;
  mutt_actx_add_attach(actx, new);
  mutt_update_compose_menu(actx, menu, 0);
  menu->current = actx->vcount - 1;
}

/**
 * struct ComposeRedrawData - Keep track when the compose screen needs redrawing
 */
struct ComposeRedrawData
{
  struct Header *msg;
  char *fcc;
};

/* prototype for use below */
static void compose_status_line(char *buf, size_t buflen, size_t col, int cols,
                                struct Menu *menu, const char *p);

static void compose_menu_redraw(struct Menu *menu)
{
  struct ComposeRedrawData *rd = menu->redraw_data;

  if (!rd)
    return;

  if (menu->redraw & REDRAW_FULL)
  {
    menu_redraw_full(menu);

    draw_envelope(rd->msg, rd->fcc);
    menu->offset = HDR_ATTACH;
    menu->pagelen = MuttIndexWindow->rows - HDR_ATTACH;
  }

  menu_check_recenter(menu);

  if (menu->redraw & REDRAW_STATUS)
  {
    char buf[LONG_STRING];
    compose_status_line(buf, sizeof(buf), 0, MuttStatusWindow->cols, menu,
                        NONULL(ComposeFormat));
    mutt_window_move(MuttStatusWindow, 0, 0);
    SETCOLOR(MT_COLOR_STATUS);
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
  else if (menu->redraw & (REDRAW_MOTION | REDRAW_MOTION_RESYNCH))
    menu_redraw_motion(menu);
  else if (menu->redraw == REDRAW_CURRENT)
    menu_redraw_current(menu);
}

/**
 * cum_attachs_size - Cumulative Attachments Size
 * @param menu Menu listing attachments
 * @retval n Number of bytes in attachments
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
        case ENCQUOTEDPRINTABLE:
          s += 3 * (info->lobin + info->hibin) + info->ascii + info->crlf;
          break;
        case ENCBASE64:
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
 * compose_format_str - Create the status bar string for compose mode
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  col      Starting column
 * @param[in]  cols     Number of screen columns
 * @param[in]  op       printf-like operator, e.g. 't'
 * @param[in]  src      printf-like format string
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  if_str   If condition is met, display this string
 * @param[in]  else_str Otherwise, display this string
 * @param[in]  data     Pointer to the mailbox Context
 * @param[in]  flags    Format flags
 * @retval src (unchanged)
 *
 * compose_format_str() is a callback function for mutt_expando_format().
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
                                      unsigned long data, enum FormatFlag flags)
{
  char fmt[SHORT_STRING], tmp[SHORT_STRING];
  int optional = (flags & MUTT_FORMAT_OPTIONAL);
  struct Menu *menu = (struct Menu *) data;

  *buf = 0;
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
      snprintf(buf, buflen, "NeoMutt %s%s", PACKAGE_VERSION, GitVer);
      break;

    case 0:
      *buf = 0;
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

static void compose_status_line(char *buf, size_t buflen, size_t col, int cols,
                                struct Menu *menu, const char *p)
{
  mutt_expando_format(buf, buflen, col, cols, p, compose_format_str,
                      (unsigned long) menu, 0);
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
int mutt_compose_menu(struct Header *msg, char *fcc, size_t fcclen,
                      struct Header *cur, int flags)
{
  char helpstr[LONG_STRING];
  char buf[LONG_STRING];
  char fname[_POSIX_PATH_MAX];
  struct AttachPtr *new = NULL;
  int i, close = 0;
  int r = -1; /* return value */
  int loop = 1;
  int fcc_set = 0; /* has the user edited the Fcc: field ? */
  struct Context *ctx = NULL, *this = NULL;
  /* Sort, SortAux could be changed in mutt_index_menu() */
  int old_sort, old_sort_aux;
  struct stat st;
  struct ComposeRedrawData rd;
#ifdef USE_NNTP
  int news = 0; /* is it a news article ? */

  if (OptNewsSend)
    news++;
#endif

  init_header_padding();

  rd.msg = msg;
  rd.fcc = fcc;

  struct Menu *menu = mutt_menu_new(MENU_COMPOSE);
  menu->offset = HDR_ATTACH;
  menu->make_entry = snd_entry;
  menu->tag = mutt_tag_attach;
#ifdef USE_NNTP
  if (news)
    menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_COMPOSE, ComposeNewsHelp);
  else
#endif
    menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_COMPOSE, ComposeHelp);
  menu->custom_menu_redraw = compose_menu_redraw;
  menu->redraw_data = &rd;
  mutt_menu_push_current(menu);

  struct AttachCtx *actx = mutt_mem_calloc(sizeof(struct AttachCtx), 1);
  actx->hdr = msg;
  mutt_update_compose_menu(actx, menu, 1);

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
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;
      case OP_COMPOSE_EDIT_TO:
#ifdef USE_NNTP
        if (news)
          break;
#endif
        edit_address_list(HDR_TO, &msg->env->to);
        if (CryptOpportunisticEncrypt)
        {
          crypt_opportunistic_encrypt(msg);
          redraw_crypt_lines(msg);
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;
      case OP_COMPOSE_EDIT_BCC:
#ifdef USE_NNTP
        if (news)
          break;
#endif
        edit_address_list(HDR_BCC, &msg->env->bcc);
        if (CryptOpportunisticEncrypt)
        {
          crypt_opportunistic_encrypt(msg);
          redraw_crypt_lines(msg);
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;
      case OP_COMPOSE_EDIT_CC:
#ifdef USE_NNTP
        if (news)
          break;
#endif
        edit_address_list(HDR_CC, &msg->env->cc);
        if (CryptOpportunisticEncrypt)
        {
          crypt_opportunistic_encrypt(msg);
          redraw_crypt_lines(msg);
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;
#ifdef USE_NNTP
      case OP_COMPOSE_EDIT_NEWSGROUPS:
        if (news)
        {
          if (msg->env->newsgroups)
            mutt_str_strfcpy(buf, msg->env->newsgroups, sizeof(buf));
          else
            buf[0] = 0;
          if (mutt_get_field("Newsgroups: ", buf, sizeof(buf), 0) == 0)
          {
            mutt_str_replace(&msg->env->newsgroups, buf);
            mutt_window_move(MuttIndexWindow, HDR_TO, HDR_XOFFSET);
            if (msg->env->newsgroups)
              mutt_paddstr(W, msg->env->newsgroups);
            else
              clrtoeol();
          }
        }
        break;
      case OP_COMPOSE_EDIT_FOLLOWUP_TO:
        if (news)
        {
          if (msg->env->followup_to)
            mutt_str_strfcpy(buf, msg->env->followup_to, sizeof(buf));
          else
            buf[0] = 0;
          if (mutt_get_field("Followup-To: ", buf, sizeof(buf), 0) == 0)
          {
            mutt_str_replace(&msg->env->followup_to, buf);
            mutt_window_move(MuttIndexWindow, HDR_CC, HDR_XOFFSET);
            if (msg->env->followup_to)
              mutt_paddstr(W, msg->env->followup_to);
            else
              clrtoeol();
          }
        }
        break;
      case OP_COMPOSE_EDIT_X_COMMENT_TO:
        if (news && XCommentTo)
        {
          if (msg->env->x_comment_to)
            mutt_str_strfcpy(buf, msg->env->x_comment_to, sizeof(buf));
          else
            buf[0] = 0;
          if (mutt_get_field("X-Comment-To: ", buf, sizeof(buf), 0) == 0)
          {
            mutt_str_replace(&msg->env->x_comment_to, buf);
            mutt_window_move(MuttIndexWindow, HDR_BCC, HDR_XOFFSET);
            if (msg->env->x_comment_to)
              mutt_paddstr(W, msg->env->x_comment_to);
            else
              clrtoeol();
          }
        }
        break;
#endif
      case OP_COMPOSE_EDIT_SUBJECT:
        if (msg->env->subject)
          mutt_str_strfcpy(buf, msg->env->subject, sizeof(buf));
        else
          buf[0] = 0;
        if (mutt_get_field(_("Subject: "), buf, sizeof(buf), 0) == 0)
        {
          mutt_str_replace(&msg->env->subject, buf);
          mutt_window_move(MuttIndexWindow, HDR_SUBJECT, HDR_XOFFSET);
          if (msg->env->subject)
            mutt_paddstr(W, msg->env->subject);
          else
            mutt_window_clrtoeol(MuttIndexWindow);
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;
      case OP_COMPOSE_EDIT_REPLY_TO:
        edit_address_list(HDR_REPLYTO, &msg->env->reply_to);
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;
      case OP_COMPOSE_EDIT_FCC:
        mutt_str_strfcpy(buf, fcc, sizeof(buf));
        if (mutt_get_field(_("Fcc: "), buf, sizeof(buf), MUTT_FILE | MUTT_CLEAR) == 0)
        {
          mutt_str_strfcpy(fcc, buf, fcclen);
          mutt_pretty_mailbox(fcc, fcclen);
          mutt_window_move(MuttIndexWindow, HDR_FCC, HDR_XOFFSET);
          mutt_paddstr(W, fcc);
          fcc_set = 1;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;
      case OP_COMPOSE_EDIT_MESSAGE:
        if (Editor && (mutt_str_strcmp("builtin", Editor) != 0) && !EditHeaders)
        {
          mutt_edit_file(Editor, msg->content->filename);
          mutt_update_encoding(msg->content);
          menu->redraw = REDRAW_FULL;
          mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
          break;
        }
      /* fallthrough */
      case OP_COMPOSE_EDIT_HEADERS:
        if ((mutt_str_strcmp("builtin", Editor) != 0) &&
            (op == OP_COMPOSE_EDIT_HEADERS || (op == OP_COMPOSE_EDIT_MESSAGE && EditHeaders)))
        {
          char *tag = NULL, *err = NULL;
          mutt_env_to_local(msg->env);
          mutt_edit_headers(NONULL(Editor), msg->content->filename, msg, fcc, fcclen);
          if (mutt_env_to_intl(msg->env, &tag, &err))
          {
            mutt_error(_("Bad IDN in \"%s\": '%s'"), tag, err);
            FREE(&err);
          }
          if (CryptOpportunisticEncrypt)
            crypt_opportunistic_encrypt(msg);
        }
        else
        {
          /* this is grouped with OP_COMPOSE_EDIT_HEADERS because the
             attachment list could change if the user invokes ~v to edit
             the message with headers, in which we need to execute the
             code below to regenerate the index array */
          mutt_builtin_editor(msg->content->filename, msg, cur);
        }
        mutt_update_encoding(msg->content);

        /* attachments may have been added */
        if (actx->idxlen && actx->idx[actx->idxlen - 1]->content->next)
        {
          mutt_actx_free_entries(actx);
          mutt_update_compose_menu(actx, menu, 1);
        }

        menu->redraw = REDRAW_FULL;
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_ATTACH_KEY:
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        new = mutt_mem_calloc(1, sizeof(struct AttachPtr));
        new->content = crypt_pgp_make_key_attachment(NULL);
        if (new->content)
        {
          update_idx(menu, actx, new);
          menu->redraw |= REDRAW_INDEX;
        }
        else
          FREE(&new);

        menu->redraw |= REDRAW_STATUS;

        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_ATTACH_FILE:
      {
        char *prompt = _("Attach file");
        int numfiles = 0;
        char **files = NULL;
        fname[0] = 0;

        if (mutt_enter_fname_full(prompt, fname, sizeof(fname), 0, 1, &files,
                                  &numfiles, MUTT_SEL_MULTI) == -1 ||
            *fname == '\0')
          break;

        int error = 0;
        if (numfiles > 1)
          mutt_message(_("Attaching selected files..."));
        for (i = 0; i < numfiles; i++)
        {
          char *att = files[i];
          new = (struct AttachPtr *) mutt_mem_calloc(1, sizeof(struct AttachPtr));
          new->unowned = 1;
          new->content = mutt_make_file_attach(att);
          if (new->content)
            update_idx(menu, actx, new);
          else
          {
            error = 1;
            mutt_error(_("Unable to attach %s!"), att);
            FREE(&new);
          }
          FREE(&files[i]);
        }

        FREE(&files);
        if (!error)
          mutt_clear_error();

        menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
      }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_ATTACH_MESSAGE:
#ifdef USE_NNTP
      case OP_COMPOSE_ATTACH_NEWS_MESSAGE:
#endif
      {
        char *prompt = _("Open mailbox to attach message from");
        fname[0] = 0;

#ifdef USE_NNTP
        OptNews = false;
        if (op == OP_COMPOSE_ATTACH_NEWS_MESSAGE)
        {
          CurrentNewsSrv = nntp_select_server(NewsServer, false);
          if (!CurrentNewsSrv)
            break;

          prompt = _("Open newsgroup to attach message from");
          OptNews = true;
        }
#endif

        if (Context)
#ifdef USE_NNTP
          if ((op == OP_COMPOSE_ATTACH_MESSAGE) ^ (Context->magic == MUTT_NNTP))
#endif
          {
            mutt_str_strfcpy(fname, NONULL(Context->path), sizeof(fname));
            mutt_pretty_mailbox(fname, sizeof(fname));
          }

        if (mutt_enter_fname(prompt, fname, sizeof(fname), 1) == -1 || !fname[0])
          break;

#ifdef USE_NNTP
        if (OptNews)
          nntp_expand_path(fname, sizeof(fname), &CurrentNewsSrv->conn->account);
        else
#endif
          mutt_expand_path(fname, sizeof(fname));
#ifdef USE_IMAP
        if (!mx_is_imap(fname))
#endif
#ifdef USE_POP
          if (!mx_is_pop(fname))
#endif
#ifdef USE_NNTP
            if (!mx_is_nntp(fname) && !OptNews)
#endif
              /* check to make sure the file exists and is readable */
              if (access(fname, R_OK) == -1)
              {
                mutt_perror(fname);
                break;
              }

        menu->redraw = REDRAW_FULL;

        ctx = mx_open_mailbox(fname, MUTT_READONLY, NULL);
        if (!ctx)
        {
          mutt_error(_("Unable to open mailbox %s"), fname);
          break;
        }

        if (!ctx->msgcount)
        {
          mx_close_mailbox(ctx, NULL);
          FREE(&ctx);
          mutt_error(_("No messages in that folder."));
          break;
        }

        this = Context; /* remember current folder and sort methods */
        old_sort = Sort;
        old_sort_aux = SortAux;

        Context = ctx;
        OptAttachMsg = true;
        mutt_message(_("Tag the messages you want to attach!"));
        close = mutt_index_menu();
        OptAttachMsg = false;

        if (!Context)
        {
          /* go back to the folder we started from */
          Context = this;
          /* Restore old $sort and $sort_aux */
          Sort = old_sort;
          SortAux = old_sort_aux;
          menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
          break;
        }

        for (i = 0; i < Context->msgcount; i++)
        {
          if (!message_is_tagged(Context, i))
            continue;

          new = (struct AttachPtr *) mutt_mem_calloc(1, sizeof(struct AttachPtr));
          new->content = mutt_make_message_attach(Context, Context->hdrs[i], 1);
          if (new->content != NULL)
            update_idx(menu, actx, new);
          else
          {
            mutt_error(_("Unable to attach!"));
            FREE(&new);
          }
        }
        menu->redraw |= REDRAW_FULL;

        if (close == OP_QUIT)
          mx_close_mailbox(Context, NULL);
        else
          mx_fastclose_mailbox(Context);
        FREE(&Context);

        /* go back to the folder we started from */
        Context = this;
        /* Restore old $sort and $sort_aux */
        Sort = old_sort;
        SortAux = old_sort_aux;
      }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_DELETE:
        CHECK_COUNT;
        if (CURATTACH->unowned)
          CURATTACH->content->unlink = 0;
        if (delete_attachment(actx, menu->current) == -1)
          break;
        mutt_update_compose_menu(actx, menu, 0);
        if (menu->current == 0)
          msg->content = actx->idx[0]->content;

        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_TOGGLE_RECODE:
      {
        CHECK_COUNT;
        if (!mutt_is_text_part(CURATTACH->content))
        {
          mutt_error(_("Recoding only affects text attachments."));
          break;
        }
        CURATTACH->content->noconv = !CURATTACH->content->noconv;
        if (CURATTACH->content->noconv)
          mutt_message(_("The current attachment won't be converted."));
        else
          mutt_message(_("The current attachment will be converted."));
        menu->redraw = REDRAW_CURRENT;
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;
      }

      case OP_COMPOSE_EDIT_DESCRIPTION:
        CHECK_COUNT;
        mutt_str_strfcpy(buf,
                         CURATTACH->content->description ? CURATTACH->content->description : "",
                         sizeof(buf));
        /* header names should not be translated */
        if (mutt_get_field("Description: ", buf, sizeof(buf), 0) == 0)
        {
          mutt_str_replace(&CURATTACH->content->description, buf);
          menu->redraw = REDRAW_CURRENT;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
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
          mutt_update_encoding(CURATTACH->content);
          menu->redraw = REDRAW_CURRENT | REDRAW_STATUS;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_TOGGLE_DISPOSITION:
        /* toggle the content-disposition between inline/attachment */
        CURATTACH->content->disposition =
            (CURATTACH->content->disposition == DISPINLINE) ? DISPATTACH : DISPINLINE;
        menu->redraw = REDRAW_CURRENT;
        break;

      case OP_EDIT_TYPE:
        CHECK_COUNT;
        {
          mutt_edit_content_type(NULL, CURATTACH->content, NULL);

          /* this may have been a change to text/something */
          mutt_update_encoding(CURATTACH->content);

          menu->redraw = REDRAW_CURRENT;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_EDIT_ENCODING:
        CHECK_COUNT;
        mutt_str_strfcpy(buf, ENCODING(CURATTACH->content->encoding), sizeof(buf));
        if (mutt_get_field("Content-Transfer-Encoding: ", buf, sizeof(buf), 0) == 0 && buf[0])
        {
          i = mutt_check_encoding(buf);
          if ((i != ENCOTHER) && (i != ENCUUENCODED))
          {
            CURATTACH->content->encoding = i;
            menu->redraw = REDRAW_CURRENT | REDRAW_STATUS;
            mutt_clear_error();
          }
          else
            mutt_error(_("Invalid encoding."));
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_SEND_MESSAGE:

        /* Note: We don't invoke send2-hook here, since we want to leave
         * users an opportunity to change settings from the ":" prompt.
         */

        if (check_attachments(actx) != 0)
        {
          menu->redraw = REDRAW_FULL;
          break;
        }

#ifdef MIXMASTER
        if (!STAILQ_EMPTY(&msg->chain) && mix_check_message(msg) != 0)
          break;
#endif

        if (!fcc_set && *fcc)
        {
          i = query_quadoption(Copy, _("Save a copy of this message?"));
          if (i == MUTT_ABORT)
            break;
          else if (i == MUTT_NO)
            *fcc = 0;
        }

        loop = 0;
        r = 0;
        break;

      case OP_COMPOSE_EDIT_FILE:
        CHECK_COUNT;
        mutt_edit_file(NONULL(Editor), CURATTACH->content->filename);
        mutt_update_encoding(CURATTACH->content);
        menu->redraw = REDRAW_CURRENT | REDRAW_STATUS;
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_TOGGLE_UNLINK:
        CHECK_COUNT;
        CURATTACH->content->unlink = !CURATTACH->content->unlink;

        menu->redraw = REDRAW_INDEX;
        /* No send2hook since this doesn't change the message. */
        break;

      case OP_COMPOSE_GET_ATTACHMENT:
        CHECK_COUNT;
        if (menu->tagprefix)
        {
          struct Body *top = NULL;
          for (top = msg->content; top; top = top->next)
          {
            if (top->tagged)
              mutt_get_tmp_attachment(top);
          }
          menu->redraw = REDRAW_FULL;
        }
        else if (mutt_get_tmp_attachment(CURATTACH->content) == 0)
          menu->redraw = REDRAW_CURRENT;

        /* No send2hook since this doesn't change the message. */
        break;

      case OP_COMPOSE_RENAME_ATTACHMENT:
      {
        char *src = NULL;
        int ret;

        CHECK_COUNT;
        if (CURATTACH->content->d_filename)
          src = CURATTACH->content->d_filename;
        else
          src = CURATTACH->content->filename;
        mutt_str_strfcpy(fname, mutt_file_basename(NONULL(src)), sizeof(fname));
        ret = mutt_get_field(_("Send attachment with name: "), fname, sizeof(fname), MUTT_FILE);
        if (ret == 0)
        {
          /*
             * As opposed to RENAME_FILE, we don't check fname[0] because it's
             * valid to set an empty string here, to erase what was set
             */
          mutt_str_replace(&CURATTACH->content->d_filename, fname);
          menu->redraw = REDRAW_CURRENT;
        }
      }
      break;

      case OP_COMPOSE_RENAME_FILE:
        CHECK_COUNT;
        mutt_str_strfcpy(fname, CURATTACH->content->filename, sizeof(fname));
        mutt_pretty_mailbox(fname, sizeof(fname));
        if (mutt_get_field(_("Rename to: "), fname, sizeof(fname), MUTT_FILE) == 0 &&
            fname[0])
        {
          if (stat(CURATTACH->content->filename, &st) == -1)
          {
            /* L10N:
               "stat" is a system call. Do "man 2 stat" for more information. */
            mutt_error(_("Can't stat %s: %s"), fname, strerror(errno));
            break;
          }

          mutt_expand_path(fname, sizeof(fname));
          if (mutt_file_rename(CURATTACH->content->filename, fname))
            break;

          mutt_str_replace(&CURATTACH->content->filename, fname);
          menu->redraw = REDRAW_CURRENT;

          if (CURATTACH->content->stamp >= st.st_mtime)
            mutt_stamp_attachment(CURATTACH->content);
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_NEW_MIME:
      {
        char type[STRING];
        char *p = NULL;
        int itype;
        FILE *fp = NULL;

        mutt_window_clearline(MuttMessageWindow, 0);
        fname[0] = 0;
        if (mutt_get_field(_("New file: "), fname, sizeof(fname), MUTT_FILE) != 0 ||
            !fname[0])
        {
          continue;
        }
        mutt_expand_path(fname, sizeof(fname));

        /* Call to lookup_mime_type () ?  maybe later */
        type[0] = 0;
        if (mutt_get_field("Content-Type: ", type, sizeof(type), 0) != 0 || !type[0])
          continue;

        p = strchr(type, '/');
        if (!p)
        {
          mutt_error(_("Content-Type is of the form base/sub"));
          continue;
        }
        *p++ = 0;
        itype = mutt_check_mime_type(type);
        if (itype == TYPEOTHER)
        {
          mutt_error(_("Unknown Content-Type %s"), type);
          continue;
        }
        new = (struct AttachPtr *) mutt_mem_calloc(1, sizeof(struct AttachPtr));
        /* Touch the file */
        fp = mutt_file_fopen(fname, "w");
        if (!fp)
        {
          mutt_error(_("Can't create file %s"), fname);
          FREE(&new);
          continue;
        }
        mutt_file_fclose(&fp);

        new->content = mutt_make_file_attach(fname);
        if (!new->content)
        {
          mutt_error(_("What we have here is a failure to make an attachment"));
          FREE(&new);
          continue;
        }
        update_idx(menu, actx, new);

        CURATTACH->content->type = itype;
        mutt_str_replace(&CURATTACH->content->subtype, p);
        CURATTACH->content->unlink = true;
        menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;

        if (mutt_compose_attachment(CURATTACH->content))
        {
          mutt_update_encoding(CURATTACH->content);
          menu->redraw = REDRAW_FULL;
        }
      }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_EDIT_MIME:
        CHECK_COUNT;
        if (mutt_edit_attachment(CURATTACH->content))
        {
          mutt_update_encoding(CURATTACH->content);
          menu->redraw = REDRAW_FULL;
        }
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
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
                                  CURATTACH->content, NULL, menu);
        /* no send2hook, since this doesn't modify the message */
        break;

      case OP_PRINT:
        CHECK_COUNT;
        mutt_print_attachment_list(actx, NULL, menu->tagprefix, CURATTACH->content);
        /* no send2hook, since this doesn't modify the message */
        break;

      case OP_PIPE:
      case OP_FILTER:
        CHECK_COUNT;
        mutt_pipe_attachment_list(actx, NULL, menu->tagprefix,
                                  CURATTACH->content, op == OP_FILTER);
        if (op == OP_FILTER) /* cte might have changed */
          menu->redraw = menu->tagprefix ? REDRAW_FULL : REDRAW_CURRENT;
        menu->redraw |= REDRAW_STATUS;
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_EXIT:
        i = query_quadoption(Postpone, _("Postpone this message?"));
        if (i == MUTT_NO)
        {
          for (i = 0; i < actx->idxlen; i++)
            if (actx->idx[i]->unowned)
              actx->idx[i]->content->unlink = false;

          if (!(flags & MUTT_COMPOSE_NOFREEHEADER))
          {
            for (i = 0; i < actx->idxlen; i++)
            {
              /* avoid freeing other attachments */
              actx->idx[i]->content->next = NULL;
              actx->idx[i]->content->parts = NULL;
              mutt_body_free(&actx->idx[i]->content);
            }
          }
          r = -1;
          loop = 0;
          break;
        }
        else if (i == MUTT_ABORT)
          break; /* abort */
        /* fallthrough */

      case OP_COMPOSE_POSTPONE_MESSAGE:

        if (check_attachments(actx) != 0)
        {
          menu->redraw = REDRAW_FULL;
          break;
        }

        loop = 0;
        r = 1;
        break;

      case OP_COMPOSE_ISPELL:
        endwin();
        snprintf(buf, sizeof(buf), "%s -x %s", NONULL(Ispell), msg->content->filename);
        if (mutt_system(buf) == -1)
          mutt_error(_("Error running \"%s\"!"), buf);
        else
        {
          mutt_update_encoding(msg->content);
          menu->redraw |= REDRAW_STATUS;
        }
        break;

      case OP_COMPOSE_WRITE_MESSAGE:

        fname[0] = '\0';
        if (Context)
        {
          mutt_str_strfcpy(fname, NONULL(Context->path), sizeof(fname));
          mutt_pretty_mailbox(fname, sizeof(fname));
        }
        if (actx->idxlen)
          msg->content = actx->idx[0]->content;
        if (mutt_enter_fname(_("Write message to mailbox"), fname, sizeof(fname), 1) != -1 &&
            fname[0])
        {
          mutt_message(_("Writing message to %s ..."), fname);
          mutt_expand_path(fname, sizeof(fname));

          if (msg->content->next)
            msg->content = mutt_make_multipart(msg->content);

          if (mutt_write_fcc(fname, msg, NULL, 0, NULL, NULL) < 0)
            msg->content = mutt_remove_multipart(msg->content);
          else
            mutt_message(_("Message written."));
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
          if (msg->security & (ENCRYPT | SIGN))
          {
            if (mutt_yesorno(
                    _("S/MIME already selected. Clear and continue ? "), MUTT_YES) != MUTT_YES)
            {
              mutt_clear_error();
              break;
            }
            msg->security &= ~(ENCRYPT | SIGN);
          }
          msg->security &= ~APPLICATION_SMIME;
          msg->security |= APPLICATION_PGP;
          crypt_opportunistic_encrypt(msg);
          redraw_crypt_lines(msg);
        }
        msg->security = crypt_pgp_send_menu(msg);
        redraw_crypt_lines(msg);
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
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
          if (msg->security & (ENCRYPT | SIGN))
          {
            if (mutt_yesorno(_("PGP already selected. Clear and continue ? "), MUTT_YES) != MUTT_YES)
            {
              mutt_clear_error();
              break;
            }
            msg->security &= ~(ENCRYPT | SIGN);
          }
          msg->security &= ~APPLICATION_PGP;
          msg->security |= APPLICATION_SMIME;
          crypt_opportunistic_encrypt(msg);
          redraw_crypt_lines(msg);
        }
        msg->security = crypt_smime_send_menu(msg);
        redraw_crypt_lines(msg);
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
        break;

#ifdef MIXMASTER
      case OP_COMPOSE_MIX:

        mix_make_chain(&msg->chain);
        mutt_message_hook(NULL, msg, MUTT_SEND2HOOK);
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

  mutt_free_attach_context(&actx);

  return r;
}
