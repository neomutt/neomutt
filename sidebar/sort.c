/**
 * @file
 * Sidebar sort functions
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page sidebar_sort Sidebar sort functions
 *
 * Sidebar sort functions
 */

#include "config.h"
#include <stdbool.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "sort.h"
#include "muttlib.h"

/// An extra parameter to control sort order
static bool sb_sort_reverse = false;

/**
 * sb_sort_count - Sort Sidebar entries by count - Implements ::sort_t - @ingroup sort_api
 */
static int sb_sort_count(const void *a, const void *b)
{
  const struct SbEntry *sbe1 = *(struct SbEntry const *const *) a;
  const struct SbEntry *sbe2 = *(struct SbEntry const *const *) b;
  const struct Mailbox *m1 = sbe1->mailbox;
  const struct Mailbox *m2 = sbe2->mailbox;

  int rc = 0;
  if (m1->msg_count == m2->msg_count)
    rc = mutt_str_coll(mailbox_path(m1), mailbox_path(m2));
  else
    rc = (m2->msg_count - m1->msg_count);

  if (sb_sort_reverse)
    rc = -rc;
  return rc;
}

/**
 * sb_sort_desc - Sort Sidebar entries by description - Implements ::sort_t - @ingroup sort_api
 */
static int sb_sort_desc(const void *a, const void *b)
{
  const struct SbEntry *sbe1 = *(struct SbEntry const *const *) a;
  const struct SbEntry *sbe2 = *(struct SbEntry const *const *) b;
  const struct Mailbox *m1 = sbe1->mailbox;
  const struct Mailbox *m2 = sbe2->mailbox;

  int rc = mutt_str_cmp(m1->name, m2->name);

  if (sb_sort_reverse)
    rc = -rc;
  return rc;
}

/**
 * sb_sort_flagged - Sort Sidebar entries by flagged - Implements ::sort_t - @ingroup sort_api
 */
static int sb_sort_flagged(const void *a, const void *b)
{
  const struct SbEntry *sbe1 = *(struct SbEntry const *const *) a;
  const struct SbEntry *sbe2 = *(struct SbEntry const *const *) b;
  const struct Mailbox *m1 = sbe1->mailbox;
  const struct Mailbox *m2 = sbe2->mailbox;

  int rc = 0;
  if (m1->msg_flagged == m2->msg_flagged)
    rc = mutt_str_coll(mailbox_path(m1), mailbox_path(m2));
  else
    rc = (m2->msg_flagged - m1->msg_flagged);

  if (sb_sort_reverse)
    rc = -rc;
  return rc;
}

/**
 * sb_sort_path - Sort Sidebar entries by path - Implements ::sort_t - @ingroup sort_api
 */
static int sb_sort_path(const void *a, const void *b)
{
  const struct SbEntry *sbe1 = *(struct SbEntry const *const *) a;
  const struct SbEntry *sbe2 = *(struct SbEntry const *const *) b;
  const struct Mailbox *m1 = sbe1->mailbox;
  const struct Mailbox *m2 = sbe2->mailbox;

  int rc = 0;
  rc = mutt_inbox_cmp(mailbox_path(m1), mailbox_path(m2));
  if (rc == 0)
    rc = mutt_str_coll(mailbox_path(m1), mailbox_path(m2));

  if (sb_sort_reverse)
    rc = -rc;
  return rc;
}

/**
 * sb_sort_unread - Sort Sidebar entries by unread - Implements ::sort_t - @ingroup sort_api
 */
static int sb_sort_unread(const void *a, const void *b)
{
  const struct SbEntry *sbe1 = *(struct SbEntry const *const *) a;
  const struct SbEntry *sbe2 = *(struct SbEntry const *const *) b;
  const struct Mailbox *m1 = sbe1->mailbox;
  const struct Mailbox *m2 = sbe2->mailbox;

  int rc = 0;
  if (m1->msg_unread == m2->msg_unread)
    rc = mutt_str_coll(mailbox_path(m1), mailbox_path(m2));
  else
    rc = (m2->msg_unread - m1->msg_unread);

  if (sb_sort_reverse)
    rc = -rc;
  return rc;
}

/**
 * sb_sort_order - Sort Sidebar entries by order of creation - Implements ::sort_t - @ingroup sort_api
 */
static int sb_sort_order(const void *a, const void *b)
{
  const struct SbEntry *sbe1 = *(struct SbEntry const *const *) a;
  const struct SbEntry *sbe2 = *(struct SbEntry const *const *) b;
  const struct Mailbox *m1 = sbe1->mailbox;
  const struct Mailbox *m2 = sbe2->mailbox;

  return (sb_sort_reverse ? -1 : 1) * (m1->gen - m2->gen);
}

/**
 * sb_sort_unsorted - Sort Sidebar entries into their original order - Implements ::sort_t - @ingroup sort_api
 */
static int sb_sort_unsorted(const void *a, const void *b)
{
  const struct SbEntry *sbe1 = *(struct SbEntry const *const *) a;
  const struct SbEntry *sbe2 = *(struct SbEntry const *const *) b;

  // This sort method isn't affected by the reverse flag
  return (sbe1->mailbox->gen - sbe2->mailbox->gen);
}

/**
 * sb_sort_entries - Sort the Sidebar entries
 * @param wdata Sidebar data
 * @param sort  Sort order, e.g. #SORT_PATH
 *
 * Sort the `wdata->entries` array according to the current sort config option
 * `$sidebar_sort_method`. This calls qsort to do the work which calls our
 * callback function "cb_qsort_sbe".
 *
 * Once sorted, the prev/next links will be reconstructed.
 */
void sb_sort_entries(struct SidebarWindowData *wdata, enum SortType sort)
{
  sort_t fn = sb_sort_unsorted;

  switch (sort & SORT_MASK)
  {
    case SORT_COUNT:
      fn = sb_sort_count;
      break;
    case SORT_DESC:
      fn = sb_sort_desc;
      break;
    case SORT_FLAGGED:
      fn = sb_sort_flagged;
      break;
    case SORT_PATH:
      fn = sb_sort_path;
      break;
    case SORT_UNREAD:
      fn = sb_sort_unread;
      break;
    case SORT_ORDER:
      fn = sb_sort_order;
    default:
      break;
  }

  sb_sort_reverse = (sort & SORT_REVERSE);
  ARRAY_SORT(&wdata->entries, fn);
}
