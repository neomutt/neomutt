/**
 * @file
 * GUI display a user-configurable status line
 *
 * @authors
 * Copyright (C) 1996-2000,2007 Michael R. Elkins <me@mutt.org>
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
 * @page status GUI display a user-configurable status line
 *
 * GUI display a user-configurable status line
 */

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "status.h"
#include "context.h"
#include "format_flags.h"
#include "mutt_globals.h"
#include "mutt_mailbox.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "options.h"
#include "protos.h"
#include "sort.h"

/* These Config Variables are only used in status.c */
struct MbTable *C_StatusChars; ///< Config: Indicator characters for the status bar

/**
 * get_sort_str - Get the sort method as a string
 * @param buf    Buffer for the sort string
 * @param buflen Length of the buffer
 * @param method Sort method, see #SortType
 * @retval ptr Buffer pointer
 */
static char *get_sort_str(char *buf, size_t buflen, enum SortType method)
{
  snprintf(buf, buflen, "%s%s%s", (method & SORT_REVERSE) ? "reverse-" : "",
           (method & SORT_LAST) ? "last-" : "",
           mutt_map_get_name(method & SORT_MASK, SortMethods));
  return buf;
}

/**
 * status_format_str - Create the status bar string - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%b     | Number of incoming folders with unread messages
 * | \%D     | Description of the mailbox
 * | \%d     | Number of deleted messages
 * | \%f     | Full mailbox path
 * | \%F     | Number of flagged messages
 * | \%h     | Hostname
 * | \%l     | Length of mailbox (in bytes)
 * | \%L     | Size (in bytes) of the messages shown (or limited)
 * | \%M     | Number of messages shown (virtual message count when limiting)
 * | \%m     | Total number of messages
 * | \%n     | Number of new messages
 * | \%o     | Number of old unread messages
 * | \%p     | Number of postponed messages
 * | \%P     | Percent of way through index
 * | \%R     | Number of read messages
 * | \%r     | Readonly/wontwrite/changed flag
 * | \%S     | Current aux sorting method (`$sort_aux`)
 * | \%s     | Current sorting method (`$sort`)
 * | \%t     | Number of tagged messages
 * | \%u     | Number of unread messages
 * | \%V     | Currently active limit pattern
 * | \%v     | NeoMutt version
 */
static const char *status_format_str(char *buf, size_t buflen, size_t col, int cols,
                                     char op, const char *src, const char *prec,
                                     const char *if_str, const char *else_str,
                                     intptr_t data, MuttFormatFlags flags)
{
  char fmt[128], tmp[128];
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);
  struct Menu *menu = (struct Menu *) data;

  *buf = '\0';
  switch (op)
  {
    case 'b':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt,
                 mutt_mailbox_check(Context ? Context->mailbox : NULL, 0));
      }
      else if (mutt_mailbox_check(Context ? Context->mailbox : NULL, 0) == 0)
        optional = false;
      break;

    case 'd':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt,
                 (Context && Context->mailbox) ? Context->mailbox->msg_deleted : 0);
      }
      else if (!Context || !Context->mailbox || (Context->mailbox->msg_deleted == 0))
        optional = false;
      break;

    case 'D':
    {
      struct Mailbox *m = Context ? Context->mailbox : NULL;
      // If there's a descriptive name, use it. Otherwise, fall-through
      if (m && m->name)
      {
        mutt_str_copy(tmp, m->name, sizeof(tmp));
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, tmp);
        break;
      }
    }
    /* fallthrough */
    case 'f':
    {
      struct Mailbox *m = Context ? Context->mailbox : NULL;

#ifdef USE_COMP_MBOX
      if (m && m->compress_info && (m->realpath[0] != '\0'))
      {
        mutt_str_copy(tmp, m->realpath, sizeof(tmp));
        mutt_pretty_mailbox(tmp, sizeof(tmp));
      }
      else
#endif
          if (m && (m->type == MUTT_NOTMUCH) && m->name)
      {
        mutt_str_copy(tmp, m->name, sizeof(tmp));
      }
      else if (m && !mutt_buffer_is_empty(&m->pathbuf))
      {
        mutt_str_copy(tmp, mailbox_path(m), sizeof(tmp));
        mutt_pretty_mailbox(tmp, sizeof(tmp));
      }
      else
        mutt_str_copy(tmp, _("(no mailbox)"), sizeof(tmp));

      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, tmp);
      break;
    }
    case 'F':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt,
                 (Context && Context->mailbox) ? Context->mailbox->msg_flagged : 0);
      }
      else if (!Context || !Context->mailbox || (Context->mailbox->msg_flagged == 0))
        optional = false;
      break;

    case 'h':
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, NONULL(ShortHostname));
      break;

    case 'l':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        mutt_str_pretty_size(tmp, sizeof(tmp),
                             (Context && Context->mailbox) ? Context->mailbox->size : 0);
        snprintf(buf, buflen, fmt, tmp);
      }
      else if (!Context || !Context->mailbox || !Context->mailbox->size)
        optional = false;
      break;

    case 'L':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        mutt_str_pretty_size(tmp, sizeof(tmp), Context ? Context->vsize : 0);
        snprintf(buf, buflen, fmt, tmp);
      }
      else if (!ctx_has_limit(Context))
        optional = false;
      break;

    case 'm':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt,
                 (Context && Context->mailbox) ? Context->mailbox->msg_count : 0);
      }
      else if (!Context || !Context->mailbox || (Context->mailbox->msg_count == 0))
        optional = false;
      break;

    case 'M':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt,
                 (Context && Context->mailbox) ? Context->mailbox->vcount : 0);
      }
      else if (!ctx_has_limit(Context))
        optional = false;
      break;

    case 'n':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt,
                 (Context && Context->mailbox) ? Context->mailbox->msg_new : 0);
      }
      else if (!Context || !Context->mailbox || (Context->mailbox->msg_new == 0))
        optional = false;
      break;

    case 'o':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt,
                 (Context && Context->mailbox) ?
                     (Context->mailbox->msg_unread - Context->mailbox->msg_new) :
                     0);
      }
      else if (!Context || !Context->mailbox ||
               ((Context->mailbox->msg_unread - Context->mailbox->msg_new) == 0))
        optional = false;
      break;

    case 'p':
    {
      int count = mutt_num_postponed(Context ? Context->mailbox : NULL, false);
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, count);
      }
      else if (count == 0)
        optional = false;
      break;
    }

    case 'P':
    {
      if (!menu)
        break;
      char *cp = NULL;
      if (menu->top + menu->pagelen >= menu->max)
      {
        cp = menu->top ?
                 /* L10N: Status bar message: the end of the list emails is visible in the index */
                 _("end") :
                 /* L10N: Status bar message: all the emails are visible in the index */
                 _("all");
      }
      else
      {
        int count = (100 * (menu->top + menu->pagelen)) / menu->max;
        snprintf(tmp, sizeof(tmp), "%d%%", count);
        cp = tmp;
      }
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, cp);
      break;
    }

    case 'r':
    {
      size_t i = 0;

      if (Context && Context->mailbox)
      {
        i = OptAttachMsg ?
                3 :
                ((Context->mailbox->readonly || Context->mailbox->dontwrite) ?
                     2 :
                     (Context->mailbox->changed ||
                      /* deleted doesn't necessarily mean changed in IMAP */
                      (Context->mailbox->type != MUTT_IMAP && Context->mailbox->msg_deleted)) ?
                     1 :
                     0);
      }

      if (!C_StatusChars || !C_StatusChars->len)
        buf[0] = '\0';
      else if (i >= C_StatusChars->len)
        snprintf(buf, buflen, "%s", C_StatusChars->chars[0]);
      else
        snprintf(buf, buflen, "%s", C_StatusChars->chars[i]);
      break;
    }

    case 'R':
    {
      int read = (Context && Context->mailbox) ?
                     (Context->mailbox->msg_count - Context->mailbox->msg_unread) :
                     0;

      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, read);
      }
      else if (read == 0)
        optional = false;
      break;
    }

    case 's':
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, get_sort_str(tmp, sizeof(tmp), C_Sort));
      break;

    case 'S':
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, get_sort_str(tmp, sizeof(tmp), C_SortAux));
      break;

    case 't':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt,
                 (Context && Context->mailbox) ? Context->mailbox->msg_tagged : 0);
      }
      else if (!Context || !Context->mailbox || (Context->mailbox->msg_tagged == 0))
        optional = false;
      break;

    case 'u':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt,
                 (Context && Context->mailbox) ? Context->mailbox->msg_unread : 0);
      }
      else if (!Context || !Context->mailbox || (Context->mailbox->msg_unread == 0))
        optional = false;
      break;

    case 'v':
      snprintf(buf, buflen, "%s", mutt_make_version());
      break;

    case 'V':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, ctx_has_limit(Context) ? Context->pattern : "");
      }
      else if (!ctx_has_limit(Context))
        optional = false;
      break;

    case 0:
      *buf = '\0';
      return src;

    default:
      snprintf(buf, buflen, "%%%s%c", prec, op);
      break;
  }

  if (optional)
  {
    mutt_expando_format(buf, buflen, col, cols, if_str, status_format_str,
                        (intptr_t) menu, MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str, status_format_str,
                        (intptr_t) menu, MUTT_FORMAT_NO_FLAGS);
  }

  return src;
}

/**
 * menu_status_line - Create the status line
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  menu     Current menu
 * @param[in]  p        Format string
 */
void menu_status_line(char *buf, size_t buflen, struct Menu *menu, const char *p)
{
  mutt_expando_format(buf, buflen, 0, menu ? menu->win_ibar->state.cols : buflen,
                      p, status_format_str, (intptr_t) menu, MUTT_FORMAT_NO_FLAGS);
}
