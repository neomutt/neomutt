/**
 * @file
 * Browser Auto-Completion
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page browser_complete Browser Auto-Completion
 *
 * Browser Auto-Completion
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "complete/lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "mutt_mailbox.h"
#include "muttlib.h"

/**
 * complete_file_mbox - Complete a Mailbox - Implements ::complete_function_t - @ingroup complete_api
 */
int complete_file_mbox(struct EnterWindowData *wdata, int op)
{
  if (!wdata)
    return FR_NO_ACTION;

  struct FileCompletionData *cdata = wdata->cdata;

  if (op == OP_EDITOR_MAILBOX_CYCLE)
  {
    wdata->first = true; /* clear input if user types a real key later */
    buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf, wdata->state->curpos);
    mutt_mailbox_next(cdata->mailbox, wdata->buffer);

    wdata->state->curpos = wdata->state->lastchar = mutt_mb_mbstowcs(
        &wdata->state->wbuf, &wdata->state->wbuflen, 0, buf_string(wdata->buffer));
    return FR_SUCCESS;
  }

  if ((op != OP_EDITOR_COMPLETE) && (op != OP_EDITOR_COMPLETE_QUERY))
    return FR_NO_ACTION;

  int rc = FR_SUCCESS;
  buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf, wdata->state->curpos);

  /* see if the path has changed from the last time */
  if ((!wdata->tempbuf && !wdata->state->lastchar) ||
      (wdata->tempbuf && (wdata->templen == wdata->state->lastchar) &&
       (memcmp(wdata->tempbuf, wdata->state->wbuf,
               wdata->state->lastchar * sizeof(wchar_t)) == 0)))
  {
    SelectFileFlags flags = MUTT_SEL_NO_FLAGS;
    if (wdata->hclass == HC_MAILBOX)
      flags |= MUTT_SEL_FOLDER;
    if (cdata->multiple)
      flags |= MUTT_SEL_MULTI;

    dlg_browser(wdata->buffer, flags, cdata->mailbox, cdata->files, cdata->numfiles);
    if (!buf_is_empty(wdata->buffer))
    {
      buf_pretty_mailbox(wdata->buffer);
      if (!wdata->pass)
        mutt_hist_add(wdata->hclass, buf_string(wdata->buffer), true);
      wdata->done = true;
      return FR_SUCCESS;
    }

    /* file selection cancelled */
    return FR_CONTINUE;
  }

  if (mutt_complete(wdata->cd, wdata->buffer) == 0)
  {
    wdata->templen = wdata->state->lastchar;
    mutt_mem_realloc(&wdata->tempbuf, wdata->templen * sizeof(wchar_t));
    memcpy(wdata->tempbuf, wdata->state->wbuf, wdata->templen * sizeof(wchar_t));
  }
  else
  {
    return FR_ERROR; // let the user know that nothing matched
  }
  replace_part(wdata->state, 0, buf_string(wdata->buffer));
  return rc;
}

/**
 * complete_file_simple - Complete a filename - Implements ::complete_function_t - @ingroup complete_api
 */
int complete_file_simple(struct EnterWindowData *wdata, int op)
{
  if (!wdata || ((op != OP_EDITOR_COMPLETE) && (op != OP_EDITOR_COMPLETE_QUERY)))
    return FR_NO_ACTION;

  int rc = FR_SUCCESS;
  size_t i;
  for (i = wdata->state->curpos;
       (i > 0) && !mutt_mb_is_shell_char(wdata->state->wbuf[i - 1]); i--)
  {
  }
  buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf + i, wdata->state->curpos - i);
  if (wdata->tempbuf && (wdata->templen == (wdata->state->lastchar - i)) &&
      (memcmp(wdata->tempbuf, wdata->state->wbuf + i,
              (wdata->state->lastchar - i) * sizeof(wchar_t)) == 0))
  {
    dlg_browser(wdata->buffer, MUTT_SEL_NO_FLAGS, NULL, NULL, NULL);
    if (!buf_is_empty(wdata->buffer))
      replace_part(wdata->state, i, buf_string(wdata->buffer));
    return FR_CONTINUE;
  }

  if (mutt_complete(wdata->cd, wdata->buffer) == 0)
  {
    wdata->templen = wdata->state->lastchar - i;
    mutt_mem_realloc(&wdata->tempbuf, wdata->templen * sizeof(wchar_t));
    memcpy(wdata->tempbuf, wdata->state->wbuf + i, wdata->templen * sizeof(wchar_t));
  }
  else
  {
    rc = FR_ERROR;
  }

  replace_part(wdata->state, i, buf_string(wdata->buffer));
  return rc;
}

/**
 * CompleteFileOps - Auto-Completion of Files
 */
const struct CompleteOps CompleteFileOps = {
  .complete = complete_file_simple,
};

/**
 * CompleteMailboxOps - Auto-Completion of Files / Mailboxes
 */
const struct CompleteOps CompleteMailboxOps = {
  .complete = complete_file_mbox,
};
