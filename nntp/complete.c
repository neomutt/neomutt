/**
 * @file
 * Auto-complete NNTP newsgroups
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
 * @page nntp_complete Auto-complete NNTP newsgroups
 *
 * Auto-complete NNTP newsgroups
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * nntp_complete - Auto-complete NNTP newsgroups
 * @param buf    Buffer containing pathname
 * @param buflen Length of buffer
 * @retval  0 Match found
 * @retval -1 No matches
 *
 * XXX rules
 */
int nntp_complete(char *buf, size_t buflen)
{
  struct NntpAccountData *adata = CurrentNewsSrv;
  size_t n = 0;
  char filepart[PATH_MAX];
  bool init = false;

  mutt_str_copy(filepart, buf, sizeof(filepart));

  /* special case to handle when there is no filepart yet
   * find the first subscribed newsgroup */
  int len = mutt_str_len(filepart);
  if (len == 0)
  {
    for (; n < adata->groups_num; n++)
    {
      struct NntpMboxData *mdata = adata->groups_list[n];

      if (mdata && mdata->subscribed)
      {
        mutt_str_copy(filepart, mdata->group, sizeof(filepart));
        init = true;
        n++;
        break;
      }
    }
  }

  for (; n < adata->groups_num; n++)
  {
    struct NntpMboxData *mdata = adata->groups_list[n];

    if (mdata && mdata->subscribed && mutt_strn_equal(mdata->group, filepart, len))
    {
      if (init)
      {
        size_t i;
        for (i = 0; filepart[i] && mdata->group[i]; i++)
        {
          if (filepart[i] != mdata->group[i])
            break;
        }
        filepart[i] = '\0';
      }
      else
      {
        mutt_str_copy(filepart, mdata->group, sizeof(filepart));
        init = true;
      }
    }
  }

  mutt_str_copy(buf, filepart, buflen);
  return init ? 0 : -1;
}
