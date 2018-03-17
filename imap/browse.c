/**
 * @file
 * GUI select an IMAP mailbox from a list
 *
 * @authors
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2008 Brendan Cully <brendan@kublai.com>
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
 * @page imap_browse Mailbox browser
 *
 * GUI select an IMAP mailbox from a list
 */

#include "config.h"
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "imap_private.h"
#include "mutt/mutt.h"
#include "mutt.h"
#include "browser.h"
#include "buffy.h"
#include "context.h"
#include "globals.h"
#include "imap/imap.h"
#include "mutt_account.h"
#include "options.h"
#include "protos.h"

/**
 * add_folder - Format and add an IMAP folder to the browser
 * @param delim       Path delimiter
 * @param folder      Name of the folder
 * @param noselect    true if item isn't selectable
 * @param noinferiors true if item has no children
 * @param state       Browser state to add to
 * @param isparent    true if item represents the parent folder
 *
 * The folder parameter should already be 'unmunged' via
 * imap_unmunge_mbox_name().
 */
static void add_folder(char delim, char *folder, int noselect, int noinferiors,
                       struct BrowserState *state, short isparent)
{
  char tmp[LONG_STRING];
  char relpath[LONG_STRING];
  struct ImapMbox mx;
  struct Buffy *b = NULL;

  if (imap_parse_path(state->folder, &mx))
    return;

  if (state->entrylen + 1 == state->entrymax)
  {
    mutt_mem_realloc(&state->entry, sizeof(struct FolderFile) * (state->entrymax += 256));
    memset(state->entry + state->entrylen, 0,
           (sizeof(struct FolderFile) * (state->entrymax - state->entrylen)));
  }

  /* render superiors as unix-standard ".." */
  if (isparent)
    mutt_str_strfcpy(relpath, "../", sizeof(relpath));
  /* strip current folder from target, to render a relative path */
  else if (mutt_str_strncmp(mx.mbox, folder, mutt_str_strlen(mx.mbox)) == 0)
    mutt_str_strfcpy(relpath, folder + mutt_str_strlen(mx.mbox), sizeof(relpath));
  else
    mutt_str_strfcpy(relpath, folder, sizeof(relpath));

  /* apply filemask filter. This should really be done at menu setup rather
   * than at scan, since it's so expensive to scan. But that's big changes
   * to browser.c */
  if (Mask && Mask->regex && !((regexec(Mask->regex, relpath, 0, NULL, 0) == 0) ^ Mask->not))
  {
    FREE(&mx.mbox);
    return;
  }

  imap_qualify_path(tmp, sizeof(tmp), &mx, folder);
  (state->entry)[state->entrylen].name = mutt_str_strdup(tmp);

  /* mark desc with delim in browser if it can have subfolders */
  if (!isparent && !noinferiors && strlen(relpath) < sizeof(relpath) - 1)
  {
    relpath[strlen(relpath) + 1] = '\0';
    relpath[strlen(relpath)] = delim;
  }

  (state->entry)[state->entrylen].desc = mutt_str_strdup(relpath);

  (state->entry)[state->entrylen].imap = true;
  /* delimiter at the root is useless. */
  if (folder[0] == '\0')
    delim = '\0';
  (state->entry)[state->entrylen].delim = delim;
  (state->entry)[state->entrylen].selectable = !noselect;
  (state->entry)[state->entrylen].inferiors = !noinferiors;

  b = Incoming;
  while (b && (mutt_str_strcmp(tmp, b->path) != 0))
    b = b->next;
  if (b)
  {
    if (Context && (mutt_str_strcmp(b->realpath, Context->realpath) == 0))
    {
      b->msg_count = Context->msgcount;
      b->msg_unread = Context->unread;
    }
    (state->entry)[state->entrylen].has_buffy = true;
    (state->entry)[state->entrylen].new = b->new;
    (state->entry)[state->entrylen].msg_count = b->msg_count;
    (state->entry)[state->entrylen].msg_unread = b->msg_unread;
  }

  (state->entrylen)++;

  FREE(&mx.mbox);
}

/**
 * browse_add_list_result - Add entries to the folder browser
 * @param idata    Server data
 * @param cmd      Command string from server
 * @param state    Browser state to add to
 * @param isparent Is this a shortcut for the parent directory?
 * @retval  0 Success
 * @retval -1 Failure
 */
static int browse_add_list_result(struct ImapData *idata, const char *cmd,
                                  struct BrowserState *state, short isparent)
{
  struct ImapList list;
  struct ImapMbox mx;
  int rc;

  if (imap_parse_path(state->folder, &mx))
  {
    mutt_debug(2, "current folder %s makes no sense\n", state->folder);
    return -1;
  }

  imap_cmd_start(idata, cmd);
  idata->cmdtype = IMAP_CT_LIST;
  idata->cmddata = &list;
  do
  {
    list.name = NULL;
    rc = imap_cmd_step(idata);

    if (rc == IMAP_CMD_CONTINUE && list.name)
    {
      /* Let a parent folder never be selectable for navigation */
      if (isparent)
        list.noselect = true;
      /* prune current folder from output */
      if (isparent || (mutt_str_strncmp(list.name, mx.mbox, strlen(list.name)) != 0))
        add_folder(list.delim, list.name, list.noselect, list.noinferiors, state, isparent);
    }
  } while (rc == IMAP_CMD_CONTINUE);
  idata->cmddata = NULL;

  FREE(&mx.mbox);
  return rc == IMAP_CMD_OK ? 0 : -1;
}

/**
 * imap_browse - IMAP hook into the folder browser
 * @param path  Current folder
 * @param state BrowserState to populate
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Fill out browser_state, given a current folder to browse
 */
int imap_browse(char *path, struct BrowserState *state)
{
  struct ImapData *idata = NULL;
  struct ImapList list;
  char buf[LONG_STRING];
  char mbox[LONG_STRING];
  char munged_mbox[LONG_STRING];
  char list_cmd[5];
  int n;
  char ctmp;
  bool showparents = false;
  bool save_lsub;
  struct ImapMbox mx;

  if (imap_parse_path(path, &mx))
  {
    mutt_error(_("%s is an invalid IMAP path"), path);
    return -1;
  }

  save_lsub = ImapCheckSubscribed;
  ImapCheckSubscribed = false;
  mutt_str_strfcpy(list_cmd, ImapListSubscribed ? "LSUB" : "LIST", sizeof(list_cmd));

  idata = imap_conn_find(&(mx.account), 0);
  if (!idata)
    goto fail;

  mutt_message(_("Getting folder list..."));

  /* skip check for parents when at the root */
  if (mx.mbox && mx.mbox[0] != '\0')
  {
    imap_fix_path(idata, mx.mbox, mbox, sizeof(mbox));
    n = mutt_str_strlen(mbox);
  }
  else
  {
    mbox[0] = '\0';
    n = 0;
  }

  if (n)
  {
    int rc;
    mutt_debug(3, "mbox: %s\n", mbox);

    /* if our target exists and has inferiors, enter it if we
     * aren't already going to */
    imap_munge_mbox_name(idata, munged_mbox, sizeof(munged_mbox), mbox);
    snprintf(buf, sizeof(buf), "%s \"\" %s", list_cmd, munged_mbox);
    imap_cmd_start(idata, buf);
    idata->cmdtype = IMAP_CT_LIST;
    idata->cmddata = &list;
    do
    {
      list.name = 0;
      rc = imap_cmd_step(idata);
      if (rc == IMAP_CMD_CONTINUE && list.name)
      {
        if (!list.noinferiors && list.name[0] &&
            (imap_mxcmp(list.name, mbox) == 0) && n < sizeof(mbox) - 1)
        {
          mbox[n++] = list.delim;
          mbox[n] = '\0';
        }
      }
    } while (rc == IMAP_CMD_CONTINUE);
    idata->cmddata = NULL;

    /* if we're descending a folder, mark it as current in browser_state */
    if (mbox[n - 1] == list.delim)
    {
      showparents = true;
      imap_qualify_path(buf, sizeof(buf), &mx, mbox);
      state->folder = mutt_str_strdup(buf);
      n--;
    }

    /* Find superiors to list
     * Note: UW-IMAP servers return folder + delimiter when asked to list
     *  folder + delimiter. Cyrus servers don't. So we ask for folder,
     *  and tack on delimiter ourselves.
     * Further note: UW-IMAP servers return nothing when asked for
     *  NAMESPACES without delimiters at the end. Argh! */
    for (n--; n >= 0 && mbox[n] != list.delim; n--)
      ;
    if (n > 0) /* "aaaa/bbbb/" -> "aaaa" */
    {
      /* forget the check, it is too delicate (see above). Have we ever
       * had the parent not exist? */
      ctmp = mbox[n];
      mbox[n] = '\0';

      if (showparents)
      {
        mutt_debug(3, "adding parent %s\n", mbox);
        add_folder(list.delim, mbox, 1, 0, state, 1);
      }

      /* if our target isn't a folder, we are in our superior */
      if (!state->folder)
      {
        /* store folder with delimiter */
        mbox[n++] = ctmp;
        ctmp = mbox[n];
        mbox[n] = '\0';
        imap_qualify_path(buf, sizeof(buf), &mx, mbox);
        state->folder = mutt_str_strdup(buf);
      }
      mbox[n] = ctmp;
    }
    /* "/bbbb/" -> add  "/", "aaaa/" -> add "" */
    else
    {
      char relpath[2];
      /* folder may be "/" */
      snprintf(relpath, sizeof(relpath), "%c", n < 0 ? '\0' : idata->delim);
      if (showparents)
        add_folder(idata->delim, relpath, 1, 0, state, 1);
      if (!state->folder)
      {
        imap_qualify_path(buf, sizeof(buf), &mx, relpath);
        state->folder = mutt_str_strdup(buf);
      }
    }
  }

  /* no namespace, no folder: set folder to host only */
  if (!state->folder)
  {
    imap_qualify_path(buf, sizeof(buf), &mx, NULL);
    state->folder = mutt_str_strdup(buf);
  }

  mutt_debug(3, "Quoting mailbox scan: %s -> ", mbox);
  snprintf(buf, sizeof(buf), "%s%%", mbox);
  imap_munge_mbox_name(idata, munged_mbox, sizeof(munged_mbox), buf);
  mutt_debug(3, "%s\n", munged_mbox);
  snprintf(buf, sizeof(buf), "%s \"\" %s", list_cmd, munged_mbox);
  if (browse_add_list_result(idata, buf, state, 0))
    goto fail;

  if (!state->entrylen)
  {
    mutt_error(_("No such folder"));
    goto fail;
  }

  mutt_clear_error();

  if (save_lsub)
    ImapCheckSubscribed = true;

  FREE(&mx.mbox);
  return 0;

fail:
  if (save_lsub)
    ImapCheckSubscribed = true;
  FREE(&mx.mbox);
  return -1;
}

/**
 * imap_mailbox_create - Create a new IMAP mailbox
 * @param folder Mailbox to create
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Prompt for a new mailbox name, and try to create it
 */
int imap_mailbox_create(const char *folder)
{
  struct ImapData *idata = NULL;
  struct ImapMbox mx;
  char buf[LONG_STRING];
  short n;

  if (imap_parse_path(folder, &mx) < 0)
  {
    mutt_debug(1, "Bad starting path %s\n", folder);
    return -1;
  }

  idata = imap_conn_find(&mx.account, MUTT_IMAP_CONN_NONEW);
  if (!idata)
  {
    mutt_debug(1, "Couldn't find open connection to %s\n", mx.account.host);
    goto fail;
  }

  mutt_str_strfcpy(buf, NONULL(mx.mbox), sizeof(buf));

  /* append a delimiter if necessary */
  n = mutt_str_strlen(buf);
  if (n && (n < sizeof(buf) - 1) && (buf[n - 1] != idata->delim))
  {
    buf[n++] = idata->delim;
    buf[n] = '\0';
  }

  if (mutt_get_field(_("Create mailbox: "), buf, sizeof(buf), MUTT_FILE) < 0)
    goto fail;

  if (!mutt_str_strlen(buf))
  {
    mutt_error(_("Mailbox must have a name."));
    goto fail;
  }

  if (imap_create_mailbox(idata, buf) < 0)
    goto fail;

  mutt_message(_("Mailbox created."));
  mutt_sleep(0);

  FREE(&mx.mbox);
  return 0;

fail:
  FREE(&mx.mbox);
  return -1;
}

/**
 * imap_mailbox_rename - Rename a mailbox
 * @param mailbox Mailbox to rename
 * @retval  0 Success
 * @retval -1 Failure
 *
 * The user will be prompted for a new name.
 */
int imap_mailbox_rename(const char *mailbox)
{
  struct ImapData *idata = NULL;
  struct ImapMbox mx;
  char buf[LONG_STRING];
  char newname[SHORT_STRING];

  if (imap_parse_path(mailbox, &mx) < 0)
  {
    mutt_debug(1, "Bad source mailbox %s\n", mailbox);
    return -1;
  }

  idata = imap_conn_find(&mx.account, MUTT_IMAP_CONN_NONEW);
  if (!idata)
  {
    mutt_debug(1, "Couldn't find open connection to %s\n", mx.account.host);
    goto fail;
  }

  if (!mx.mbox)
  {
    mutt_error(_("Cannot rename root folder"));
    goto fail;
  }

  snprintf(buf, sizeof(buf), _("Rename mailbox %s to: "), mx.mbox);
  mutt_str_strfcpy(newname, mx.mbox, sizeof(newname));

  if (mutt_get_field(buf, newname, sizeof(newname), MUTT_FILE) < 0)
    goto fail;

  if (!mutt_str_strlen(newname))
  {
    mutt_error(_("Mailbox must have a name."));
    goto fail;
  }

  imap_fix_path(idata, newname, buf, sizeof(buf));

  if (imap_rename_mailbox(idata, &mx, buf) < 0)
  {
    mutt_error(_("Rename failed: %s"), imap_get_qualifier(idata->buf));
    goto fail;
  }

  mutt_message(_("Mailbox renamed."));
  mutt_sleep(0);

  FREE(&mx.mbox);
  return 0;

fail:
  FREE(&mx.mbox);
  return -1;
}
