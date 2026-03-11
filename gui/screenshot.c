/**
 * @file
 * Screenshot Capture
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page gui_screenshot Screenshot Capture
 *
 * Capture the terminal screen to an HTML file, conditional on
 * './configure --debug-screenshot'
 */

#include "config.h"
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "mutt/mbyte.h"
#include "screenshot.h"
#include "color/lib.h"

/**
 * struct ScreenshotCell - A single cell in the screenshot grid
 */
struct ScreenshotCell
{
  wchar_t ch;   ///< Character stored in this cell
  int style_id; ///< Index into ScreenshotState.style_classes
  bool cont;    ///< true if this cell is a continuation of a wide character
};

/**
 * struct ScreenshotState - State for an in-progress screenshot capture
 */
struct ScreenshotState
{
  int rows;                     ///< Number of rows in the grid
  int cols;                     ///< Number of columns in the grid
  int cur_row;                  ///< Current cursor row
  int cur_col;                  ///< Current cursor column
  int cur_style_id;             ///< Current style index
  struct ScreenshotCell *cells; ///< Grid of cells (rows * cols)
  struct HashTable *styles;     ///< Map of class-string to style id
  struct StringArray style_classes; ///< Array of CSS class strings
};

/// Currently active screenshot capture, or NULL
struct ScreenshotState *ScreenshotActive = NULL;

/**
 * screenshot_free_styles - Free all style data
 * @param ss Screenshot state
 */
static void screenshot_free_styles(struct ScreenshotState *ss)
{
  string_array_clear(&ss->style_classes);
  mutt_hash_free(&ss->styles);
}

/**
 * screenshot_style_add - Register a new CSS class string
 * @param ss      Screenshot state
 * @param classes Space-separated CSS class names
 * @retval num Style id for the new entry
 */
static int screenshot_style_add(struct ScreenshotState *ss, const char *classes)
{
  // Assert class names only contain characters safe for an HTML attribute
  for (const char *p = classes; *p; p++)
  {
    ASSERT(isalnum((unsigned char) *p) || (*p == ' ') || (*p == '-') || (*p == '_'));
  }

  const char *dup = mutt_str_dup(classes);
  ARRAY_ADD(&ss->style_classes, dup);
  int id = ARRAY_SIZE(&ss->style_classes) - 1;

  // Store (id + 1) so that style id 0 is distinguishable from "not found" (NULL)
  mutt_hash_insert(ss->styles, classes, (void *) (intptr_t) (id + 1));
  return id;
}

/**
 * screenshot_style_id_for_stack - Look up or create a style id for a ColorStack
 * @param ss Screenshot state
 * @param cs ColorStack to look up
 * @retval num Style id
 */
static int screenshot_style_id_for_stack(struct ScreenshotState *ss,
                                         const struct ColorStack *cs)
{
  if (!cs || (cs->len <= 0))
    return 0;

  struct Buffer *buf = buf_pool_get();
  struct Buffer *name = buf_pool_get();
  for (int i = 0; i < cs->len; i++)
  {
    if (cs->ids[i] == MT_COLOR_NORMAL)
      continue;

    if (!buf_is_empty(buf))
      buf_addch(buf, ' ');
    buf_reset(name);
    get_colorid_name(cs->ids[i], name);
    buf_addstr(buf, buf_string(name));
  }

  const char *classes = buf_string(buf);
  void *found = mutt_hash_find(ss->styles, classes);
  int id = -1;
  if (found)
    id = (int) ((intptr_t) found - 1); // Undo the +1 offset from screenshot_style_add
  else
    id = screenshot_style_add(ss, classes);

  buf_pool_release(&name);
  buf_pool_release(&buf);
  return id;
}

/**
 * screenshot_reset_cells - Clear all cells to spaces
 * @param ss Screenshot state
 */
static void screenshot_reset_cells(struct ScreenshotState *ss)
{
  const int total = ss->rows * ss->cols;
  for (int i = 0; i < total; i++)
  {
    ss->cells[i].ch = L' ';
    ss->cells[i].style_id = 0;
    ss->cells[i].cont = false;
  }
}

/**
 * screenshot_set_cell - Set the contents of a single cell
 * @param ss       Screenshot state
 * @param row      Row in the grid
 * @param col      Column in the grid
 * @param ch       Character to store
 * @param style_id Style index
 * @param cont     true if this is a continuation of a wide character
 */
static void screenshot_set_cell(struct ScreenshotState *ss, int row, int col,
                                wchar_t ch, int style_id, bool cont)
{
  if ((row < 0) || (col < 0) || (row >= ss->rows) || (col >= ss->cols))
    return;

  const int idx = (row * ss->cols) + col;
  ss->cells[idx].ch = ch;
  ss->cells[idx].style_id = style_id;
  ss->cells[idx].cont = cont;
}

/**
 * screenshot_write_escaped - Write HTML-escaped text to a file
 * @param fp    File to write to
 * @param text  Text to escape
 * @param bytes Number of bytes to write
 */
static void screenshot_write_escaped(FILE *fp, const char *text, int bytes)
{
  for (int i = 0; i < bytes; i++)
  {
    switch (text[i])
    {
      case '&':
        fputs("&amp;", fp);
        break;
      case '<':
        fputs("&lt;", fp);
        break;
      case '>':
        fputs("&gt;", fp);
        break;
      default:
        fputc(text[i], fp);
        break;
    }
  }
}

/**
 * screenshot_new - Start a new screenshot capture
 * @param rows Number of terminal rows
 * @param cols Number of terminal columns
 * @retval ptr  New ScreenshotState
 * @retval NULL Error
 */
struct ScreenshotState *screenshot_new(int rows, int cols)
{
  if ((rows <= 0) || (cols <= 0))
    return NULL;
  if (rows > (INT_MAX / cols))
    return NULL;

  struct ScreenshotState *ss = MUTT_MEM_CALLOC(1, struct ScreenshotState);
  ss->rows = rows;
  ss->cols = cols;

  ss->styles = mutt_hash_new(101, MUTT_HASH_STRDUP_KEYS);
  screenshot_style_add(ss, "");

  size_t num_cells = (size_t) rows * (size_t) cols;
  ss->cells = MUTT_MEM_CALLOC(num_cells, struct ScreenshotCell);
  screenshot_reset_cells(ss);

  return ss;
}

/**
 * screenshot_end_and_write - Finish capture and write the HTML file
 * @param[in,out] pss  Pointer to ScreenshotState pointer (will be set to NULL)
 * @param[in]     path Output file path, or NULL to discard
 * @retval true  File was written successfully
 * @retval false File couldn't be opened, or no state
 */
bool screenshot_end_and_write(struct ScreenshotState **pss, const char *path)
{
  if (!pss || !*pss)
    return false;

  struct ScreenshotState *ss = *pss;

  bool written = false;
  FILE *fp = NULL;
  if (path)
    fp = mutt_file_fopen(path, "w");

  if (fp)
  {
    fputs("<div class=\"term-window\">\n", fp);
    fputs("<div class=\"term-title\">XXX</div>\n", fp);
    fputs("<pre class=\"terminal\" role=\"img\" aria-label=\"XXX\">\n", fp);
    for (int r = 0; r < ss->rows; r++)
    {
      int cur_style = -1;
      for (int c = 0; c < ss->cols; c++)
      {
        const struct ScreenshotCell *cell = &ss->cells[(r * ss->cols) + c];
        if (cell->cont)
          continue;

        if (cell->style_id != cur_style)
        {
          if (cur_style >= 0)
            fputs("</span>", fp);
          cur_style = cell->style_id;
          const char *classes = *ARRAY_GET(&ss->style_classes, cur_style);
          if (classes && *classes)
            fprintf(fp, "<span class=\"%s\">", classes);
          else
            fputs("<span>", fp);
        }

        char out[MB_LEN_MAX] = { 0 };
        mbstate_t state = { 0 };
        size_t len = wcrtomb(out, cell->ch, &state);
        if (len == (size_t) -1)
        {
          fputc('?', fp);
        }
        else
        {
          screenshot_write_escaped(fp, out, (int) len);
        }
      }
      if (cur_style >= 0)
        fputs("</span>", fp);
      fputc('\n', fp);
    }
    fputs("</pre>\n", fp);
    fputs("</div>\n", fp);
    mutt_file_fclose(&fp);
    written = true;
  }

  screenshot_free_styles(ss);
  FREE(&ss->cells);
  FREE(pss);
  return written;
}

/**
 * screenshot_set_color_stack - Set the current colour for subsequent writes
 * @param ss Screenshot state
 * @param cs ColorStack to record
 */
void screenshot_set_color_stack(struct ScreenshotState *ss, const struct ColorStack *cs)
{
  if (!ss)
    return;

  ss->cur_style_id = screenshot_style_id_for_stack(ss, cs);
}

/**
 * screenshot_move_cursor - Move the virtual cursor
 * @param ss  Screenshot state
 * @param row Row position
 * @param col Column position
 */
void screenshot_move_cursor(struct ScreenshotState *ss, int row, int col)
{
  if (!ss)
    return;

  if (row < 0)
    row = 0;
  if (col < 0)
    col = 0;
  if (row >= ss->rows)
    row = ss->rows - 1;
  if (col >= ss->cols)
    col = ss->cols - 1;

  ss->cur_row = row;
  ss->cur_col = col;
}

/**
 * screenshot_write_text - Write text at the current cursor position
 * @param ss    Screenshot state
 * @param str   String to write
 * @param bytes Number of bytes, or -1 for the whole string
 */
void screenshot_write_text(struct ScreenshotState *ss, const char *str, int bytes)
{
  if (!ss || !str)
    return;

  if (bytes < 0)
    bytes = (int) strlen(str);

  mbstate_t state = { 0 };
  const char *ptr = str;
  int remaining = bytes;

  while ((remaining > 0) && (ss->cur_row < ss->rows))
  {
    wchar_t wc = 0;
    size_t consumed = mbrtowc(&wc, ptr, remaining, &state);
    if ((consumed == (size_t) -1) || (consumed == (size_t) -2))
    {
      wc = L'?';
      consumed = 1;
      memset(&state, 0, sizeof(state));
    }
    else if (consumed == 0)
    {
      break;
    }

    int width = mutt_mb_wcwidth(wc);
    if (width <= 0)
      width = 1;

    if (ss->cur_col >= ss->cols)
    {
      ss->cur_row++;
      ss->cur_col = 0;
      if (ss->cur_row >= ss->rows)
        break;
    }

    if ((ss->cur_col + width) > ss->cols)
    {
      ss->cur_row++;
      ss->cur_col = 0;
      if (ss->cur_row >= ss->rows)
        break;
    }

    screenshot_set_cell(ss, ss->cur_row, ss->cur_col, wc, ss->cur_style_id, false);
    for (int i = 1; i < width; i++)
      screenshot_set_cell(ss, ss->cur_row, ss->cur_col + i, L' ', ss->cur_style_id, true);

    ss->cur_col += width;
    ptr += consumed;
    remaining -= (int) consumed;
  }
}
