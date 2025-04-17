/**
 * @file
 * Simple Pager Window
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
 * @page spager_win_spager Simple Pager Window
 *
 * Simple Pager Window
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "win_spager.h"
#include "color/lib.h"
#include "pfile/lib.h"
#include "search.h"
#include "wdata.h"
#include "win_observer.h"
#ifdef USE_DEBUG_WINDOW
#include "debug/lib.h"
#endif

/// Don't highlight anything beyond this column
const int MaxSyntaxColumns = 4096;

/**
 * vrow_wrap - Wrap the text of a Row
 * @param vr    ViewRow
 * @param width Width in screen columns
 * @param flags Control the wrapping
 */
void vrow_wrap(struct ViewRow *vr, int width, RowWrapFlags flags)
{
  if (!vr)
    return;

  vr->num_bytes = mutt_str_len(vr->text);
  vr->num_cols = mutt_strwidth(vr->text);

  ARRAY_FREE(&vr->segments);
  if (vr->num_cols <= width)
    return;

  mutt_debug(LL_DEBUG1, "Wrapping: %s\n", vr->text);
  mutt_debug(LL_DEBUG1, "%d bytes, %d cols\n", vr->num_bytes, vr->num_cols);
  mutt_debug(LL_DEBUG1, "WRAP: %d into %d\n", vr->num_cols, width);

  int total_bytes = 0;
  int total_cols = 0;

  size_t bytes = 0;
  size_t text_len = vr->num_bytes - 1; //QWQ workaround for infinite loop
  size_t cols = 0;

  while (total_bytes < text_len)
  {
    struct Segment seg = { total_bytes, total_cols };
    ARRAY_ADD(&vr->segments, seg);

    bytes = mutt_wstr_trunc(vr->text + total_bytes,
                            text_len - total_bytes, width, &cols);

    if ((total_bytes == 0) && (flags & RW_MARKERS))
      width--;

    total_bytes += bytes;
    total_cols += cols;
  }

  struct Segment *seg = NULL;
  ARRAY_FOREACH(seg, &vr->segments)
  {
    mutt_debug(LL_DEBUG1, "Segment %d: %d bytes, %d cols\n", ARRAY_FOREACH_IDX_seg, seg->offset_bytes, seg->offset_cols);
  }
}

/**
 * vrows_wrap - Wrap the text of an array of Rows
 * @param vra   Array of ViewRows
 * @param width Width in screen columns
 * @param flags Control the wrapping
 */
void vrows_wrap(struct ViewRowArray *vra, int width, RowWrapFlags flags)
{
  if (!vra)
    return;

  struct ViewRow **pvr = NULL;
  ARRAY_FOREACH(pvr, vra)
  {
    vrow_wrap(*pvr, width, flags);
  }
}


/**
 * get_vrow - XXX
 */
struct ViewRow *get_vrow(struct PagedFile *pf, int row)
{
  struct ViewRow *vr = MUTT_MEM_CALLOC(1, struct ViewRow);

  struct PagedRow *pr = ARRAY_GET(&pf->rows, row);
  if (!pr)
    return NULL;

  vr->text = paged_row_get_filtered(pr);

  vr->num_bytes = pr->num_bytes;
  vr->num_cols = pr->num_cols;

  paged_row_normalise2(pr, &vr->markup);

  return vr;
}

/**
 * fill_vrows - XXX
 */
void fill_vrows(struct SimplePagerWindowData *wdata)
{
  int wrap_width = 12; // wdata->page_cols; // 42;
  struct ViewRowArray *vrows = &wdata->vrows;
  struct PagedRowArray *rows = &wdata->paged_file->rows;

  int scr_row = 0;

  // We may be starting at any position in any Row
  int cur_row = wdata->top_row;
  int cur_col = wdata->top_col;
  int cur_off = wdata->top_off;

  for (; cur_row < ARRAY_SIZE(rows); cur_row++, cur_col = 0, cur_off = 0)
  {
    // Take a Row
    struct PagedRow *row = ARRAY_GET(rows, cur_row);
    const char *text = paged_row_get_filtered(row);

    struct PagedTextMarkupArray ptma = ARRAY_HEAD_INITIALIZER;
    paged_row_normalise2(row, &ptma);

    if (row->num_cols == 0)
    {
      mutt_debug(LL_DEBUG1, "ROW %2d:\n", scr_row);
      scr_row++;
      if (scr_row >= wdata->page_rows)
        goto done;
    }

    // Divide it into ViewRows
    while (cur_col < row->num_cols)
    {
      struct ViewRow **pvrow = ARRAY_GET(vrows, scr_row);
      if (pvrow) // Already processed this one
        break;

      if ((row->num_cols - cur_col) > wrap_width)
      {
        // Add a chunk of the Row
        mutt_debug(LL_DEBUG1, "ROW %2d: %.*s\n", scr_row, wrap_width, text + cur_off);
        struct ViewRow *vr = MUTT_MEM_CALLOC(1, struct ViewRow);
        ARRAY_INIT(&vr->markup);
        markup_copy_region(&ptma, cur_off, wrap_width, &vr->markup);
        vr->text = mutt_strn_dup(text + cur_off, wrap_width);
        vr->num_bytes = wrap_width;
        vr->num_cols = wrap_width; //XXX
        cur_col += wrap_width;
        cur_off += wrap_width; // XXX need to measure
        ARRAY_SET(vrows, scr_row, vr);
      }
      else
      {
        // Add the remainder of the Row
        mutt_debug(LL_DEBUG1, "ROW %2d: %s\n", scr_row, text + cur_off);
        struct ViewRow *vr = MUTT_MEM_CALLOC(1, struct ViewRow);
        ARRAY_INIT(&vr->markup);
        markup_copy_region(&ptma, cur_off, row->num_bytes - cur_off, &vr->markup);
        vr->text = mutt_str_dup(text + cur_off);
        vr->num_bytes = row->num_bytes - cur_off;
        vr->num_cols = row->num_cols - cur_off;
        cur_col += row->num_cols - cur_off;
        cur_off += row->num_bytes - cur_off;
        ARRAY_SET(vrows, scr_row, vr);
      }

      scr_row++;
      if (scr_row >= wdata->page_rows)
        goto done;
    }
  }

done:
  struct Buffer *buf = buf_pool_get();
  struct ViewRow **pvr = NULL;

  mutt_debug(LL_DEBUG1, "VRows:\n");
  ARRAY_FOREACH(pvr, &wdata->vrows)
  {
    struct ViewRow *vr = *pvr;

    buf_reset(buf);
    buf_add_printf(buf, "%d", ARRAY_FOREACH_IDX_pvr);
    buf_add_printf(buf, "\t'\033[1m%s\033[0m'", vr->text);
    mutt_debug(LL_DEBUG1, "%s\n", buf_string(buf));

    buf_reset(buf);
    // buf_add_printf(buf, "%d bytes, %d cols, ", vr->num_bytes, vr->num_cols);

    struct PagedTextMarkup *ptm = NULL;
    ARRAY_FOREACH(ptm, &vr->markup)
    {
      buf_add_printf(buf, "(%d+%d) ", ptm->first, ptm->bytes);
      // if (ptm->cid > 0)
      //   buf_add_printf(buf, "cid:%d ", ptm->cid);
      // if (ptm->ac_text)
      //   buf_add_printf(buf, "%p", (void *) ptm->ac_text);
    }
    mutt_debug(LL_DEBUG1, "\t%s\n", buf_string(buf));
  }

  buf_pool_release(&buf);
}

/**
 * win_spager_recalc - Recalculate the Simple Pager display - Implements MuttWindow::recalc() - @ingroup window_recalc
 *
 * Recalculate:
 * - Wrap rows
 * - Merge colours
 * - Save Window dimensions
 *
 * Wrapping:
 * - Measure
 * - Wrap
 *
 * Colours:
 * - Base:   MT_COLOR_NORMAL              }
 * - File:   PagedFile.ac_file            }-- Merge into ac_merged
 * - Row:    PagedRow.ac_row              }
 * - Markup: PagedTextMarkupArray text    }
 * - Search: PagedTextMarkupArray search
 */
static int win_spager_recalc(struct MuttWindow *win)
{
  if (!win || !win->wdata)
    return -1;

  struct SimplePagerWindowData *wdata = win->wdata;
  struct PagedFile *pf = wdata->paged_file;

  // RowWrapFlags rw_flags = RW_WRAP | RW_MARKERS;

  int wrap_width = win->state.cols;
  if (wdata->c_wrap > 0)
  {
    wrap_width = MIN(wrap_width, wdata->c_wrap);
  }
  else if (wdata->c_wrap < 0)
  {
    wrap_width = MAX(wrap_width + wdata->c_wrap, 10);
  }

  wdata->page_rows = win->state.rows;
  wdata->page_cols = win->state.cols;
  fill_vrows(wdata);

  // vrows_wrap(&wdata->vrows, wrap_width, rw_flags);

  if (!pf->ac_file)
    pf->ac_file = simple_color_get(MT_COLOR_NORMAL);

#if 0
  const struct AttrColor *ac_search = simple_color_get(MT_COLOR_SEARCH);

  for (int i = 0; i < ARRAY_SIZE(&pf->rows); i++)
  {
    struct PagedRow *pr = ARRAY_GET(&pf->rows, i);

    // if ((pr->cid > MT_COLOR_NONE) && !pr->ac_row)
    if (pr->cid > MT_COLOR_NONE)
      pr->ac_row = simple_color_get(pr->cid);

    // if (!pr->ac_merged)
    pr->ac_merged = merged_color_overlay(pf->ac_file, pr->ac_row);

    struct PagedTextMarkup *ptm = NULL;
    ARRAY_FOREACH(ptm, &pr->text)
    {
      // if ((ptm->cid > MT_COLOR_NONE) && !ptm->ac_text)
      if (ptm->cid > MT_COLOR_NONE)
        ptm->ac_text = simple_color_get(ptm->cid);

      // if (!ptm->ac_merged)
      ptm->ac_merged = merged_color_overlay(pr->ac_merged, ptm->ac_text);
    }

    ARRAY_FOREACH(ptm, &pr->search)
    {
      ptm->ac_text = ac_search;
    }
  }
#endif

  wdata->page_rows = win->state.rows;
  wdata->page_cols = win->state.cols;

  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * display_row2 - Display a row of text in the Simple Pager
 * @param win         Window to draw on
 * @param row         Row in the Window
 * @param text        Text to display
 * @param text_offset Start offset into the text
 * @param text_end    End offset into the text
 * @param pr          Row to display
 */
void display_row2(struct MuttWindow *win, int row, const char *text,
                         int text_offset, int text_end, struct PagedRow *pr)
{
  struct SimplePagerWindowData *wdata = win->wdata;
  struct PagedTextMarkupArray *ptma_text = &pr->text;
  struct PagedTextMarkupArray *ptma_search = &pr->search;

  int i_text = 0;
  int i_search = 0;

  struct PagedTextMarkup *ptm_text = ARRAY_GET(ptma_text, i_text);
  struct PagedTextMarkup *ptm_search = ARRAY_GET(ptma_search, i_search);

  const struct AttrColor *ac = NULL;
  int last = 0;
  int pos = text_offset;
  int col = pos;

  if (text_offset > 0)
  {
    col--;
    mutt_window_move(win, row, 0);
    mutt_curses_set_color_by_id(MT_COLOR_MARKERS);
    mutt_window_addch(win, '+');
#ifdef USE_DEBUG_WINDOW
    mutt_refresh();
#endif
  }

  while (text && (pos < text_end))
  {
    // Skip any text syntax that's behind us
    while (pos_after_text_markup(pos, ptm_text))
    {
      i_text++;
      ptm_text = ARRAY_GET(ptma_text, i_text);
    }

    if (wdata->search->show_search)
    {
      // Skip any search syntax that's behind us
      while (pos_after_text_markup(pos, ptm_search))
      {
        i_search++;
        ptm_search = ARRAY_GET(ptma_search, i_search);
      }
    }

    last = text_end;

    // Prevent slowdown for degenerate text
    if (pos > MaxSyntaxColumns)
      break;

    // The text is highlighted
    if (pos_in_text_markup(pos, ptm_text))
    {
      ac = ptm_text->ac_merged;
      last = ptm_text->first + ptm_text->bytes;
    }
    else if (ptm_text)
    {
      last = MIN(last, ptm_text->first);
      ac = pr->ac_merged;
    }
    last = MIN(last, text_end);

    if (wdata->search->show_search)
    {
      // Search highlighting takes priority
      if (pos_in_text_markup(pos, ptm_search))
      {
        ac = merged_color_overlay(ac, ptm_search->ac_text);
        last = MIN(last, ptm_search->first + ptm_search->bytes);
      }
      else if (ptm_search)
      {
        last = MIN(last, ptm_search->first);
      }
    }

    if (!ac)
      ac = simple_color_get(pr->cid);

    // Display the actual text
    // from:   pos..last
    mutt_window_move(win, row, pos - col);
    mutt_curses_set_color(ac);
    mutt_window_addnstr(win, text + pos, last - pos);
#ifdef USE_DEBUG_WINDOW
    mutt_refresh();
#endif
    pos = last;
  }

  mutt_window_move(win, row, pos - col);
  // mutt_curses_set_color(pr->ac_merged);
  mutt_curses_set_color(simple_color_get(MT_COLOR_NORMAL));
#ifndef USE_DEBUG_WINDOW
  mutt_window_clrtoeol(win);
#endif
}

/**
 * display_row3 - Display a row of text in the Simple Pager
 * @param win         Window to draw on
 * @param row         Row in the Window
 * @param text        Text to display
 * @param text_offset Start offset into the text
 * @param text_end    End offset into the text
 * @param pr          Row to display
 */
void display_row3(struct MuttWindow *win, int row, const char *text,
                         int text_offset, int text_end, struct ViewRow *vr)
{
  // struct SimplePagerWindowData *wdata = win->wdata;
  // struct PagedTextMarkupArray *ptma_text = &vr->markup;
  // struct PagedTextMarkupArray *ptma_search = &vr->search;

  mutt_window_move(win, row, 0);
#ifndef USE_DEBUG_WINDOW
  mutt_window_clrtoeol(win);
#endif

  if (vr && vr->text)
  {
    // mutt_curses_set_color(pr->ac_merged);
    mutt_curses_set_color(simple_color_get(MT_COLOR_NORMAL));
    mutt_window_addstr(win, vr->text);
  }
#if 0
  int i_text = 0;
  int i_search = 0;

  struct PagedTextMarkup *ptm_text = ARRAY_GET(ptma_text, i_text);
  struct PagedTextMarkup *ptm_search = ARRAY_GET(ptma_search, i_search);

  const struct AttrColor *ac = NULL;
  int last = 0;
  int pos = text_offset;
  int col = pos;

  if (text_offset > 0)
  {
    col--;
    mutt_window_move(win, row, 0);
    mutt_curses_set_color_by_id(MT_COLOR_MARKERS);
    mutt_window_addch(win, '+');
#ifdef USE_DEBUG_WINDOW
    mutt_refresh();
#endif
  }
#endif

#if 0
  while (vr->text && (pos < text_end))
  {
    // Skip any text syntax that's behind us
    while (pos_after_text_markup(pos, ptm_text))
    {
      i_text++;
      ptm_text = ARRAY_GET(ptma_text, i_text);
    }

    if (wdata->search->show_search)
    {
      // Skip any search syntax that's behind us
      while (pos_after_text_markup(pos, ptm_search))
      {
        i_search++;
        ptm_search = ARRAY_GET(ptma_search, i_search);
      }
    }

    last = text_end;

    // Prevent slowdown for degenerate text
    if (pos > MaxSyntaxColumns)
      break;

    // The text is highlighted
    if (pos_in_text_markup(pos, ptm_text))
    {
      ac = ptm_text->ac_merged;
      last = ptm_text->first + ptm_text->bytes;
    }
    else if (ptm_text)
    {
      last = MIN(last, ptm_text->first);
      ac = pr->ac_merged;
    }
    last = MIN(last, text_end);

    if (wdata->search->show_search)
    {
      // Search highlighting takes priority
      if (pos_in_text_markup(pos, ptm_search))
      {
        ac = merged_color_overlay(ac, ptm_search->ac_text);
        last = MIN(last, ptm_search->first + ptm_search->bytes);
      }
      else if (ptm_search)
      {
        last = MIN(last, ptm_search->first);
      }
    }

    if (!ac)
      ac = simple_color_get(pr->cid);

    // Display the actual text
    // from:   pos..last
    mutt_window_move(win, row, pos - col);
    mutt_curses_set_color(ac);
    mutt_window_addnstr(win, vr->text + pos, last - pos);
#ifdef USE_DEBUG_WINDOW
    mutt_refresh();
#endif
    pos = last;
  }

  mutt_window_move(win, row, pos - col);
  // mutt_curses_set_color(pr->ac_merged);
  mutt_curses_set_color(simple_color_get(MT_COLOR_NORMAL));
#ifndef USE_DEBUG_WINDOW
  mutt_window_clrtoeol(win);
#endif
#endif
}

/**
 * display_row4 - Display a row of text in the Simple Pager
 * @param win         Window to draw on
 * @param row         Row in the Window
 * @param pr          Row to display
 */
void display_row4(struct MuttWindow *win, int row, struct ViewRow *vr)
{
  // struct SimplePagerWindowData *wdata = win->wdata;
  // struct PagedTextMarkupArray *ptma_text = &vr->markup;
  // struct PagedTextMarkupArray *ptma_search = &vr->search;

  mutt_window_move(win, row, 0);
  if (!vr)
  {
    mutt_window_clrtoeol(win);
    return;
  }

#ifndef USE_DEBUG_WINDOW
  mutt_window_clrtoeol(win);
#endif

  struct PagedTextMarkup *ptm = NULL;
  ARRAY_FOREACH(ptm, &vr->markup)
  {
    // if ((ptm->cid > MT_COLOR_NONE) && !ptm->ac_text)
    if (ptm->cid > MT_COLOR_NONE)
      ptm->ac_text = simple_color_get(ptm->cid);

    // if (!ptm->ac_merged)
    //   ptm->ac_merged = merged_color_overlay(pr->ac_merged, ptm->ac_text);

    // mutt_window_move(win, row, pos - col);
    mutt_curses_set_color(ptm->ac_text);
    mutt_window_addnstr(win, vr->text + ptm->first, ptm->bytes);
#ifdef USE_DEBUG_WINDOW
    mutt_refresh();
#endif
  }

  mutt_window_clrtoeol(win);
}

/**
 * win_spager_repaint - Repaint the Simple Pager display - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int win_spager_repaint(struct MuttWindow *win)
{
  mutt_debug(LL_DEBUG1, "\033[1;33mrepaint\033[0m\n");
  struct SimplePagerWindowData *wdata = win->wdata;
  if (!wdata)
    return 0;

#ifdef USE_DEBUG_WINDOW
  window_invalidate_all();
  debug_win_blanket(win, MT_COLOR_INDICATOR, 'O');
  mutt_refresh();
#endif

  struct PagedFile *pf = wdata->paged_file;
  // FILE *fp = pf->source->fp;
  // rewind(fp);

  // const char *text = NULL;

  // struct PagedRowArray *pra = &wdata->paged_file->rows;

  // int pr_index = 0;
  // int seg_index = 0;

  for (int screen_row = 0; screen_row < win->state.rows; screen_row++)
  {
#if 0
    if (paged_rows_find_virtual_row(pra, screen_row + wdata->vrow, &pr_index, &seg_index))
    {
      struct PagedRow *pr = ARRAY_GET(pra, pr_index);
      struct Segment *seg = ARRAY_GET(&pr->segments, seg_index);

      // paged_file_apply_filters(pf, pr_index);

      // text = paged_row_get_virtual_text(pr, seg);

      int text_offset = 0;
      if (seg)
        text_offset = seg->offset_bytes;

      int text_end = pr->num_bytes;
      struct Segment *seg_next = ARRAY_GET(&pr->segments, seg_index + 1);
      if (seg_next)
        text_end = seg_next->offset_bytes;

      struct PagedRow pr_normal = { 0 };
      paged_row_normalise(pr, &pr_normal);
      const char *text2 = ""; //QWQ paged_row_get_plain(pr);
      display_row2(win, screen_row, text2, text_offset, text_end, &pr_normal);
      paged_row_clear(&pr_normal);

#ifdef USE_DEBUG_WINDOW
      mutt_refresh();
#endif
    }
#endif
    if (screen_row < ARRAY_SIZE(&wdata->vrows))
    {
      struct ViewRow **pvr = ARRAY_GET(&wdata->vrows, screen_row);
      struct ViewRow *vr = *pvr;

      display_row4(win, screen_row, vr);
    }
    else
    {
      mutt_window_move(win, screen_row, 0);
      mutt_curses_set_color(pf->ac_file);
      if (wdata->c_tilde)
        mutt_window_addstr(win, "~");
#ifndef USE_DEBUG_WINDOW
      mutt_window_clrtoeol(win);
#endif
    }
  }

  win->actions |= WA_RECALC;

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * spager_window_new - Create a new Simple Pager Window
 * @param pf  PagedFile to use
 * @param sub Config Subset
 * @retval ptr New Simple Pager Window
 */
struct MuttWindow *spager_window_new(struct PagedFile *pf, struct ConfigSubset *sub)
{
  struct MuttWindow *win = mutt_window_new(WT_PAGER, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);

  struct SimplePagerWindowData *wdata = spager_wdata_new();
  wdata->paged_file = pf;
  wdata->sub = sub;

  win->wdata = wdata;
  win->wdata_free = spager_wdata_free;

  win->recalc = win_spager_recalc;
  win->repaint = win_spager_repaint;

  win_spager_add_observers(win, sub);

  update_cached_config(wdata, NULL);

  return win;
}
