/**
 * @file
 * Background commands dialog
 *
 * @authors
 * Copyright (C) 2026 Reza Jelveh
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
 * @page background_dlg_background Background commands dialog
 *
 * Background commands dialog
 */

#include "config.h"
#include <stdlib.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "private.h"

/// Help Bar for the Background Commands dialog
static const struct Mapping BgHelp[] = {
  // clang-format off
  { N_("Exit"),   OP_EXIT },
  { N_("Delete"), OP_DELETE },
  { N_("Save"),   OP_SAVE },
  { N_("Clear"),  OP_BACKGROUND_CLEAR },
  { N_("Help"),   OP_HELP },
  { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { NULL, 0 },
  // clang-format on
};

/**
 * bg_rows_build - Map the menu rows to job slots
 * @param[out] n_rows Number of rows
 * @retval ptr Array of slots, use FREE()
 */
int *bg_rows_build(int *n_rows)
{
  int *order = mutt_mem_calloc(MAX_JOBS, sizeof(int));
  int n = 0;
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (Jobs[i].cmd)
      order[n++] = i;
  }
  *n_rows = n;
  return order;
}

/**
 * bg_make_entry - Format a job for the menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 */
static int bg_make_entry(struct Menu *menu, int line, int max_cols, struct Buffer *buf)
{
  const int slot = ((const int *) menu->mdata)[line];
  const struct BackgroundJob *job = &Jobs[slot];

  const char *status = job->running ? _("run") : _("done");
  char exitbuf[16] = { 0 };
  if (!job->running)
  {
    if (job->exit_code < 0)
      mutt_str_copy(exitbuf, "sig", sizeof(exitbuf));
    else
      snprintf(exitbuf, sizeof(exitbuf), "%d", job->exit_code);
  }

  const int bytes = buf_printf(buf, "%2d  %-4s  %4s  %s", line + 1, status,
                               exitbuf, buf_string(job->cmd));

  return mutt_strnwidth(buf_string(buf), bytes);
}

/**
 * bg_rows_free - Free the row-to-slot mapping - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 */
static void bg_rows_free(struct Menu *menu, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

/**
 * dlg_background - List the background commands and view their output - @ingroup gui_dlg
 */
void dlg_background(void)
{
  int n_rows = 0;
  int *order = bg_rows_build(&n_rows);
  if (n_rows == 0)
  {
    FREE(&order);
    mutt_message(_("No background commands"));
    return;
  }

  struct MenuDefinition *md_background = menu_find(MENU_BACKGROUND);
  ASSERT(md_background);
  struct SimpleDialogWindows sdw = simple_dialog_new(md_background, WT_DLG_BACKGROUND, BgHelp);

  struct Menu *menu = sdw.menu;
  menu->mdata = order;
  menu->mdata_free = bg_rows_free;
  menu->make_entry = bg_make_entry;
  menu->max = n_rows;
  menu->show_indicator = true;

  sbar_set_title(sdw.sbar, _("Background Commands"));

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  struct KeyEvent event = { 0, OP_NULL };
  int op = OP_NULL;
  do
  {
    // Update the status of running jobs
    menu->redraw = MENU_REDRAW_FULL;
    menu->win->actions |= WA_REPAINT;

    window_redraw(NULL);

    event = km_dokey(md_background, GETCH_NONE);
    op = event.op;

    if (op == OP_TIMEOUT)
      continue;

    if ((op == OP_EXIT) || (op == OP_QUIT) || (op == OP_ABORT))
      break;

    struct BackgroundJob *job = &Jobs[order[menu->current]];

    if (op == OP_GENERIC_SELECT_ENTRY)
    {
      bg_view_output(job);
      continue;
    }

    if (op == OP_DELETE)
    {
      if (job->running)
        mutt_message(_("Job is still running"));
      else
        job_slot_free(job);
      goto rebuild;
    }

    if (op == OP_BACKGROUND_CLEAR)
    {
      for (int i = 0; i < MAX_JOBS; i++)
      {
        if (Jobs[i].cmd && !Jobs[i].running)
          job_slot_free(&Jobs[i]);
      }
      goto rebuild;
    }

    if (op == OP_SAVE)
    {
      bg_save_output(job);
      continue;
    }

    (void) menu_function_dispatcher(menu->win, &event);
    continue;

  rebuild:
    FREE(&menu->mdata);
    order = bg_rows_build(&n_rows);
    menu->mdata = order;
    menu->max = n_rows;
    if (menu->current >= n_rows)
      menu->current = n_rows - 1;
    if (n_rows == 0)
      break;
  } while (true);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  simple_dialog_free(&sdw.dlg);
}
