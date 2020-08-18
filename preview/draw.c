#include "config.h"
#include "private.h"

#include "mutt/buffer.h"
#include "mutt/logging.h"
#include "mutt/queue.h"
#include "mutt/string2.h"
#include "email/lib.h"
#include "gui/color.h"
#include "gui/curs_lib.h"
#include "gui/lib.h"
#include "gui/mutt_window.h"
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

  mutt_window_move(win, 0, 0);

  const chtype ch_default_separator = C_AsciiChars ? '-' : ACS_HLINE;
  bool use_default_separator = !valid_sep(C_PreviewDividerCharH);
  int sep_size = use_default_separator ? 1 : mutt_strwidth(C_PreviewDividerCharH);

  if (sep_size > *max_cols)
  {
    return;
  }

  mutt_curses_set_color(MT_COLOR_PREVIEW_DIVIDER);

  for (int i = 0; i < *max_cols; i += sep_size)
  {
    if (use_default_separator)
    {
      mutt_window_addch(ch_default_separator);
    }
    else
    {
      mutt_window_addstr(C_PreviewDividerCharH);
    }
  }

  for (int i = 1; i < *max_rows; ++i)
  {
    mutt_window_move(win, 0, i);
    mutt_window_clrtoeol(win);
  }

  *row_offset += 1;
  *max_rows -= 1;
}

void preview_draw(struct MuttWindow *win)
{
  if (!C_PreviewEnabled || !win)
    return;

  if (!mutt_window_is_visible(win))
    return;

  struct PreviewWindowData *data = preview_wdata_get(win);

  int col = 0, row = 0;
  int num_rows = win->state.rows;
  int num_cols = win->state.cols;

  draw_divider(data, win, &row, &num_rows, &col, &num_cols);

  if (data->current_email == NULL)
  {
    mutt_window_mvprintw(win, col, row, _("No email selected"));
    return;
  }

  struct Email *em = data->current_email;

  struct Address *from = TAILQ_FIRST(&em->env->from);
  mutt_window_mvprintw(win, 5, 5, "Selected mail id: %s, from: %s, subject: %s",
                       em->env->message_id, from ? from->mailbox : "unknown",
                       em->env->subject);

  mutt_window_mvprintw(win, 5, 8, "%.*s", mutt_buffer_len(&data->buffer),
                       mutt_buffer_string(&data->buffer));
}
