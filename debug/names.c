/**
 * @file
 * Debug names
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * @page debug_names Debug names
 *
 * Debug names
 */

#include "config.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "menu/lib.h"
#include "mview.h"

#define DEBUG_NAME(NAME)                                                       \
  case NAME:                                                                   \
    return #NAME

#define DEBUG_DEFAULT                                                          \
  default:                                                                     \
    return "UNKNOWN"

const char *name_content_type(enum ContentType type)
{
  switch (type)
  {
    DEBUG_NAME(TYPE_ANY);
    DEBUG_NAME(TYPE_APPLICATION);
    DEBUG_NAME(TYPE_AUDIO);
    DEBUG_NAME(TYPE_IMAGE);
    DEBUG_NAME(TYPE_MESSAGE);
    DEBUG_NAME(TYPE_MODEL);
    DEBUG_NAME(TYPE_MULTIPART);
    DEBUG_NAME(TYPE_OTHER);
    DEBUG_NAME(TYPE_TEXT);
    DEBUG_NAME(TYPE_VIDEO);
    DEBUG_DEFAULT;
  }
}

const char *name_content_encoding(enum ContentEncoding enc)
{
  switch (enc)
  {
    DEBUG_NAME(ENC_7BIT);
    DEBUG_NAME(ENC_8BIT);
    DEBUG_NAME(ENC_BASE64);
    DEBUG_NAME(ENC_BINARY);
    DEBUG_NAME(ENC_OTHER);
    DEBUG_NAME(ENC_QUOTED_PRINTABLE);
    DEBUG_NAME(ENC_UUENCODED);
    DEBUG_DEFAULT;
  }
}

const char *name_content_disposition(enum ContentDisposition disp)
{
  switch (disp)
  {
    DEBUG_NAME(DISP_ATTACH);
    DEBUG_NAME(DISP_FORM_DATA);
    DEBUG_NAME(DISP_INLINE);
    DEBUG_NAME(DISP_NONE);
    DEBUG_DEFAULT;
  }
}

const char *name_notify_type(enum NotifyType type)
{
  switch (type)
  {
    DEBUG_NAME(NT_ACCOUNT);
    DEBUG_NAME(NT_ALIAS);
    DEBUG_NAME(NT_ALL);
    DEBUG_NAME(NT_ALTERN);
    DEBUG_NAME(NT_ATTACH);
    DEBUG_NAME(NT_BINDING);
    DEBUG_NAME(NT_COLOR);
    DEBUG_NAME(NT_COMMAND);
    DEBUG_NAME(NT_CONFIG);
    DEBUG_NAME(NT_EMAIL);
    DEBUG_NAME(NT_ENVELOPE);
    DEBUG_NAME(NT_GLOBAL);
    DEBUG_NAME(NT_HEADER);
    DEBUG_NAME(NT_INDEX);
    DEBUG_NAME(NT_MAILBOX);
    DEBUG_NAME(NT_MENU);
    DEBUG_NAME(NT_MVIEW);
    DEBUG_NAME(NT_PAGER);
    DEBUG_NAME(NT_SCORE);
    DEBUG_NAME(NT_SUBJRX);
    DEBUG_NAME(NT_WINDOW);
    DEBUG_DEFAULT;
  }
}

const char *name_mailbox_type(enum MailboxType type)
{
  switch (type)
  {
    DEBUG_NAME(MUTT_COMPRESSED);
    DEBUG_NAME(MUTT_IMAP);
    DEBUG_NAME(MUTT_MAILBOX_ANY);
    DEBUG_NAME(MUTT_MAILBOX_ERROR);
    DEBUG_NAME(MUTT_MAILDIR);
    DEBUG_NAME(MUTT_MBOX);
    DEBUG_NAME(MUTT_MH);
    DEBUG_NAME(MUTT_MMDF);
    DEBUG_NAME(MUTT_NNTP);
    DEBUG_NAME(MUTT_NOTMUCH);
    DEBUG_NAME(MUTT_POP);
    DEBUG_NAME(MUTT_UNKNOWN);
    DEBUG_DEFAULT;
  }
}

const char *name_notify_global(int id)
{
  switch (id)
  {
    DEBUG_NAME(NT_GLOBAL_COMMAND);
    DEBUG_NAME(NT_GLOBAL_SHUTDOWN);
    DEBUG_NAME(NT_GLOBAL_STARTUP);
    DEBUG_NAME(NT_GLOBAL_TIMEOUT);
    DEBUG_DEFAULT;
  }
}

const char *name_notify_config(int id)
{
  switch (id)
  {
    DEBUG_NAME(NT_CONFIG_SET);
    DEBUG_NAME(NT_CONFIG_RESET);
    DEBUG_NAME(NT_CONFIG_DELETED);
    DEBUG_DEFAULT;
  }
}

const char *name_notify_mailbox(int id)
{
  switch (id)
  {
    DEBUG_NAME(NT_MAILBOX_ADD);
    DEBUG_NAME(NT_MAILBOX_CHANGE);
    DEBUG_NAME(NT_MAILBOX_DELETE);
    DEBUG_NAME(NT_MAILBOX_DELETE_ALL);
    DEBUG_NAME(NT_MAILBOX_INVALID);
    DEBUG_NAME(NT_MAILBOX_RESORT);
    DEBUG_NAME(NT_MAILBOX_UNTAG);
    DEBUG_NAME(NT_MAILBOX_UPDATE);
    DEBUG_DEFAULT;
  }
}

const char *name_notify_mview(int id)
{
  switch (id)
  {
    DEBUG_NAME(NT_MVIEW_ADD);
    DEBUG_NAME(NT_MVIEW_CHANGE);
    DEBUG_NAME(NT_MVIEW_DELETE);
    DEBUG_DEFAULT;
  }
}

const char *name_window_type(const struct MuttWindow *win)
{
  if (!win)
    return "NULL";

  switch (win->type)
  {
    DEBUG_NAME(WT_ALL_DIALOGS);
    DEBUG_NAME(WT_CONTAINER);
    DEBUG_NAME(WT_CUSTOM);
    DEBUG_NAME(WT_DLG_ALIAS);
    DEBUG_NAME(WT_DLG_ATTACH);
    DEBUG_NAME(WT_DLG_AUTOCRYPT);
    DEBUG_NAME(WT_DLG_BROWSER);
    DEBUG_NAME(WT_DLG_CERTIFICATE);
    DEBUG_NAME(WT_DLG_COMPOSE);
    DEBUG_NAME(WT_DLG_CRYPT_GPGME);
    DEBUG_NAME(WT_DLG_DO_PAGER);
    DEBUG_NAME(WT_DLG_HISTORY);
    DEBUG_NAME(WT_DLG_INDEX);
    DEBUG_NAME(WT_DLG_PATTERN);
    DEBUG_NAME(WT_DLG_PGP);
    DEBUG_NAME(WT_DLG_POSTPONE);
    DEBUG_NAME(WT_DLG_QUERY);
    DEBUG_NAME(WT_DLG_REMAILER);
    DEBUG_NAME(WT_DLG_SMIME);
    DEBUG_NAME(WT_HELP_BAR);
    DEBUG_NAME(WT_INDEX);
    DEBUG_NAME(WT_MENU);
    DEBUG_NAME(WT_MESSAGE);
    DEBUG_NAME(WT_PAGER);
    DEBUG_NAME(WT_ROOT);
    DEBUG_NAME(WT_SIDEBAR);
    DEBUG_NAME(WT_STATUS_BAR);
    DEBUG_DEFAULT;
  }
}

const char *name_window_size(const struct MuttWindow *win)
{
  if (!win)
    return "NULL";

  switch (win->size)
  {
    DEBUG_NAME(MUTT_WIN_SIZE_FIXED);
    DEBUG_NAME(MUTT_WIN_SIZE_MAXIMISE);
    DEBUG_NAME(MUTT_WIN_SIZE_MINIMISE);
    DEBUG_DEFAULT;
  }
}

const char *name_menu_type(enum MenuType mt)
{
  switch (mt)
  {
    DEBUG_NAME(MENU_ALIAS);
    DEBUG_NAME(MENU_ATTACH);
#ifdef USE_AUTOCRYPT
    DEBUG_NAME(MENU_AUTOCRYPT_ACCT);
#endif
    DEBUG_NAME(MENU_COMPOSE);
    DEBUG_NAME(MENU_EDITOR);
    DEBUG_NAME(MENU_FOLDER);
    DEBUG_NAME(MENU_GENERIC);
#ifdef CRYPT_BACKEND_GPGME
    DEBUG_NAME(MENU_KEY_SELECT_PGP);
    DEBUG_NAME(MENU_KEY_SELECT_SMIME);
#endif
    DEBUG_NAME(MENU_INDEX);
#ifdef MIXMASTER
    DEBUG_NAME(MENU_MIX);
#endif
    DEBUG_NAME(MENU_PAGER);
    DEBUG_NAME(MENU_PGP);
    DEBUG_NAME(MENU_POSTPONE);
    DEBUG_NAME(MENU_QUERY);
    DEBUG_NAME(MENU_SMIME);
    DEBUG_NAME(MENU_MAX);
    DEBUG_DEFAULT;
  }
}
