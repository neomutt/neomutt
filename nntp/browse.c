/**
 * @file
 * Browse NNTP groups
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page nntp_browse Browse NNTP groups
 *
 * Browse NNTP groups
 */

#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "lib.h"
#include "browser.h"
#include "format_flags.h"
#include "muttlib.h"

/**
 * group_index_format_str - Format a string for the newsgroup menu - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%C     | Current newsgroup number
 * | \%d     | Description of newsgroup (becomes from server)
 * | \%f     | Newsgroup name
 * | \%M     | - if newsgroup not allowed for direct post (moderated for example)
 * | \%N     | N if newsgroup is new, u if unsubscribed, blank otherwise
 * | \%n     | Number of new articles in newsgroup
 * | \%s     | Number of unread articles in newsgroup
 */
const char *group_index_format_str(char *buf, size_t buflen, size_t col, int cols,
                                   char op, const char *src, const char *prec,
                                   const char *if_str, const char *else_str,
                                   intptr_t data, MuttFormatFlags flags)
{
  char fn[128], fmt[128];
  struct Folder *folder = (struct Folder *) data;

  switch (op)
  {
    case 'C':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, folder->num + 1);
      break;

    case 'd':
      if (folder->ff->nd->desc)
      {
        char *desc = mutt_str_dup(folder->ff->nd->desc);
        if (C_NewsgroupsCharset)
          mutt_ch_convert_string(&desc, C_NewsgroupsCharset, C_Charset, MUTT_ICONV_HOOK_FROM);
        mutt_mb_filter_unprintable(&desc);

        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, desc);
        FREE(&desc);
      }
      else
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, "");
      }
      break;

    case 'f':
      mutt_str_copy(fn, folder->ff->name, sizeof(fn));
      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, fn);
      break;

    case 'M':
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      if (folder->ff->nd->deleted)
        snprintf(buf, buflen, fmt, 'D');
      else
        snprintf(buf, buflen, fmt, folder->ff->nd->allowed ? ' ' : '-');
      break;

    case 'N':
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      if (folder->ff->nd->subscribed)
        snprintf(buf, buflen, fmt, ' ');
      else
        snprintf(buf, buflen, fmt, folder->ff->has_new_mail ? 'N' : 'u');
      break;

    case 'n':
      if (C_MarkOld && (folder->ff->nd->last_cached >= folder->ff->nd->first_message) &&
          (folder->ff->nd->last_cached <= folder->ff->nd->last_message))
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, folder->ff->nd->last_message - folder->ff->nd->last_cached);
      }
      else
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, folder->ff->nd->unread);
      }
      break;

    case 's':
      if (flags & MUTT_FORMAT_OPTIONAL)
      {
        if (folder->ff->nd->unread != 0)
        {
          mutt_expando_format(buf, buflen, col, cols, if_str,
                              group_index_format_str, data, flags);
        }
        else
        {
          mutt_expando_format(buf, buflen, col, cols, else_str,
                              group_index_format_str, data, flags);
        }
      }
      else
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, folder->ff->nd->unread);
      }
      break;
  }
  return src;
}
