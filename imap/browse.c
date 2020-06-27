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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "private.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "browser.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "muttlib.h"
#include "imap/lib.h"

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
static void add_folder(char delim, char *folder, bool noselect, bool noinferiors,
                       struct BrowserState *state, bool isparent)
{
  char tmp[PATH_MAX];
  char relpath[PATH_MAX];
  struct ConnAccount cac = { { 0 } };
  char mailbox[1024];

  if (imap_parse_path(state->folder, &cac, mailbox, sizeof(mailbox)))
    return;

  if (state->entrylen + 1 == state->entrymax)
  {
    mutt_mem_realloc(&state->entry, sizeof(struct FolderFile) * (state->entrymax += 256));
    memset(state->entry + state->entrylen, 0,
           (sizeof(struct FolderFile) * (state->entrymax - state->entrylen)));
  }

  /* render superiors as unix-standard ".." */
  if (isparent)
    mutt_str_copy(relpath, "../", sizeof(relpath));
  /* strip current folder from target, to render a relative path */
  else if (mutt_str_startswith(folder, mailbox))
    mutt_str_copy(relpath, folder + mutt_str_len(mailbox), sizeof(relpath));
  else
    mutt_str_copy(relpath, folder, sizeof(relpath));

  /* apply filemask filter. This should really be done at menu setup rather
   * than at scan, since it's so expensive to scan. But that's big changes
   * to browser.c */
  if (!mutt_regex_match(C_Mask, relpath))
  {
    return;
  }

  imap_qualify_path(tmp, sizeof(tmp), &cac, folder);
  (state->entry)[state->entrylen].name = mutt_str_dup(tmp);

  /* mark desc with delim in browser if it can have subfolders */
  if (!isparent && !noinferiors && (strlen(relpath) < sizeof(relpath) - 1))
  {
    relpath[strlen(relpath) + 1] = '\0';
    relpath[strlen(relpath)] = delim;
  }

  (state->entry)[state->entrylen].desc = mutt_str_dup(relpath);

  (state->entry)[state->entrylen].imap = true;
  /* delimiter at the root is useless. */
  if (folder[0] == '\0')
    delim = '\0';
  (state->entry)[state->entrylen].delim = delim;
  (state->entry)[state->entrylen].selectable = !noselect;
  (state->entry)[state->entrylen].inferiors = !noinferiors;

  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    if (mutt_str_equal(tmp, mailbox_path(np->mailbox)))
      break;
  }

  if (np)
  {
    (state->entry)[state->entrylen].has_mailbox = true;
    (state->entry)[state->entrylen].has_new_mail = np->mailbox->has_new;
    (state->entry)[state->entrylen].msg_count = np->mailbox->msg_count;
    (state->entry)[state->entrylen].msg_unread = np->mailbox->msg_unread;
  }
  neomutt_mailboxlist_clear(&ml);

  (state->entrylen)++;
}

/**
 * browse_add_list_result - Add entries to the folder browser
 * @param adata    Imap Account data
 * @param cmd      Command string from server
 * @param state    Browser state to add to
 * @param isparent Is this a shortcut for the parent directory?
 * @retval  0 Success
 * @retval -1 Failure
 */
static int browse_add_list_result(struct ImapAccountData *adata, const char *cmd,
                                  struct BrowserState *state, bool isparent)
{
  struct ImapList list = { 0 };
  int rc;
  struct Url *url = url_parse(state->folder);

  imap_cmd_start(adata, cmd);
  adata->cmdresult = &list;
  do
  {
    list.name = NULL;
    rc = imap_cmd_step(adata);

    if ((rc == IMAP_RES_CONTINUE) && list.name)
    {
      /* Let a parent folder never be selectable for navigation */
      if (isparent)
        list.noselect = true;
      /* prune current folder from output */
      if (isparent || !mutt_str_startswith(url->path, list.name))
        add_folder(list.delim, list.name, list.noselect, list.noinferiors, state, isparent);
    }
  } while (rc == IMAP_RES_CONTINUE);
  adata->cmdresult = NULL;

  url_free(&url);

  return (rc == IMAP_RES_OK) ? 0 : -1;
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
int imap_browse(const char *path, struct BrowserState *state)
{
  struct ImapAccountData *adata = NULL;
  struct ImapList list = { 0 };
  struct ConnAccount cac = { { 0 } };
  char buf[PATH_MAX + 16];
  char mbox[PATH_MAX];
  char munged_mbox[PATH_MAX];
  const char *list_cmd = NULL;
  int len;
  int n;
  char ctmp;
  bool showparents = false;
  bool save_lsub;

  if (imap_parse_path(path, &cac, buf, sizeof(buf)))
  {
    mutt_error(_("%s is an invalid IMAP path"), path);
    return -1;
  }

  save_lsub = C_ImapCheckSubscribed;
  C_ImapCheckSubscribed = false;

  // Pick first mailbox connected to the same server
  struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
  neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_IMAP);
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &ml, entries)
  {
    adata = imap_adata_get(np->mailbox);
    // Pick first mailbox connected on the same server
    if (imap_account_match(&adata->conn->account, &cac))
      break;
    adata = NULL;
  }
  neomutt_mailboxlist_clear(&ml);
  if (!adata)
    goto fail;

  if (C_ImapListSubscribed)
  {
    /* RFC3348 section 3 states LSUB is unreliable for hierarchy information.
     * The newer LIST extensions are designed for this.  */
    if (adata->capabilities & IMAP_CAP_LIST_EXTENDED)
      list_cmd = "LIST (SUBSCRIBED RECURSIVEMATCH)";
    else
      list_cmd = "LSUB";
  }
  else
  {
    list_cmd = "LIST";
  }

  mutt_message(_("Getting folder list..."));

  /* skip check for parents when at the root */
  if (buf[0] == '\0')
  {
    mbox[0] = '\0';
    n = 0;
  }
  else
  {
    imap_fix_path(adata->delim, buf, mbox, sizeof(mbox));
    n = mutt_str_len(mbox);
  }

  if (n)
  {
    int rc;
    mutt_debug(LL_DEBUG3, "mbox: %s\n", mbox);

    /* if our target exists and has inferiors, enter it if we
     * aren't already going to */
    imap_munge_mbox_name(adata->unicode, munged_mbox, sizeof(munged_mbox), mbox);
    len = snprintf(buf, sizeof(buf), "%s \"\" %s", list_cmd, munged_mbox);
    if (adata->capabilities & IMAP_CAP_LIST_EXTENDED)
      snprintf(buf + len, sizeof(buf) - len, " RETURN (CHILDREN)");
    imap_cmd_start(adata, buf);
    adata->cmdresult = &list;
    do
    {
      list.name = 0;
      rc = imap_cmd_step(adata);
      if ((rc == IMAP_RES_CONTINUE) && list.name)
      {
        if (!list.noinferiors && list.name[0] &&
            (imap_mxcmp(list.name, mbox) == 0) && (n < sizeof(mbox) - 1))
        {
          mbox[n++] = list.delim;
          mbox[n] = '\0';
        }
      }
    } while (rc == IMAP_RES_CONTINUE);
    adata->cmdresult = NULL;

    /* if we're descending a folder, mark it as current in browser_state */
    if (mbox[n - 1] == list.delim)
    {
      showparents = true;
      imap_qualify_path(buf, sizeof(buf), &cac, mbox);
      state->folder = mutt_str_dup(buf);
      n--;
    }

    /* Find superiors to list
     * Note: UW-IMAP servers return folder + delimiter when asked to list
     *  folder + delimiter. Cyrus servers don't. So we ask for folder,
     *  and tack on delimiter ourselves.
     * Further note: UW-IMAP servers return nothing when asked for
     *  NAMESPACES without delimiters at the end. Argh! */
    for (n--; n >= 0 && mbox[n] != list.delim; n--)
      ; // do nothing

    if (n > 0) /* "aaaa/bbbb/" -> "aaaa" */
    {
      /* forget the check, it is too delicate (see above). Have we ever
       * had the parent not exist? */
      ctmp = mbox[n];
      mbox[n] = '\0';

      if (showparents)
      {
        mutt_debug(LL_DEBUG3, "adding parent %s\n", mbox);
        add_folder(list.delim, mbox, true, false, state, true);
      }

      /* if our target isn't a folder, we are in our superior */
      if (!state->folder)
      {
        /* store folder with delimiter */
        mbox[n++] = ctmp;
        ctmp = mbox[n];
        mbox[n] = '\0';
        imap_qualify_path(buf, sizeof(buf), &cac, mbox);
        state->folder = mutt_str_dup(buf);
      }
      mbox[n] = ctmp;
    }
    /* "/bbbb/" -> add  "/", "aaaa/" -> add "" */
    else
    {
      char relpath[2];
      /* folder may be "/" */
      snprintf(relpath, sizeof(relpath), "%c", (n < 0) ? '\0' : adata->delim);
      if (showparents)
        add_folder(adata->delim, relpath, true, false, state, true);
      if (!state->folder)
      {
        imap_qualify_path(buf, sizeof(buf), &cac, relpath);
        state->folder = mutt_str_dup(buf);
      }
    }
  }

  /* no namespace, no folder: set folder to host only */
  if (!state->folder)
  {
    imap_qualify_path(buf, sizeof(buf), &cac, NULL);
    state->folder = mutt_str_dup(buf);
  }

  mutt_debug(LL_DEBUG3, "Quoting mailbox scan: %s -> ", mbox);
  snprintf(buf, sizeof(buf), "%s%%", mbox);
  imap_munge_mbox_name(adata->unicode, munged_mbox, sizeof(munged_mbox), buf);
  mutt_debug(LL_DEBUG3, "%s\n", munged_mbox);
  len = snprintf(buf, sizeof(buf), "%s \"\" %s", list_cmd, munged_mbox);
  if (adata->capabilities & IMAP_CAP_LIST_EXTENDED)
    snprintf(buf + len, sizeof(buf) - len, " RETURN (CHILDREN)");
  if (browse_add_list_result(adata, buf, state, false))
    goto fail;

  if (state->entrylen == 0)
  {
    mutt_error(_("No such folder"));
    goto fail;
  }

  mutt_clear_error();

  if (save_lsub)
    C_ImapCheckSubscribed = true;

  return 0;

fail:
  if (save_lsub)
    C_ImapCheckSubscribed = true;
  return -1;
}

/**
 * imap_mailbox_create - Create a new IMAP mailbox
 * @param path Mailbox to create
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Prompt for a new mailbox name, and try to create it
 */
int imap_mailbox_create(const char *path)
{
  struct ImapAccountData *adata = NULL;
  struct ImapMboxData *mdata = NULL;
  char name[1024];
  short n;

  if (imap_adata_find(path, &adata, &mdata) < 0)
  {
    mutt_debug(LL_DEBUG1, "Couldn't find open connection to %s\n", path);
    goto err;
  }

  /* append a delimiter if necessary */
  mutt_str_copy(name, mdata->real_name, sizeof(name));
  n = mutt_str_len(name);
  if (n && (n < sizeof(name) - 1) && (name[n - 1] != adata->delim))
  {
    name[n++] = adata->delim;
    name[n] = '\0';
  }

  if (mutt_get_field(_("Create mailbox: "), name, sizeof(name), MUTT_FILE) < 0)
    goto err;

  if (mutt_str_len(name) == 0)
  {
    mutt_error(_("Mailbox must have a name"));
    goto err;
  }

  if (imap_create_mailbox(adata, name) < 0)
    goto err;

  imap_mdata_free((void *) &mdata);
  mutt_message(_("Mailbox created"));
  mutt_sleep(0);
  return 0;

err:
  imap_mdata_free((void *) &mdata);
  return -1;
}

/**
 * imap_mailbox_rename - Rename a mailbox
 * @param path Mailbox to rename
 * @retval  0 Success
 * @retval -1 Failure
 *
 * The user will be prompted for a new name.
 */
int imap_mailbox_rename(const char *path)
{
  struct ImapAccountData *adata = NULL;
  struct ImapMboxData *mdata = NULL;
  char buf[PATH_MAX];
  char newname[PATH_MAX];

  if (imap_adata_find(path, &adata, &mdata) < 0)
  {
    mutt_debug(LL_DEBUG1, "Couldn't find open connection to %s\n", path);
    return -1;
  }

  if (mdata->real_name[0] == '\0')
  {
    mutt_error(_("Can't rename root folder"));
    goto err;
  }

  snprintf(buf, sizeof(buf), _("Rename mailbox %s to: "), mdata->name);
  mutt_str_copy(newname, mdata->name, sizeof(newname));

  if (mutt_get_field(buf, newname, sizeof(newname), MUTT_FILE) < 0)
    goto err;

  if (mutt_str_len(newname) == 0)
  {
    mutt_error(_("Mailbox must have a name"));
    goto err;
  }

  imap_fix_path(adata->delim, newname, buf, sizeof(buf));

  if (imap_rename_mailbox(adata, mdata->name, buf) < 0)
  {
    mutt_error(_("Rename failed: %s"), imap_get_qualifier(adata->buf));
    goto err;
  }

  mutt_message(_("Mailbox renamed"));
  mutt_sleep(0);
  return 0;

err:
  imap_mdata_free((void *) &mdata);
  return -1;
}
