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
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "muttlib.h"
#include "options.h"

/**
 * browser_compare_subject - Compare the subject of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_compare_subject(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  /* inbox should be sorted ahead of its siblings */
  int r = mutt_inbox_cmp(pa->name, pb->name);
  if (r == 0)
    r = mutt_str_coll(pa->name, pb->name);
  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_order - Compare the order of creation of two browser entries - Implements ::sort_t - @ingroup sort_api
 *
 * @note This only affects browsing mailboxes and is a no-op for folders.
 */
static int browser_compare_order(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return ((c_sort_browser & SORT_REVERSE) ? -1 : 1) * (pa->gen - pb->gen);
}

/**
 * browser_compare_desc - Compare the descriptions of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_compare_desc(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = mutt_str_coll(pa->desc, pb->desc);

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_date - Compare the date of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_compare_date(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = pa->mtime - pb->mtime;

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_size - Compare the size of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_compare_size(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = pa->size - pb->size;

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_count - Compare the message count of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_compare_count(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = 0;
  if (pa->has_mailbox && pb->has_mailbox)
    r = pa->msg_count - pb->msg_count;
  else if (pa->has_mailbox)
    r = -1;
  else
    r = 1;

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_count_new - Compare the new count of two browser entries - Implements ::sort_t - @ingroup sort_api
 */
static int browser_compare_count_new(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = 0;
  if (pa->has_mailbox && pb->has_mailbox)
    r = pa->msg_unread - pb->msg_unread;
  else if (pa->has_mailbox)
    r = -1;
  else
    r = 1;

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare - Sort the items in the browser - Implements ::sort_t - @ingroup sort_api
 *
 * Wild compare function that calls the others. It's useful because it provides
 * a way to tell "../" is always on the top of the list, independently of the
 * sort method.
 */
static int browser_compare(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  if ((mutt_str_coll(pa->desc, "../") == 0) || (mutt_str_coll(pa->desc, "..") == 0))
    return -1;
  if ((mutt_str_coll(pb->desc, "../") == 0) || (mutt_str_coll(pb->desc, "..") == 0))
    return 1;

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  switch (c_sort_browser & SORT_MASK)
  {
    case SORT_COUNT:
      return browser_compare_count(a, b);
    case SORT_DATE:
      return browser_compare_date(a, b);
    case SORT_DESC:
      return browser_compare_desc(a, b);
    case SORT_SIZE:
      return browser_compare_size(a, b);
    case SORT_UNREAD:
      return browser_compare_count_new(a, b);
    case SORT_SUBJECT:
      return browser_compare_subject(a, b);
    default:
    case SORT_ORDER:
      return browser_compare_order(a, b);
  }
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
  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
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

  ARRAY_SORT(&state->entry, browser_compare);
}
