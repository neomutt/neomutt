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

#include "config.h"
#include <stdio.h>
#include "context.h"
#include "format_flags.h"
#include "globals.h"
#include "lib.h"
#include "mapping.h"
#include "mbyte_table.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mx.h"
#include "options.h"
#include "protos.h"
#include "sort.h"
#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif

static char *get_sort_str(char *buf, size_t buflen, int method)
{
  snprintf(buf, buflen, "%s%s%s", (method & SORT_REVERSE) ? "reverse-" : "",
           (method & SORT_LAST) ? "last-" : "",
           mutt_getnamebyvalue(method & SORT_MASK, SortMethods));
  return buf;
}

static void _menu_status_line(char *buf, size_t buflen, size_t col, int cols,
                              struct Menu *menu, const char *p);

/**
 * status_format_str - Format a string for the status bar
 *
 * | Expando | Description
 * |:--------|:----------------------------------------------------------------
 * | \%b     | number of incoming folders with unread messages [option]
 * | \%d     | number of deleted messages [option]
 * | \%f     | full mailbox path
 * | \%F     | number of flagged messages [option]
 * | \%h     | hostname
 * | \%l     | length of mailbox (in bytes) [option]
 * | \%m     | total number of messages [option]
 * | \%M     | number of messages shown (virtual message count when limiting) [option]
 * | \%n     | number of new messages [option]
 * | \%o     | number of old unread messages [option]
 * | \%p     | number of postponed messages [option]
 * | \%P     | percent of way through index
 * | \%r     | readonly/wontwrite/changed flag
 * | \%s     | current sorting method ($sort)
 * | \%S     | current aux sorting method ($sort_aux)
 * | \%t     | # of tagged messages [option]
 * | \%u     | number of unread messages [option]
 * | \%v     | Mutt version
 * | \%V     | currently active limit pattern [option]
 */
static const char *status_format_str(char *buf, size_t buflen, size_t col, int cols,
                                     char op, const char *src, const char *prefix,
                                     const char *ifstring, const char *elsestring,
                                     unsigned long data, enum FormatFlag flags)
{
  char fmt[SHORT_STRING], tmp[SHORT_STRING], *cp = NULL;
  int count, optional = (flags & MUTT_FORMAT_OPTIONAL);
  struct Menu *menu = (struct Menu *) data;

  *buf = 0;
  switch (op)
  {
    case 'b':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(buf, buflen, fmt, mutt_buffy_check(false));
      }
      else if (!mutt_buffy_check(false))
        optional = 0;
      break;

    case 'd':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(buf, buflen, fmt, Context ? Context->deleted : 0);
      }
      else if (!Context || !Context->deleted)
        optional = 0;
      break;

    case 'f':
    {
#ifdef USE_NOTMUCH
      char *p = NULL;
      if (Context && Context->magic == MUTT_NOTMUCH && (p = nm_get_description(Context)))
        strfcpy(tmp, p, sizeof(tmp));
      else
#endif
#ifdef USE_COMPRESSED
          if (Context && Context->compress_info && Context->realpath)
      {
        strfcpy(tmp, Context->realpath, sizeof(tmp));
        mutt_pretty_mailbox(tmp, sizeof(tmp));
      }
      else
#endif
          if (Context && Context->path)
      {
        strfcpy(tmp, Context->path, sizeof(tmp));
        mutt_pretty_mailbox(tmp, sizeof(tmp));
      }
      else
        strfcpy(tmp, _("(no mailbox)"), sizeof(tmp));

      snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
      snprintf(buf, buflen, fmt, tmp);
      break;
    }
    case 'F':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(buf, buflen, fmt, Context ? Context->flagged : 0);
      }
      else if (!Context || !Context->flagged)
        optional = 0;
      break;

    case 'h':
      snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
      snprintf(buf, buflen, fmt, NONULL(Hostname));
      break;

    case 'l':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        mutt_pretty_size(tmp, sizeof(tmp), Context ? Context->size : 0);
        snprintf(buf, buflen, fmt, tmp);
      }
      else if (!Context || !Context->size)
        optional = 0;
      break;

    case 'L':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        mutt_pretty_size(tmp, sizeof(tmp), Context ? Context->vsize : 0);
        snprintf(buf, buflen, fmt, tmp);
      }
      else if (!Context || !Context->pattern)
        optional = 0;
      break;

    case 'm':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(buf, buflen, fmt, Context ? Context->msgcount : 0);
      }
      else if (!Context || !Context->msgcount)
        optional = 0;
      break;

    case 'M':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(buf, buflen, fmt, Context ? Context->vcount : 0);
      }
      else if (!Context || !Context->pattern)
        optional = 0;
      break;

    case 'n':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(buf, buflen, fmt, Context ? Context->new : 0);
      }
      else if (!Context || !Context->new)
        optional = 0;
      break;

    case 'o':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(buf, buflen, fmt, Context ? Context->unread - Context->new : 0);
      }
      else if (!Context || !(Context->unread - Context->new))
        optional = 0;
      break;

    case 'p':
      count = mutt_num_postponed(0);
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(buf, buflen, fmt, count);
      }
      else if (!count)
        optional = 0;
      break;

    case 'P':
      if (!menu)
        break;
      if (menu->top + menu->pagelen >= menu->max)
        cp = menu->top ? "end" : "all";
      else
      {
        count = (100 * (menu->top + menu->pagelen)) / menu->max;
        snprintf(tmp, sizeof(tmp), "%d%%", count);
        cp = tmp;
      }
      snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
      snprintf(buf, buflen, fmt, cp);
      break;

    case 'r':
    {
      size_t i = 0;

      if (Context)
      {
        i = option(OPTATTACHMSG) ?
                3 :
                ((Context->readonly || Context->dontwrite) ?
                     2 :
                     (Context->changed ||
                      /* deleted doesn't necessarily mean changed in IMAP */
                      (Context->magic != MUTT_IMAP && Context->deleted)) ?
                     1 :
                     0);
      }

      if (!StChars || !StChars->len)
        buf[0] = 0;
      else if (i >= StChars->len)
        snprintf(buf, buflen, "%s", StChars->chars[0]);
      else
        snprintf(buf, buflen, "%s", StChars->chars[i]);
      break;
    }

    case 's':
      snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
      snprintf(buf, buflen, fmt, get_sort_str(tmp, sizeof(tmp), Sort));
      break;

    case 'S':
      snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
      snprintf(buf, buflen, fmt, get_sort_str(tmp, sizeof(tmp), SortAux));
      break;

    case 't':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(buf, buflen, fmt, Context ? Context->tagged : 0);
      }
      else if (!Context || !Context->tagged)
        optional = 0;
      break;

    case 'u':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prefix);
        snprintf(buf, buflen, fmt, Context ? Context->unread : 0);
      }
      else if (!Context || !Context->unread)
        optional = 0;
      break;

    case 'v':
      snprintf(fmt, sizeof(fmt), "NeoMutt %%s%%s");
      snprintf(buf, buflen, fmt, PACKAGE_VERSION, GitVer);
      break;

    case 'V':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prefix);
        snprintf(buf, buflen, fmt, (Context && Context->pattern) ? Context->pattern : "");
      }
      else if (!Context || !Context->pattern)
        optional = 0;
      break;

    case 0:
      *buf = 0;
      return src;

    default:
      snprintf(buf, buflen, "%%%s%c", prefix, op);
      break;
  }

  if (optional)
    _menu_status_line(buf, buflen, col, cols, menu, ifstring);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    _menu_status_line(buf, buflen, col, cols, menu, elsestring);

  return src;
}

static void _menu_status_line(char *buf, size_t buflen, size_t col, int cols,
                              struct Menu *menu, const char *p)
{
  mutt_expando_format(buf, buflen, col, cols, p, status_format_str, (unsigned long) menu, 0);
}

void menu_status_line(char *buf, size_t buflen, struct Menu *menu, const char *p)
{
  mutt_expando_format(buf, buflen, 0,
                    menu ? menu->statuswin->cols : MuttStatusWindow->cols, p,
                    status_format_str, (unsigned long) menu, 0);
}
