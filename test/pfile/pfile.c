/**
 * @file
 * Test code for the Paged File
 *
 * @authors
 * Copyright (C) 2024-2025 Richard Russon <rich@flatcap.org>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "debug/lib.h"
#include "color/lib.h"
#include "pfile/lib.h"
#include "test_common.h"

struct PfileTest
{
  const char *name;
  bool use_text;
  int search_start;
  int search_end;
};

static void dump_paged_markup(const struct PagedTextMarkup *ptm, struct Buffer *buf)
{
  if (!ptm || !buf)
    return;

  buf_add_printf(buf, "(%d:%d-%d)", ptm->cid, ptm->first, ptm->first + ptm->bytes - 1);
}

static void dump_paged_row(const struct PagedRow *pr, struct Buffer *buf)
{
  if (!pr || !buf)
    return;

  buf_add_printf(buf, "{b%d:c%d:", pr->num_bytes, pr->num_cols);

  struct PagedTextMarkup *ptm = NULL;
  size_t count = ARRAY_SIZE(&pr->text);
  if (count > 0)
  {
    buf_addstr(buf, "T:{");
    ARRAY_FOREACH(ptm, &pr->text)
    {
      dump_paged_markup(ptm, buf);
      if ((ARRAY_FOREACH_IDX_ptm + 1) < count)
        buf_addstr(buf, ",");
    }
    buf_addstr(buf, "}");
  }

  count = ARRAY_SIZE(&pr->search);
  if (count > 0)
  {
    buf_addstr(buf, ",S:{");
    ARRAY_FOREACH(ptm, &pr->search)
    {
      dump_paged_markup(ptm, buf);
      if ((ARRAY_FOREACH_IDX_ptm + 1) < count)
        buf_addstr(buf, ",");
    }
    buf_addstr(buf, "}");
  }

  buf_addstr(buf, "}");
}

static void dump_paged_file(const struct PagedFile *pf, struct Buffer *buf)
{
  if (!pf || !buf)
    return;

  buf_add_printf(buf, "L:%d:{", ARRAY_SIZE(&pf->rows));

  struct PagedRow *pr = NULL;
  size_t count = ARRAY_SIZE(&pf->rows);
  ARRAY_FOREACH(pr, &pf->rows)
  {
    dump_paged_row(pr, buf);
    if ((ARRAY_FOREACH_IDX_pr + 1) < count)
      buf_addstr(buf, ",");
  }

  buf_addstr(buf, "}");
}

void test_pfile(void)
{
  static const char *result =
      "L:16:{{b21:c21:},{b21:c21:T:{(21:6-14)}},{b21:c21:T:{(21:6-14)},S:{(41:0-2)}},"
      "{b21:c21:T:{(21:6-14)},S:{(41:3-8)}},{b21:c21:T:{(21:6-14)},S:{(41:3-11)}},"
      "{b21:c21:T:{(21:6-14)},S:{(41:3-17)}},{b21:c21:T:{(21:6-14)},S:{(41:3-20)}},"
      "{b21:c21:T:{(21:6-14)},S:{(41:6-14)}},{b21:c21:T:{(21:6-14)},S:{(41:6-20)}},"
      "{b21:c21:T:{(21:6-14)},S:{(41:6-23)}},{b21:c21:T:{(21:6-14)},S:{(41:9-20)}},"
      "{b21:c21:T:{(21:6-14)},S:{(41:12-26)}},{b21:c21:T:{(21:6-14)},S:{(41:12-29)}},"
      "{b21:c21:T:{(21:6-14)},S:{(41:15-32)}},{b21:c21:T:{(21:6-14)},S:{(41:18-38)}},"
      "{b21:c21:,S:{(41:6-20)}}}";

  static const struct PfileTest Tests[] = {
    // clang-format off
    { "A", false, -1, -1 },
    { "B", true,  -1, -1 },
    { "C", true,   0,  3 },
    { "D", true,   3,  6 },
    { "E", true,   3,  9 },
    { "F", true,   3, 15 },
    { "G", true,   3, 18 },
    { "H", true,   6,  9 },
    { "I", true,   6, 15 },
    { "J", true,   6, 18 },
    { "K", true,   9, 12 },
    { "L", true,  12, 15 },
    { "M", true,  12, 18 },
    { "N", true,  15, 18 },
    { "O", true,  18, 21 },
    { "P", false,  6, 15 },
    // clang-format on
  };

  struct PagedFile *pf = paged_file_new(NULL);
  TEST_CHECK(pf != NULL);

  for (int i = 0; i < countof(Tests); i++)
  {
    const struct PfileTest *st = &Tests[i];

    TEST_CASE(st->name);

    struct PagedRow *pr = paged_file_new_row(pf);
    TEST_CHECK(pr != NULL);

    if (st->use_text)
    {
      paged_row_add_text(pf->source, pr, "AAAbbb");
      paged_row_add_colored_text(pf->source, pr, MT_COLOR_INDICATOR, "CCCdddEEE");
      paged_row_add_text(pf->source, pr, "fffGGG");
    }
    else
    {
      paged_row_add_text(pf->source, pr, "AAAbbbCCCdddEEEfffGGG");
    }

    if (st->search_start >= 0)
    {
      paged_row_add_search(pr, st->search_start, st->search_end);
    }
  }

  struct Buffer *buf = buf_pool_get();
  dump_paged_file(pf, buf);
  TEST_CHECK_STR_EQ(buf_string(buf), result);
  buf_pool_release(&buf);
  paged_file_free(&pf);
}
