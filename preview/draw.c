#include "config.h"
#include "private.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt_globals.h"

static bool valid_sep(const char *wanted)
{
  if (!wanted)
    return false;
  int h_s = mutt_strwidth(wanted);
  if (h_s <= 0)
    return false;
  return true;
}

static void draw_divider(struct PreviewWindowData *data, struct MuttWindow *win,
                         int *row_offset, int *max_rows, int *col_offset, int *max_cols)
{
  if (!row_offset || !max_rows || !col_offset || !max_cols)
  {
    return;
  }

  mutt_window_move(win, *col_offset, *row_offset);

  const bool c_ascii_chars = cs_subset_bool(NeoMutt->sub, "ascii_chars");
  const chtype ch_default_separator = c_ascii_chars ? '-' : ACS_HLINE;
  const char *const c_preview_divider_char_h = cs_subset_string(NeoMutt->sub, PREVIEW_CONFIG_PREFIX "divider_horizontal");
  bool use_default_separator = !valid_sep(c_preview_divider_char_h);
  int sep_size = use_default_separator ? 1 : mutt_strwidth(c_preview_divider_char_h);

  if (sep_size > *max_cols)
  {
    return;
  }

  mutt_curses_set_color(MT_COLOR_PREVIEW_DIVIDER);

  for (int i = *col_offset; i < *max_cols; i += sep_size)
  {
    if (use_default_separator)
    {
      mutt_window_addch(ch_default_separator);
    }
    else
    {
      mutt_window_addstr(c_preview_divider_char_h);
    }
  }
  mutt_curses_set_color(MT_COLOR_NORMAL);

  for (int i = *row_offset + 1; i < *max_rows; ++i)
  {
    mutt_window_move(win, *col_offset, i);
    mutt_window_clrtoeol(win);
  }

  *row_offset += 1;
}

void preview_draw(struct MuttWindow *win)
{
  const bool c_preview_enabled = cs_subset_bool(NeoMutt->sub, PREVIEW_CONFIG_PREFIX "enabled");
  if (!c_preview_enabled || !win)
    return;

  if (!mutt_window_is_visible(win))
    return;

  struct PreviewWindowData *wdata = preview_wdata_get(win);

  int col = 0, row = 0;
  int num_rows = win->state.rows;
  int num_cols = win->state.cols;

  draw_divider(wdata, win, &row, &num_rows, &col, &num_cols);

  if (wdata->current_email == NULL)
  {
    mutt_window_mvprintw(win, col, row, _("No email selected"));
    return;
  }

  struct Email *em = wdata->current_email;
  struct Address *from = TAILQ_FIRST(&em->env->from);

  struct Buffer disp_buff;
  mutt_buffer_init(&disp_buff);

  col += 1; // offset col of 1, it is prettier.

  mutt_curses_set_color(MT_COLOR_PREVIEW_TEXT);
#define display(fmt, ...)                                                             \
  do                                                                                  \
  {                                                                                   \
    mutt_buffer_reset(&disp_buff);                                                    \
    mutt_buffer_printf(&disp_buff, _(fmt), __VA_ARGS__);                              \
    int max_byte = mutt_wstr_trunc(mutt_buffer_string(&disp_buff), mutt_buffer_len(&disp_buff), \
                                   num_cols - col, NULL);                             \
    mutt_window_mvprintw(win, col, row, "%.*s", max_byte, mutt_buffer_string(&disp_buff));      \
    ++row;                                                                            \
  } while (0)

  display("Mail from: %s <%s>", from ? from->personal : _("unknown"),
          from ? from->mailbox : _("unknown"));
  display("Subject: %s", em->env->subject);
  ++row;

  // We don't want the preview to be to close to the bottom (status bar)
  --num_rows;

  {
    const int max_line_size = num_cols - col;
    const char *data = mutt_buffer_string(&wdata->buffer);
    int data_size = mutt_buffer_len(&wdata->buffer);

    while (data_size && row < num_rows)
    {
      int max_byte = mutt_wstr_trunc(data, data_size, max_line_size, NULL);
      mutt_window_mvprintw(win, col, row, "%.*s", max_byte, data);
      data += max_byte;
      data_size -= max_byte;
      ++row;
    }
  }
  mutt_buffer_dealloc(&disp_buff);
  mutt_curses_set_color(MT_COLOR_NORMAL);
}
