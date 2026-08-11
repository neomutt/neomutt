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
#include <string.h>
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
#include "muttlib.h"
#include "pager/lib.h"

/// Maximum number of jobs kept at once; Clear frees the finished ones
#define MAX_JOBS 10

/// Maximum number of job outputs kept in memory
#define BG_OUTPUT_RING 10

/**
 * struct BackgroundJob - A command running in the background
 */
struct BackgroundJob
{
  pid_t pid;           ///< Process id
  bool running;        ///< true if the process is still running
  int exit_code;       ///< Exit code, -1 if still running or killed by a signal
  struct Buffer *cmd;  ///< Command line
  struct Buffer *file; ///< Temp file with captured output while running
  struct Buffer *out;  ///< Output kept in memory after completion
};

static struct BackgroundJob Jobs[MAX_JOBS] = { 0 };
static struct Buffer *LastCommand = NULL;

/// Finished jobs with output in memory, oldest first; evicted when full
ARRAY_HEAD(OutputRing, struct BackgroundJob *);
static struct OutputRing OutputRing = ARRAY_HEAD_INITIALIZER;

/// Events of a macro paused by bg_wait(), resumed when the jobs finish
static struct KeyEventArray Parked = ARRAY_HEAD_INITIALIZER;
static bool ParkedActive = false;

// -----------------------------------------------------------------------------

// clang-format off
/**
 * OpBackground - Functions for the Background Commands Dialog
 */
static const struct MenuFuncOp OpBackground[] = { /* map: background */
  { "background-clear",              OP_BACKGROUND_CLEAR },
  { NULL, 0 },
};

/**
 * BackgroundDefaultBindings - Key bindings for the Background Commands Dialog
 */
static const struct MenuOpSeq BackgroundDefaultBindings[] = { /* map: background */
  { OP_BACKGROUND_CLEAR,                    "x" },
  { OP_DELETE,                              "d" },
  { OP_SAVE,                                "s" },
  { 0, NULL },
};
// clang-format on

/**
 * background_init_keys - Initialise the Background Keybindings - Implements ::init_keys_api
 */
void background_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic)
{
  struct MenuDefinition *md = NULL;
  struct SubMenu *sm = NULL;

  sm = km_register_submenu(OpBackground);
  md = km_register_menu(MENU_BACKGROUND, "background");
  km_menu_add_submenu(md, sm);
  km_menu_add_submenu(md, sm_generic);
  km_menu_add_bindings(md, BackgroundDefaultBindings);
}

// -----------------------------------------------------------------------------

/**
 * output_ring_remove - Drop one output, shifting the rest
 * @param pos Position in the ring
 */
static void output_ring_remove(int pos)
{
  struct BackgroundJob **job = ARRAY_GET(&OutputRing, pos);
  buf_free(&(*job)->out);
  const int n = ARRAY_SIZE(&OutputRing) - pos - 1;
  if (n > 0)
    memmove(job, job + 1, n * sizeof(struct BackgroundJob *));
  ARRAY_SHRINK(&OutputRing, 1);
}

/**
 * output_ring_remove_job - Drop a job's output from the ring
 * @param job Job to remove
 */
static void output_ring_remove_job(struct BackgroundJob *job)
{
  for (size_t i = 0; i < ARRAY_SIZE(&OutputRing); i++)
  {
    if (*ARRAY_GET(&OutputRing, i) == job)
    {
      output_ring_remove(i);
      return;
    }
  }
}

/**
 * output_ring_add - Keep a finished job's output, evicting the oldest if needed
 * @param job Job whose output was just copied to memory
 */
static void output_ring_add(struct BackgroundJob *job)
{
  ARRAY_ADD(&OutputRing, job);
  if (ARRAY_SIZE(&OutputRing) > BG_OUTPUT_RING)
    output_ring_remove(0);
}

/**
 * output_ring_clear - Drop all outputs
 */
static void output_ring_clear(void)
{
  struct BackgroundJob **job = NULL;
  ARRAY_FOREACH(job, &OutputRing)
  {
    buf_free(&(*job)->out);
  }
  ARRAY_FREE(&OutputRing);
}

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
  else
    output_ring_remove_job(job);
  unlink(buf_string(job->file));
  buf_free(&job->file);
  buf_free(&job->out);
  buf_free(&job->cmd);
  *job = (struct BackgroundJob) { 0 };
}

/**
 * bg_read_output - Read a job's captured output into memory
 * @param file Temp file with the output
 * @retval ptr Output, or NULL on error
 */
static struct Buffer *bg_read_output(const char *file)
{
  FILE *fp = mutt_file_fopen(file, "r");
  if (!fp)
    return NULL;

  struct Buffer *out = buf_new(NULL);
  char buf[2048] = { 0 };
  size_t l = 0;
  while ((l = fread(buf, 1, sizeof(buf), fp)) > 0)
    buf_addstr_n(out, buf, l);
  mutt_file_fclose(&fp);
  return out;
}

/**
 * job_has_output - Does a job have any captured output to show?
 * @param job Job to check
 */
static bool job_has_output(const struct BackgroundJob *job)
{
  if (job->out)
    return true;

  struct stat st = { 0 };
  return job->file && (stat(buf_string(job->file), &st) == 0) && (st.st_size > 0);
}

/**
 * bg_view_output - Show a job's captured output in the pager
 * @param job Job to display
 *
 * The pager unlinks the file it displays, so it always gets a disposable copy.
 */
static void bg_view_output(const struct BackgroundJob *job)
{
  if (!job_has_output(job))
  {
    mutt_message(_("No output from background command"));
    return;
  }

  struct Buffer *view = buf_pool_get();
  buf_mktemp(view);

  FILE *fp_out = mutt_file_fopen(buf_string(view), "w");
  if (!fp_out)
  {
    mutt_perror("fopen");
    buf_pool_release(&view);
    return;
  }

  if (job->out)
  {
    fwrite(buf_string(job->out), 1, buf_len(job->out), fp_out);
  }
  else
  {
    FILE *fp_in = mutt_file_fopen(buf_string(job->file), "r");
    if (fp_in)
    {
      mutt_file_copy_stream(fp_in, fp_out);
      mutt_file_fclose(&fp_in);
    }
  }
  mutt_file_fclose(&fp_out);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(view);

  pview.banner = buf_string(job->cmd);
  pview.flags = MUTT_PAGER_LOGS | MUTT_PAGER_BOTTOM;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  buf_pool_release(&view);
}

/**
 * bg_save_output - Save a job's captured output to a file
 * @param job Job to save
 */
static void bg_save_output(const struct BackgroundJob *job)
{
  if (!job_has_output(job))
  {
    mutt_message(_("No output from background command"));
    return;
  }

  struct Buffer *path = buf_pool_get();
  if ((mw_get_field(_("Save to file: "), path, MUTT_COMP_CLEAR, HC_FILE,
                    &CompleteFileOps, NULL) != 0) ||
      buf_is_empty(path))
  {
    goto done;
  }

  expand_path(path, false);

  FILE *fp_out = mutt_file_fopen(buf_string(path), "w");
  if (!fp_out)
  {
    mutt_perror("%s", buf_string(path));
    goto done;
  }

  if (job->out)
  {
    fwrite(buf_string(job->out), 1, buf_len(job->out), fp_out);
  }
  else
  {
    FILE *fp_in = mutt_file_fopen(buf_string(job->file), "r");
    if (fp_in)
    {
      mutt_file_copy_stream(fp_in, fp_out);
      mutt_file_fclose(&fp_in);
    }
  }
  mutt_file_fclose(&fp_out);
  mutt_message(_("Output saved to %s"), buf_string(path));

done:
  buf_pool_release(&path);
}

/**
 * bg_rows_build - Map the menu rows to job slots
 * @param[out] n_rows Number of rows
 * @retval ptr Array of slots, use FREE()
 */
static int *bg_rows_build(int *n_rows)
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
 * bg_rows_free - Free the row-to-slot mapping - Implements Menu::mdata_free() - @ingroup menu_mdata_free
 */
static void bg_rows_free(struct Menu *menu, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

/**
 * dlg_output - List the background commands and view their output - @ingroup gui_dlg
 */
void dlg_output(void)
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

    // The temp file is only needed while the job runs; keep the output in memory
    struct Buffer *out = bg_read_output(buf_string(job->file));
    unlink(buf_string(job->file));
    buf_free(&job->file);
    if (out && !buf_is_empty(out))
    {
      job->out = out;
      output_ring_add(job);
    }
    else
    {
      buf_free(&out);
    }

    mutt_message(_("Background command \"%s\" finished (exit %d)"),
                 buf_string(job->cmd), job->exit_code);
    reaped++;
  }

  return reaped;
}

/**
 * bg_jobs_running - Do any background jobs still run?
 * @retval true If any job is still running
 */
static bool bg_jobs_running(void)
{
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (Jobs[i].running)
      return true;
  }
  return false;
}

/**
 * bg_user_in_index - Is the user looking at the Index?
 * @retval true If the focused window is the Index
 *
 * A parked macro is only resumed in the Index.  Elsewhere (job dialog,
 * composer, field editor) its keys would be swallowed or typed into the
 * wrong window.
 */
static bool bg_user_in_index(void)
{
  struct MuttWindow *win = window_get_focus();
  while (win)
  {
    if (win->type == WT_DLG_INDEX)
      return true;
    win = win->parent;
  }
  return false;
}

/**
 * bg_timeout_observer - Reap finished jobs and resume a parked macro - Implements ::observer_t - @ingroup observer_api
 */
static int bg_timeout_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_TIMEOUT)
    return 0;

  bg_reap();

  // Resume a parked macro once all jobs have finished
  if (ParkedActive && !bg_jobs_running() && bg_user_in_index())
  {
    // Parked holds the events in processing order; push_first re-queues
    // them so they pop in the same order, after any current macro
    for (size_t i = 0; i < ARRAY_SIZE(&Parked); i++)
    {
      const struct KeyEvent event = *ARRAY_GET(&Parked, i);
      mutt_push_macro_event_first(event.ch, event.op);
    }
    ARRAY_FREE(&Parked);
    ParkedActive = false;
  }
  return 0;
}

/**
 * bg_wait - Wait for all background commands to finish
 *
 * The rest of the current macro is parked and resumes automatically once
 * every running job has finished (see bg_timeout_observer).  The UI keeps
 * working normally while the jobs run.
 */
void bg_wait(void)
{
  // Nothing to wait for: the macro continues
  if (!bg_jobs_running())
    return;

  // The op dispatch clears the message line; re-show the start notification
  mutt_message(_("Background command started: %s"), buf_string(LastCommand));

  // Park the rest of the macro; bg_timeout_observer resumes it
  mutt_take_macro_events(&Parked);
  ParkedActive = true;
}

/**
 * bg_job_start - Run a command in the background
 * @param cmd Command to run
 * @retval num Slot number of the new job
 * @retval -1  Error
 */
int bg_job_start(const char *cmd)
{
  struct BackgroundJob *job = NULL;
  for (int i = 0; i < MAX_JOBS; i++)
  {
    if (!Jobs[i].cmd)
    {
      job = &Jobs[i];
      break;
    }
  }
  if (!job)
  {
    mutt_error(_("Too many background commands"));
    return -1;
  }

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

  if (buf_is_empty(cmd))
    goto done;

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
  output_ring_clear();
  ARRAY_FREE(&Parked);
  buf_free(&LastCommand);
}
