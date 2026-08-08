/**
 * @file
 * Run external commands in the background
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
 * @page main_background Run external commands in the background
 *
 * Run external commands in the background
 */

#include "config.h"
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "background.h"
#include "browser/lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pager/lib.h"

/// Maximum number of jobs kept at once; oldest finished slot is recycled
#define MAX_JOBS 10

/**
 * struct BackgroundJob - A command running in the background
 */
struct BackgroundJob
{
  pid_t pid;           ///< Process id
  bool running;        ///< true if the process is still running
  int exit_code;       ///< Exit code, -1 if still running or killed by a signal
  struct Buffer *cmd;  ///< Command line
  struct Buffer *file; ///< Temp file with captured output
};

static struct BackgroundJob Jobs[MAX_JOBS] = { 0 };
static struct Buffer *LastCommand = NULL;

// -----------------------------------------------------------------------------

/**
 * job_slot_free - Release a job slot
 * @param job Job to release
 */
static void job_slot_free(struct BackgroundJob *job)
{
  if (!job->cmd)
    return;

  if (job->running)
    kill(job->pid, SIGTERM);
  unlink(buf_string(job->file));
  buf_free(&job->cmd);
  buf_free(&job->file);
  *job = (struct BackgroundJob) { 0 };
}

/**
 * bg_view_output - Show a job's captured output in the pager
 * @param job Job to display
 */
static void bg_view_output(const struct BackgroundJob *job)
{
  struct stat st = { 0 };
  if ((stat(buf_string(job->file), &st) != 0) || (st.st_size == 0))
  {
    mutt_message(_("No output from background command"));
    return;
  }

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(job->file);

  pview.banner = buf_string(job->cmd);
  pview.flags = MUTT_PAGER_LOGS | MUTT_PAGER_BOTTOM;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
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

/// Help Bar for the Background Commands dialog
static const struct Mapping BgHelp[] = {
  // clang-format off
  { N_("Exit"),   OP_EXIT },
  { N_("Help"),   OP_HELP },
  { N_("Select"), OP_GENERIC_SELECT_ENTRY },
  { NULL, 0 },
  // clang-format on
};

/**
 * dlg_output - List the background commands and view their output - @ingroup gui_dlg
 */
void dlg_output(void)
{
  // Map menu rows to job slots (the slots may not be contiguous)
  int order[MAX_JOBS];
  int n_jobs = 0;
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (Jobs[i].cmd)
      order[n_jobs++] = i;
  }

  if (n_jobs == 0)
  {
    mutt_message(_("No background commands"));
    return;
  }

  const struct MenuDefinition *md_generic = generic_get_menu_definition();
  struct SimpleDialogWindows sdw = simple_dialog_new(md_generic, WT_DLG_BACKGROUND, BgHelp);

  struct Menu *menu = sdw.menu;
  menu->mdata = order; // Menu doesn't own the data
  menu->mdata_free = NULL;
  menu->make_entry = bg_make_entry;
  menu->max = n_jobs;
  menu->show_indicator = true;

  sbar_set_title(sdw.sbar, _("Background Commands"));

  struct MuttWindow *old_focus = window_set_focus(menu->win);
  // ---------------------------------------------------------------------------
  // Event Loop
  int choice = -1;
  struct KeyEvent event = { 0, OP_NULL };
  int op = OP_NULL;
  do
  {
    // Update the status of running jobs
    menu->redraw = MENU_REDRAW_FULL;
    menu->win->actions |= WA_REPAINT;

    window_redraw(NULL);

    event = km_dokey(md_generic, GETCH_NONE);
    op = event.op;

    if (op == OP_TIMEOUT)
      continue;

    if (op == OP_GENERIC_SELECT_ENTRY)
    {
      choice = menu->current;
      break;
    }

    if ((op == OP_EXIT) || (op == OP_QUIT) || (op == OP_ABORT))
      break;

    (void) menu_function_dispatcher(menu->win, &event);
  } while (true);
  // ---------------------------------------------------------------------------

  window_set_focus(old_focus);
  simple_dialog_free(&sdw.dlg);

  if (choice >= 0)
    bg_view_output(&Jobs[order[choice]]);
}

// -----------------------------------------------------------------------------

/**
 * bg_reap - Poll for finished jobs
 * @retval num Number of jobs that finished
 */
int bg_reap(void)
{
  int reaped = 0;
  int status = 0;
  for (int i = 0; i < MAX_JOBS; i++)
  {
    struct BackgroundJob *job = &Jobs[i];
    if (!job->running)
      continue;

    if (waitpid(job->pid, &status, WNOHANG) == 0)
      continue;

    job->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    job->running = false;
    mutt_message(_("Background command \"%s\" finished (exit %d)"),
                 buf_string(job->cmd), job->exit_code);
    reaped++;
  }

  return reaped;
}

/**
 * bg_timeout_observer - Reap finished jobs - Implements ::observer_t - @ingroup observer_api
 */
static int bg_timeout_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_TIMEOUT)
    return 0;

  bg_reap();
  return 0;
}

/**
 * bg_wait - Wait for all background commands to finish
 *
 * Blocks until every running job has finished, polling the 1s timeout tick.
 * Keys pressed meanwhile are queued and processed afterwards.
 * A macro continues with its remaining keys once the wait ends.
 * Ctrl-G aborts the wait and the rest of the macro.
 */
void bg_wait(void)
{
  while (true)
  {
    bg_reap();
    bool any = false;
    for (int i = 0; i < MAX_JOBS; i++)
    {
      if (Jobs[i].running)
      {
        any = true;
        break;
      }
    }
    if (!any)
      return;

    // GETCH_IGNORE_MACRO: don't consume the rest of a running macro
    const struct KeyEvent event = mutt_getch(GETCH_IGNORE_MACRO);
    if (event.op == OP_TIMEOUT)
    {
      window_redraw(NULL);
      continue;
    }
    if (event.op == OP_ABORT) // Ctrl-G: stop waiting and drop the rest of the macro
    {
      mutt_flush_macro_to_endcond();
      return;
    }
    if (event.op < OP_NULL) // repaint etc.
      continue;

    // Queue the key so it's processed after the wait: insert at the front,
    // the macro buffer pops from the back
    mutt_push_macro_event_first(event.ch, (event.op > OP_NULL) ? event.op : OP_NULL);
  }
}

/**
 * bg_job_start - Run a command in the background
 * @param cmd Command to run
 * @retval num Slot number of the new job
 * @retval -1  Error
 */
int bg_job_start(const char *cmd)
{
  // Prefer an unused slot; recycle the first finished one otherwise
  struct BackgroundJob *job = NULL;
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (!Jobs[i].cmd) // never used slot
    {
      job = &Jobs[i];
      break;
    }
    if (!job && !Jobs[i].running) // finished slot
      job = &Jobs[i];
  }
  if (!job)
  {
    mutt_error(_("Too many background commands"));
    return -1;
  }
  if (job->cmd) // recycling a finished job
    job_slot_free(job);

  struct Buffer *file = buf_pool_get();
  buf_mktemp(file);

  FILE *fp = mutt_file_fopen(buf_string(file), "a");
  if (!fp)
  {
    mutt_perror("fopen");
    goto fail;
  }

  const int fdn = open("/dev/null", O_RDONLY);
  const int fdout = fileno(fp);
  const int fderr = dup(fdout);
  if ((fdn < 0) || (fderr < 0))
  {
    if (fdn >= 0)
      close(fdn);
    if (fderr >= 0)
      close(fderr);
    mutt_file_fclose(&fp);
    mutt_perror("open");
    goto fail;
  }

  pid_t pid = filter_create_fd(cmd, NULL, NULL, NULL, fdn, fdout, fderr, NeoMutt->env);
  mutt_sig_unblock_system(true);
  close(fdn);
  mutt_file_fclose(&fp);

  if (pid < 0)
  {
    mutt_error(_("Error starting background command"));
    goto fail;
  }

  job->pid = pid;
  job->running = true;
  job->exit_code = -1;
  job->cmd = buf_new(cmd);
  job->file = buf_new(buf_string(file));
  buf_pool_release(&file);

  return job - Jobs;

fail:
  unlink(buf_string(file));
  buf_pool_release(&file);
  return -1;
}

/**
 * bg_job_exit_code - Get a job's exit code
 * @param slot Slot number
 * @retval -1  Job still running, killed by a signal, or invalid slot
 * @retval num Exit code of a finished job
 */
int bg_job_exit_code(int slot)
{
  if ((slot < 0) || (slot >= MAX_JOBS))
    return -1;
  return Jobs[slot].exit_code;
}

/**
 * bg_start_command - Prompt for a command and run it in the background
 */
void bg_start_command(void)
{
  struct Buffer *cmd = buf_pool_get();

  if (mw_get_field(_("Background command: "), cmd, MUTT_COMP_NONE,
                   HC_EXT_COMMAND, &CompleteFileOps, NULL) != 0)
  {
    goto done;
  }

  if (buf_is_empty(cmd)) // re-run the last command
  {
    if (!LastCommand)
      goto done;
    buf_copy(cmd, LastCommand);
  }

  if (bg_job_start(buf_string(cmd)) < 0)
    goto done;

  if (!LastCommand)
    LastCommand = buf_new("");
  buf_copy(LastCommand, cmd);

  mutt_message(_("Background command started: %s"), buf_string(cmd));

done:
  buf_pool_release(&cmd);
}

/**
 * bg_init - Initialise the background command handling
 */
void bg_init(void)
{
  notify_observer_add(NeoMutt->notify_timeout, NT_TIMEOUT, bg_timeout_observer, NULL);
}

/**
 * bg_cleanup - Kill running jobs and free their resources
 */
void bg_cleanup(void)
{
  for (int i = 0; i < MAX_JOBS; i++)
    job_slot_free(&Jobs[i]);
  buf_free(&LastCommand);
}
