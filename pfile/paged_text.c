/**
 * @file
 * Markup for text for the Simple Pager
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

/**
 * @page pfile_paged_text Markup for text for the Simple Pager
 *
 * Markup for text for the Simple Pager
 */

#include "config.h"
#include <stddef.h>
#include "paged_text.h"

/**
 * enum MarkupIntersect - XXX
 */
enum MarkupIntersect
{
  MI_BEFORE, ///< XXX
  MI_START,  ///< XXX
  MI_MIDDLE, ///< XXX
  MI_ALL,    ///< XXX
  MI_END,    ///< XXX
  MI_AFTER,  ///< XXX
};


/**
 * markup_dump - XXX
 */
void markup_dump(const struct PagedTextMarkupArray *ptma, int first, int last)
{
  if (!ptma)
    return;

  int count = 0;
  struct Buffer *buf = buf_pool_get();

  buf_addstr(buf, "M:");
  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, ptma)
  {
    int mod = 0;
    buf_addstr(buf, "(");
    for (int i = 0; i < ptm->bytes; i++, count++)
    {
      if (ptm->first >= 100)
      {
        buf_addstr(buf, "\033[1;7;33m"); // Yellow
        mod = -100;
      }
      else if ((count < first) || (count > last))
      {
        buf_addstr(buf, "\033[1;32m"); // Green
      }
      else
      {
        buf_addstr(buf, "\033[1;7;31m"); // Red
      }

      buf_add_printf(buf, "%d", ptm->first + i + mod);
      buf_addstr(buf, "\033[0m");

      if (i < (ptm->bytes - 1))
        buf_addstr(buf, ",");
    }
    buf_addstr(buf, ")");

    if (ARRAY_FOREACH_IDX_ptm < (ARRAY_SIZE(ptma) - 1))
      buf_addstr(buf, ",");
  }

  mutt_debug(LL_DEBUG1, "%s\n", buf_string(buf));

  buf_pool_release(&buf);
}

/**
 * paged_text_markup_new - Create a new PagedTextMarkup
 * @param ptma PagedTextMarkupArray to add to
 * @retval ptr New PagedTextMarkup
 */
struct PagedTextMarkup *paged_text_markup_new(struct PagedTextMarkupArray *ptma)
{
  if (!ptma)
    return NULL;

  struct PagedTextMarkup ptm = { 0 };
  ARRAY_ADD(ptma, ptm);

  return ARRAY_LAST(ptma);
}


/**
 * markup_intersect - XXX
 */
enum MarkupIntersect markup_intersect(struct PagedTextMarkup *ptm, int first, int bytes, int *ifirst, int *ibytes)
{
  if ((first + bytes) <= ptm->first)
  {
    *ifirst = -1;
    *ibytes = -1;
    return MI_BEFORE;
  }

  if (first >= (ptm->first + ptm->bytes))
  {
    *ifirst = -1;
    *ibytes = -1;
    return MI_AFTER;
  }

  if (first <= ptm->first)
  {
    if ((first + bytes) >= (ptm->first + ptm->bytes))
    {
      *ifirst = ptm->first;
      *ibytes = ptm->bytes;
      return MI_ALL;
    }
    else
    {
      *ifirst = ptm->first;
      *ibytes = first + bytes - ptm->first;
      return MI_START;
    }
  }
  else
  {
    if ((first + bytes) >= (ptm->first + ptm->bytes))
    {
      *ifirst = first;
      *ibytes = ptm->first + ptm->bytes - first;
      return MI_END;
    }
    else
    {
      *ifirst = first;
      *ibytes = bytes;
      return MI_MIDDLE;
    }
  }
}


/**
 * markup_insert - XXX
 */
bool markup_insert(struct PagedTextMarkupArray *ptma, const char *text, int position, int first, int bytes)
{
  if (!ptma || !text)
    return false;

  mutt_debug(LL_DEBUG1, "insert: pos %d, '%s' %d bytes\n", position, text, bytes);
  mutt_debug(LL_DEBUG1, "\n");

  int total_pos = 0;

  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, ptma)
  {
    mutt_debug(LL_DEBUG1, "piece %d: %d - %d\n", ARRAY_FOREACH_IDX_ptm, total_pos, total_pos + ptm->bytes - 1);

    if ((position >= total_pos) && (position < (total_pos + ptm->bytes)))
    {
      int start = position - total_pos;
      mutt_debug(LL_DEBUG1, "    start = %d\n", start);

      if (start == 0)
      {
        struct PagedTextMarkup ptm_new = { 0 };
        ptm_new.first = first;
        ptm_new.bytes = bytes;

        ARRAY_INSERT(ptma, ARRAY_FOREACH_IDX_ptm, &ptm_new);
      }
      else
      {
        struct PagedTextMarkup ptm_start = *ptm;
        struct PagedTextMarkup ptm_new = { 0 };
        ptm_new.first = first;
        ptm_new.bytes = bytes;

        ptm->first += start;
        ptm->bytes -= start;

        // ptm_start.first
        ptm_start.bytes = start;

        ARRAY_INSERT(ptma, ARRAY_FOREACH_IDX_ptm, &ptm_new);
        ARRAY_INSERT(ptma, ARRAY_FOREACH_IDX_ptm, &ptm_start);
      }

      return true;
    }

    total_pos += ptm->bytes;
  }

  struct PagedTextMarkup ptm_new = { 0 };
  ptm_new.first = first;
  ptm_new.bytes = bytes;

  ARRAY_ADD(ptma, ptm_new);

  return true;
}

/**
 * markup_delete - XXX
 */
bool markup_delete(struct PagedTextMarkupArray *ptma, int position, int bytes)
{
  if (!ptma)
    return false;
  if (bytes == 0)
    return true;

  mutt_debug(LL_DEBUG1, "delete: pos %d, %d bytes\n", position, bytes);
  mutt_debug(LL_DEBUG1, "\n");

  int total_pos = 0;

  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, ptma)
  {
    mutt_debug(LL_DEBUG1, "piece %d: %d - %d\n", ARRAY_FOREACH_IDX_ptm, total_pos, total_pos + ptm->bytes - 1);

    int start = -1;
    int last = -1;

    if ((position < total_pos) && ((position + bytes - 1) > total_pos))
    {
      start = 0;
    }
    else if ((position >= total_pos) && (position < (total_pos + ptm->bytes)))
    {
      start = position - total_pos;
    }

    if (((position + bytes - 1) >= total_pos) && ((position + bytes - 1) < (total_pos + ptm->bytes)))
    {
      last = position + bytes - 1 - total_pos;
    }
    else if ((start != -1) && (position + bytes) >= total_pos)
    {
      last = ptm->bytes - 1;
    }

    total_pos += ptm->bytes;

    if ((start == -1) && (last == -1))
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;32mignore\033[0m\n");
      continue;
    }
    else if ((start == 0) && (last == (ptm->bytes - 1)))
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;36mkill\033[0m\n");
      ptm->first = -1;
      ptm->bytes = -1;
    }
    else if ((start > 0) && (last < (ptm->bytes - 1)))
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;35msplit\033[0m\n");

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first;
      ptm_new.bytes = start;

      ptm->first += (last + 1);
      ptm->bytes -= (last + 1);

      ARRAY_INSERT(ptma, ARRAY_FOREACH_IDX_ptm, &ptm_new);
    }
    else if (start <= 0)
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;31mstart = %d\033[0m\n", start);
      mutt_debug(LL_DEBUG1, "\t\033[1;7;33mlast = %d\033[0m\n", last);

      int remainder = last + 1;
      ptm->first += remainder;
      ptm->bytes -= remainder;
    }
    else
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;31mstart = %d\033[0m\n", start);
      mutt_debug(LL_DEBUG1, "\t\033[1;7;33mlast = %d\033[0m\n", last);

      int remainder = last - start + 1;
      ptm->bytes -= remainder;
    }
  }

  int i = 0;
  while (i < ARRAY_SIZE(ptma))
  {
    ptm = ARRAY_GET(ptma, i);
    if (!ptm)
      break;

    if (ptm->first == -1)
      ARRAY_REMOVE(ptma, ptm);
    else
      i++;
  }

  mutt_debug(LL_DEBUG1, "\n");
  return true;
}

/**
 * markup_apply - XXX
 */
bool markup_apply(struct PagedTextMarkupArray *ptma, int position, int bytes, int cid, const struct AttrColor *ac)
{
  if (!ptma)
    return false;

  mutt_debug(LL_DEBUG1, "markup: pos %d, %d bytes\n", position, bytes);
  mutt_debug(LL_DEBUG1, "\n");

  int total_pos = 0;

  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, ptma)
  {
    mutt_debug(LL_DEBUG1, "piece %d: %d - %d\n", ARRAY_FOREACH_IDX_ptm, total_pos, total_pos + ptm->bytes - 1);

    int start = -1;
    int last = -1;

    if ((position < total_pos) && ((position + bytes - 1) > total_pos))
    {
      start = 0;
    }
    else if ((position >= total_pos) && (position < (total_pos + ptm->bytes)))
    {
      start = position - total_pos;
    }

    if (((position + bytes - 1) >= total_pos) && ((position + bytes - 1) < (total_pos + ptm->bytes)))
    {
      last = position + bytes - 1 - total_pos;
    }
    else if ((start != -1) && (position + bytes) >= total_pos)
    {
      last = ptm->bytes - 1;
    }

    total_pos += ptm->bytes;

    if ((start == -1) && (last == -1))
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;32mignore\033[0m\n");
      continue;
    }
    else if ((start == 0) && (last == (ptm->bytes - 1)))
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;36mentire\033[0m\n");
      ptm->cid = cid;
      ptm->ac_text = ac;
    }
    else if ((start > 0) && (last < (ptm->bytes - 1)))
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;35msplit\033[0m\n");

      struct PagedTextMarkup ptm_start = { 0 };
      ptm_start.first = ptm->first;
      ptm_start.bytes = start;

      ptm->first += (last + 1);
      ptm->bytes -= (last + 1);

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first + start - last - 1;
      ptm_new.bytes = bytes;
      ptm_new.cid = cid;
      ptm_new.ac_text = ac;

      ARRAY_INSERT(ptma, ARRAY_FOREACH_IDX_ptm, &ptm_new);
      ARRAY_INSERT(ptma, ARRAY_FOREACH_IDX_ptm, &ptm_start);
      ARRAY_FOREACH_IDX_ptm += 2;
    }
    else if (start <= 0)
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;31mstart = %d\033[0m\n", start);
      mutt_debug(LL_DEBUG1, "\t\033[1;7;33mlast = %d\033[0m\n", last);

      int remainder = last + 1;

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first;
      ptm_new.bytes = remainder;
      ptm_new.cid = cid;
      ptm_new.ac_text = ac;

      ptm->first += remainder;
      ptm->bytes -= remainder;

      ARRAY_INSERT(ptma, ARRAY_FOREACH_IDX_ptm, &ptm_new);
      ARRAY_FOREACH_IDX_ptm++;
    }
    else
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;31mstart = %d\033[0m\n", start);
      mutt_debug(LL_DEBUG1, "\t\033[1;7;33mlast = %d\033[0m\n", last);

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first;
      ptm_new.bytes = start;

      ptm->first += start;
      ptm->bytes -= start;
      ptm->cid = cid;
      ptm_new.ac_text = ac;

      ARRAY_INSERT(ptma, ARRAY_FOREACH_IDX_ptm, &ptm_new);
      ARRAY_FOREACH_IDX_ptm++;
    }
  }

  mutt_debug(LL_DEBUG1, "\n");
  return true;
}


void markup_copy_region(const struct PagedTextMarkupArray *src, int first, int bytes, struct PagedTextMarkupArray *dst)
{
  mutt_debug(LL_DEBUG1, "markup: pos %d, %d bytes\n", first, bytes);
  mutt_debug(LL_DEBUG1, "\n");

  int total_pos = 0;

  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, src)
  {
    mutt_debug(LL_DEBUG1, "piece %d: %d - %d\n", ARRAY_FOREACH_IDX_ptm, total_pos, total_pos + ptm->bytes - 1);

    int start = -1;
    int last = -1;

    if ((first < total_pos) && ((first + bytes - 1) > total_pos))
    {
      start = 0;
    }
    else if ((first >= total_pos) && (first < (total_pos + ptm->bytes)))
    {
      start = first - total_pos;
    }

    if (((first + bytes - 1) >= total_pos) && ((first + bytes - 1) < (total_pos + ptm->bytes)))
    {
      last = first + bytes - 1 - total_pos;
    }
    else if ((start != -1) && (first + bytes) >= total_pos)
    {
      last = ptm->bytes - 1;
    }

    total_pos += ptm->bytes;

    if ((start == -1) && (last == -1))
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;32mignore\033[0m\n");
      continue;
    }
    else if ((start == 0) && (last == (ptm->bytes - 1)))
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;36mentire\033[0m\n");
      // Copy the entire Markup
      ARRAY_ADD(dst, *ptm);
    }
    else if ((start > 0) && (last < (ptm->bytes - 1)))
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;35msplit\033[0m\n");

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first + start;
      ptm_new.bytes = bytes - last - 1;
      ptm_new.cid = ptm->cid;
      ptm_new.ac_text = ptm->ac_text;
      ptm_new.source = ptm->source;

      ARRAY_ADD(dst, ptm_new);
    }
    else if (start <= 0)
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;31mstart = %d\033[0m\n", start);
      mutt_debug(LL_DEBUG1, "\t\033[1;7;33mlast = %d\033[0m\n", last);

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first;
      ptm_new.bytes = last + 1;
      ptm_new.cid = ptm->cid;
      ptm_new.ac_text = ptm->ac_text;
      ptm_new.source = ptm->source;

      ARRAY_ADD(dst, ptm_new);
    }
    else
    {
      mutt_debug(LL_DEBUG1, "\t\033[1;7;31mstart = %d\033[0m\n", start);
      mutt_debug(LL_DEBUG1, "\t\033[1;7;33mlast = %d\033[0m\n", last);

      struct PagedTextMarkup ptm_new = { 0 };
      ptm_new.first = ptm->first;
      ptm_new.bytes = start;
      ptm_new.cid = ptm->cid;
      ptm_new.ac_text = ptm->ac_text;
      ptm_new.source = ptm->source;

      ARRAY_ADD(dst, ptm_new);
    }
  }

  mutt_debug(LL_DEBUG1, "\n");
}

