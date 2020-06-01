/**
 * @file
 * Dump the details of the nested Windows
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page debug_window Dump the details of the nested Windows
 *
 * Dump the details of the nested Windows
 */

#include "config.h"
#include "mutt/lib.h"
#include "gui/lib.h"
#include "lib.h"

extern struct MuttWindow *RootWindow;

static const char *win_size(const struct MuttWindow *win)
{
  if (!win)
    return "???";

  switch (win->size)
  {
    case MUTT_WIN_SIZE_FIXED:
      return "FIX";
    case MUTT_WIN_SIZE_MAXIMISE:
      return "MAX";
    case MUTT_WIN_SIZE_MINIMISE:
      return "MIN";
  }

  return "???";
}

static const char *win_name(const struct MuttWindow *win)
{
  if (!win)
    return "UNKNOWN";

  switch (win->type)
  {
    case WT_ALL_DIALOGS:
      return "All Dialogs";
    case WT_CONTAINER:
      return "Container";
    case WT_CUSTOM:
      return "Custom";
    case WT_DLG_ALIAS:
      return "Alias Dialog";
    case WT_DLG_ATTACH:
      return "Attach Dialog";
    case WT_DLG_AUTOCRYPT:
      return "Autocrypt Dialog";
    case WT_DLG_BROWSER:
      return "Browser Dialog";
    case WT_DLG_CERTIFICATE:
      return "Certificate Dialog";
    case WT_DLG_COMPOSE:
      return "Compose Dialog";
    case WT_DLG_CRYPT_GPGME:
      return "Crypt-GPGME Dialog";
    case WT_DLG_DO_PAGER:
      return "Pager Dialog";
    case WT_DLG_HISTORY:
      return "History Dialog";
    case WT_DLG_INDEX:
      return "Index Dialog";
    case WT_DLG_PGP:
      return "Pgp Dialog";
    case WT_DLG_POSTPONE:
      return "Postpone Dialog";
    case WT_DLG_QUERY:
      return "Query Dialog";
    case WT_DLG_REMAILER:
      return "Remailer Dialog";
    case WT_DLG_SMIME:
      return "Smime Dialog";
    case WT_HELP_BAR:
      return "Help Bar";
    case WT_INDEX:
      return "Index";
    case WT_INDEX_BAR:
      return "Index Bar";
    case WT_MESSAGE:
      return "Message";
    case WT_PAGER:
      return "Pager";
    case WT_PAGER_BAR:
      return "Pager Bar";
    case WT_ROOT:
      return "Root Dialog";
    case WT_SIDEBAR:
      return "Sidebar";
    default:
      return "UNKNOWN";
  }
}

static void win_dump(struct MuttWindow *win, int indent)
{
  bool visible = mutt_window_is_visible(win);

  mutt_debug(LL_DEBUG1, "%*s%s[%d,%d] %s-%c \033[1;33m%s\033[0m (%d,%d)%s\n",
             indent, "", visible ? "" : "\033[1;30m", win->state.col_offset,
             win->state.row_offset, win_size(win),
             (win->orient == MUTT_WIN_ORIENT_VERTICAL) ? 'V' : 'H', win_name(win),
             win->state.cols, win->state.rows, visible ? "" : "\033[0m");

  struct MuttWindow *np = NULL;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    win_dump(np, indent + 4);
  }
}

static void win_serialise(struct MuttWindow *win, struct Buffer *buf)
{
  if (!mutt_window_is_visible(win))
    return;

  mutt_buffer_add_printf(buf, "<%s {%dx,%dy} [%dC,%dR]", win_size(win),
                         win->state.col_offset, win->state.row_offset,
                         win->state.cols, win->state.rows);
  struct MuttWindow *np = NULL;
  TAILQ_FOREACH(np, &win->children, entries)
  {
    win_serialise(np, buf);
  }
  mutt_buffer_addstr(buf, ">");
}

void debug_win_dump(void)
{
  mutt_debug(LL_DEBUG1, "\n");
  win_dump(RootWindow, 0);
  mutt_debug(LL_DEBUG1, "\n");
  struct Buffer buf = mutt_buffer_make(1024);
  win_serialise(RootWindow, &buf);
  mutt_debug(LL_DEBUG1, "%s\n", mutt_b2s(&buf));
  mutt_buffer_dealloc(&buf);
}
