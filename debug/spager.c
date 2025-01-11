/**
 * @file
 * Simple Pager Debugging
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page debug_spager Simple Pager Debugging
 *
 * Simple Pager Debugging
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "debug/lib.h"
#include "pfile/lib.h"
#include "spager/lib.h"

void dump_markup(struct PagedTextMarkupArray *ptma, const char *label)
{
  mutt_debug(LL_DEBUG1, "    %s (%d)\n", label, ARRAY_SIZE(ptma));

  struct Buffer *buf = buf_pool_get();

  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, ptma)
  {
    buf_reset(buf);
    buf_add_printf(buf, "        [%d-%d] ", ptm->first, ptm->first + ptm->bytes - 1);

    if (ptm->cid > 0)
    {
      buf_add_printf(buf, "%s(%d) ", name_color_id(ptm->cid), ptm->cid);
      if (ptm->ac_text)
        buf_add_printf(buf, "ac_text %p ", (void *) ptm->ac_text);
    }
    else
    {
      buf_add_printf(buf, "[plain] ");
    }

    if (ptm->ac_merged)
      buf_add_printf(buf, "ac_merged %p ", (void *) ptm->ac_merged);

    // if ((ARRAY_FOREACH_IDX + 1) < ARRAY_SIZE(ptma))
    //   buf_add_printf(buf, "        --------------\n");

    mutt_debug(LL_DEBUG1, "%s\n", buf_string(buf));
  }

  buf_pool_release(&buf);
}

void dump_rows(struct PagedRowArray *pra)
{
  mutt_debug(LL_DEBUG1, "rows (%d)\n", ARRAY_SIZE(pra));
  int count = 0;
  struct PagedRow *pr = NULL;
  ARRAY_FOREACH(pr, pra)
  {
    if (count++ > 10)
      break;
    mutt_debug(LL_DEBUG1, "    offset %ld\n", pr->offset);
    mutt_debug(LL_DEBUG1, "    %d bytes, %d cols\n", pr->num_bytes, pr->num_cols);
    if (pr->cid > 0)
    {
      mutt_debug(LL_DEBUG1, "    cid %s (%d)\n", name_color_id(pr->cid), pr->cid);
      if (pr->ac_row)
        mutt_debug(LL_DEBUG1, "    ac_row %p\n", (void *) pr->ac_row);
    }
    else
    {
      mutt_debug(LL_DEBUG1, "    [plain]\n");
    }

    dump_markup(&pr->text, "text");
    dump_markup(&pr->search, "search");
    mutt_debug(LL_DEBUG1, "======================\n");
  }
}

void dump_spager(struct PagedFile *pf)
{
  if (!pf)
    return;

  mutt_debug(LL_DEBUG1, "PagedFile\n");
  mutt_debug(LL_DEBUG1, "fd %d\n", fileno(pf->fp));
  dump_rows(&pf->rows);
}
