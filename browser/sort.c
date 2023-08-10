/**
 * @file
 * Browser sorting
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page browser_sorting Browser sorting
 *
 * Browser sorting
 */

#include "config.h"
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "sort.h"
#include "lib.h"
#include "globals.h"
#include "muttlib.h"

/**
 * browser_sort_subject - Compare the subject of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_sort_subject(const void *a, const void *b, void *arg)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  /* inbox should be sorted ahead of its siblings */
  int rc = mutt_inbox_cmp(pa->name, pb->name);
  if (rc == 0)
    rc = mutt_str_coll(pa->name, pb->name);

  return rc;
}

/**
 * browser_sort_order - Compare the order of creation of two browser entries - Implements ::sort_t - @ingroup sort_api
 *
 * @note This only affects browsing mailboxes and is a no-op for folders.
 */
static int browser_sort_order(const void *a, const void *b, void *arg)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  return mutt_numeric_cmp(pa->gen, pb->gen);
}

/**
 * browser_sort_desc - Compare the descriptions of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_sort_desc(const void *a, const void *b, void *arg)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  return mutt_str_coll(pa->desc, pb->desc);
}

/**
 * browser_sort_date - Compare the date of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_sort_date(const void *a, const void *b, void *arg)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  return mutt_numeric_cmp(pa->mtime, pb->mtime);
}

/**
 * browser_sort_size - Compare the size of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_sort_size(const void *a, const void *b, void *arg)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  return mutt_numeric_cmp(pa->size, pb->size);
}

/**
 * browser_sort_count - Compare the message count of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_sort_count(const void *a, const void *b, void *arg)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int rc = 0;
  if (pa->has_mailbox && pb->has_mailbox)
    rc = mutt_numeric_cmp(pa->msg_count, pb->msg_count);
  else if (pa->has_mailbox)
    rc = -1;
  else
    rc = 1;

  return rc;
}

/**
 * browser_sort_count_new - Compare the new count of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_sort_count_new(const void *a, const void *b, void *arg)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int rc = 0;
  if (pa->has_mailbox && pb->has_mailbox)
    rc = mutt_numeric_cmp(pa->msg_unread, pb->msg_unread);
  else if (pa->has_mailbox)
    rc = -1;
  else
    rc = 1;

  return rc;
}

/**
 * browser_compare - Sort the items in the browser - Implements ::sort_t - @ingroup sort_api
 *
 * Wild compare function that calls the others. It's useful because it provides
 * a way to tell "../" is always on the top of the list, independently of the
 * sort method.  $browser_sort_dirs_first is also handled here.
 */
static int browser_compare(const void *a, const void *b, void *arg)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;
  const enum SortType c_sort_browser = *(enum SortType *) arg;

  if ((mutt_str_coll(pa->desc, "../") == 0) || (mutt_str_coll(pa->desc, "..") == 0))
    return -1;
  if ((mutt_str_coll(pb->desc, "../") == 0) || (mutt_str_coll(pb->desc, "..") == 0))
    return 1;

  if (cs_subset_bool(NeoMutt->sub, "browser_sort_dirs_first"))
    if (S_ISDIR(pa->mode) != S_ISDIR(pb->mode))
      return S_ISDIR(pa->mode) ? -1 : 1;

  int rc;
  switch (c_sort_browser & SORT_MASK)
  {
    case SORT_COUNT:
      rc = browser_sort_count(a, b, NULL);
      break;
    case SORT_DATE:
      rc = browser_sort_date(a, b, NULL);
      break;
    case SORT_DESC:
      rc = browser_sort_desc(a, b, NULL);
      break;
    case SORT_SIZE:
      rc = browser_sort_size(a, b, NULL);
      break;
    case SORT_UNREAD:
      rc = browser_sort_count_new(a, b, NULL);
      break;
    case SORT_SUBJECT:
      rc = browser_sort_subject(a, b, NULL);
      break;
    default:
    case SORT_ORDER:
      rc = browser_sort_order(a, b, NULL);
      break;
  }

  return c_sort_browser & SORT_REVERSE ? -rc : rc;
}

/**
 * browser_sort - Sort the entries in the browser
 * @param state Browser state
 *
 * Call to qsort using browser_compare function.
 * Some specific sort methods are not used via NNTP.
 */
void browser_sort(struct BrowserState *state)
{
  enum SortType c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  switch (c_sort_browser & SORT_MASK)
  {
#ifdef USE_NNTP
    case SORT_SIZE:
    case SORT_DATE:
      if (OptNews)
        return;
#endif
    default:
      break;
  }

  ARRAY_SORT(&state->entry, browser_compare, &c_sort_browser);
}
