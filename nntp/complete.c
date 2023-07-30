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
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "lib.h"
#include "adata.h"
#include "mdata.h"

/**
 * nntp_complete - Auto-complete NNTP newsgroups
 * @param buf Buffer containing pathname
 * @retval  0 Match found
 * @retval -1 No matches
 *
 * XXX rules
 */
int nntp_complete(struct Buffer *buf)
{
  struct NntpAccountData *adata = CurrentNewsSrv;
  size_t n = 0;
  struct Buffer *filepart = buf_new(buf_string(buf));
  bool init = false;

  /* special case to handle when there is no filepart yet
   * find the first subscribed newsgroup */
  int len = buf_len(filepart);
  if (len == 0)
  {
    for (; n < adata->groups_num; n++)
    {
      struct NntpMboxData *mdata = adata->groups_list[n];

      if (mdata && mdata->subscribed)
      {
        buf_strcpy(filepart, mdata->group);
        init = true;
        n++;
        break;
      }
    }
  }

  for (; n < adata->groups_num; n++)
  {
    struct NntpMboxData *mdata = adata->groups_list[n];

    if (mdata && mdata->subscribed &&
        mutt_strn_equal(mdata->group, buf_string(filepart), len))
    {
      if (init)
      {
        char *str = filepart->data;
        size_t i;
        for (i = 0; (str[i] != '\0') && mdata->group[i]; i++)
        {
          if (str[i] != mdata->group[i])
            break;
        }
        str[i] = '\0';
        buf_fix_dptr(filepart);
      }
      else
      {
        buf_strcpy(filepart, mdata->group);
        init = true;
      }
    }
  }

  buf_copy(buf, filepart);
  buf_free(&filepart);
  return init ? 0 : -1;
}
