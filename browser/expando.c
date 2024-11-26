/**
 * @file
 * Browser Expando definitions
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page browser_expando Browser Expando definitions
 *
 * Browser Expando definitions
 */

#include "config.h"
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "expando.h"
#include "lib.h"
#include "expando/lib.h"
#include "muttlib.h"

/**
 * folder_date_num - Browser: Last modified (strftime) - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_date_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  if (!folder->ff->local)
    return 0;

  return folder->ff->mtime;
}

/**
 * folder_date - Browser: Last modified (strftime) - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_date(const struct ExpandoNode *node, void *data,
                        MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  if (!folder->ff->local)
    return;

  bool use_c_locale = false;
  const char *text = node->text;
  if (*text == '!')
  {
    use_c_locale = true;
    text++;
  }

  char tmp[128] = { 0 };
  struct tm tm = mutt_date_localtime(folder->ff->mtime);

  if (use_c_locale)
  {
    strftime_l(tmp, sizeof(tmp), text, &tm, NeoMutt->time_c_locale);
  }
  else
  {
    strftime(tmp, sizeof(tmp), text, &tm);
  }

  buf_strcpy(buf, tmp);
}

/**
 * folder_space - Fixed whitespace - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_space(const struct ExpandoNode *node, void *data,
                         MuttFormatFlags flags, struct Buffer *buf)
{
  buf_addstr(buf, " ");
}

/**
 * folder_a_num - Browser: Alert for new mail - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_a_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  return folder->ff->notify_user;
}

/**
 * folder_C_num - Browser: Index number - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_C_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  return folder->num + 1;
}

/**
 * folder_d_num - Browser: Last modified - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_d_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;
  if (!folder->ff->local)
    return 0;

  return folder->ff->mtime;
}

/**
 * folder_d - Browser: Last modified - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_d(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;
  if (!folder->ff->local)
    return;

  static const time_t one_year = 31536000;
  const char *t_fmt = ((mutt_date_now() - folder->ff->mtime) < one_year) ?
                          "%b %d %H:%M" :
                          "%b %d  %Y";

  char tmp[128] = { 0 };

  mutt_date_localtime_format(tmp, sizeof(tmp), t_fmt, folder->ff->mtime);

  buf_strcpy(buf, tmp);
}

/**
 * folder_D_num - Browser: Last modified ($date_format) - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_D_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;
  if (!folder->ff->local)
    return 0;

  return folder->ff->mtime;
}

/**
 * folder_D - Browser: Last modified ($date_format) - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_D(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;
  if (!folder->ff->local)
    return;

  char tmp[128] = { 0 };
  bool use_c_locale = false;
  const char *const c_date_format = cs_subset_string(NeoMutt->sub, "date_format");
  const char *t_fmt = NONULL(c_date_format);
  if (*t_fmt == '!')
  {
    t_fmt++;
    use_c_locale = true;
  }

  if (use_c_locale)
  {
    mutt_date_localtime_format_locale(tmp, sizeof(tmp), t_fmt,
                                      folder->ff->mtime, NeoMutt->time_c_locale);
  }
  else
  {
    mutt_date_localtime_format(tmp, sizeof(tmp), t_fmt, folder->ff->mtime);
  }

  buf_strcpy(buf, tmp);
}

/**
 * folder_f - Browser: Filename - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_f(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  const char *s = NONULL(folder->ff->name);

  buf_printf(buf, "%s%s", s,
             folder->ff->local ?
                 (S_ISLNK(folder->ff->mode) ?
                      "@" :
                      (S_ISDIR(folder->ff->mode) ?
                           "/" :
                           (((folder->ff->mode & S_IXUSR) != 0) ? "*" : ""))) :
                 "");
}

/**
 * folder_F - Browser: File permissions - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_F(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  if (folder->ff->local)
  {
    buf_printf(buf, "%c%c%c%c%c%c%c%c%c%c",
               S_ISDIR(folder->ff->mode) ? 'd' : (S_ISLNK(folder->ff->mode) ? 'l' : '-'),
               ((folder->ff->mode & S_IRUSR) != 0) ? 'r' : '-',
               ((folder->ff->mode & S_IWUSR) != 0) ? 'w' : '-',
               ((folder->ff->mode & S_ISUID) != 0) ? 's' :
               ((folder->ff->mode & S_IXUSR) != 0) ? 'x' :
                                                     '-',
               ((folder->ff->mode & S_IRGRP) != 0) ? 'r' : '-',
               ((folder->ff->mode & S_IWGRP) != 0) ? 'w' : '-',
               ((folder->ff->mode & S_ISGID) != 0) ? 's' :
               ((folder->ff->mode & S_IXGRP) != 0) ? 'x' :
                                                     '-',
               ((folder->ff->mode & S_IROTH) != 0) ? 'r' : '-',
               ((folder->ff->mode & S_IWOTH) != 0) ? 'w' : '-',
               ((folder->ff->mode & S_ISVTX) != 0) ? 't' :
               ((folder->ff->mode & S_IXOTH) != 0) ? 'x' :
                                                     '-');
  }
  else if (folder->ff->imap)
  {
    /* mark folders with subfolders AND mail */
    buf_printf(buf, "IMAP %c", (folder->ff->inferiors && folder->ff->selectable) ? '+' : ' ');
  }
}

/**
 * folder_g - Browser: Group name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_g(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;
  if (!folder->ff->local)
    return;

  struct group *gr = getgrgid(folder->ff->gid);
  if (gr)
  {
    buf_addstr(buf, gr->gr_name);
  }
  else
  {
    buf_printf(buf, "%u", folder->ff->gid);
  }
}

/**
 * folder_i - Browser: Description - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_i(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  const char *s = NULL;
  if (folder->ff->desc)
    s = folder->ff->desc;
  else
    s = folder->ff->name;

  buf_printf(buf, "%s%s", s,
             folder->ff->local ?
                 (S_ISLNK(folder->ff->mode) ?
                      "@" :
                      (S_ISDIR(folder->ff->mode) ?
                           "/" :
                           (((folder->ff->mode & S_IXUSR) != 0) ? "*" : ""))) :
                 "");
}

/**
 * folder_l_num - Browser: Hard links - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_l_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  if (folder->ff->local)
    return folder->ff->nlink;

  return 0;
}

/**
 * folder_l - Browser: Hard links - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_l(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;
  if (!folder->ff->local)
    return;

  buf_add_printf(buf, "%d", (int) folder->ff->nlink);
}

/**
 * folder_m_num - Browser: Number of messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_m_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  if (folder->ff->has_mailbox)
    return folder->ff->msg_count;

  return 0;
}

/**
 * folder_m - Browser: Number of messages - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_m(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;
  if (!folder->ff->has_mailbox)
    return;

  buf_add_printf(buf, "%d", folder->ff->msg_count);
}

/**
 * folder_n_num - Browser: Number of unread messages - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  if (folder->ff->has_mailbox)
    return folder->ff->msg_unread;

  return 0;
}

/**
 * folder_n - Browser: Number of unread messages - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_n(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;
  if (!folder->ff->has_mailbox)
    return;

  buf_add_printf(buf, "%d", folder->ff->msg_unread);
}

/**
 * folder_N_num - Browser: New mail flag - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_N_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;
  return folder->ff->has_new_mail;
}

/**
 * folder_N - Browser: New mail flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_N(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = folder->ff->has_new_mail ? "N" : " ";
  buf_strcpy(buf, s);
}

/**
 * folder_p_num - Browser: Poll for new mail - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_p_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;

  return folder->ff->poll_new_mail;
}

/**
 * folder_s_num - Browser: Size in bytes - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_s_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;
  return folder->ff->size;
}

/**
 * folder_s - Browser: Size in bytes - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_s(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  char tmp[128] = { 0 };

  mutt_str_pretty_size(tmp, sizeof(tmp), folder->ff->size);
  buf_strcpy(buf, tmp);
}

/**
 * folder_t_num - Browser: Is Tagged - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long folder_t_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct Folder *folder = data;
  return folder->ff->tagged;
}

/**
 * folder_t - Browser: Is Tagged - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_t(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = folder->ff->tagged ? "*" : " ";
  buf_strcpy(buf, s);
}

/**
 * folder_u - Browser: Owner name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void folder_u(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Folder *folder = data;
  if (!folder->ff->local)
    return;

  struct passwd *pw = getpwuid(folder->ff->uid);
  if (pw)
  {
    buf_addstr(buf, pw->pw_name);
  }
  else
  {
    buf_printf(buf, "%u", folder->ff->uid);
  }
}

/**
 * FolderRenderData - Callbacks for Browser Expandos
 *
 * @sa FolderFormatDef, ExpandoDataFolder, ExpandoDataGlobal
 */
const struct ExpandoRenderData FolderRenderData[] = {
  // clang-format off
  { ED_FOLDER, ED_FOL_NOTIFY,        NULL,         folder_a_num },
  { ED_FOLDER, ED_FOL_NUMBER,        NULL,         folder_C_num },
  { ED_FOLDER, ED_FOL_DATE,          folder_d,     folder_d_num },
  { ED_FOLDER, ED_FOL_DATE_FORMAT,   folder_D,     folder_D_num },
  { ED_FOLDER, ED_FOL_FILE_MODE,     folder_F,     NULL },
  { ED_FOLDER, ED_FOL_FILENAME,      folder_f,     NULL },
  { ED_FOLDER, ED_FOL_FILE_GROUP,    folder_g,     NULL },
  { ED_FOLDER, ED_FOL_DESCRIPTION,   folder_i,     NULL },
  { ED_FOLDER, ED_FOL_HARD_LINKS,    folder_l,     folder_l_num },
  { ED_FOLDER, ED_FOL_MESSAGE_COUNT, folder_m,     folder_m_num },
  { ED_FOLDER, ED_FOL_NEW_MAIL,      folder_N,     folder_N_num },
  { ED_FOLDER, ED_FOL_UNREAD_COUNT,  folder_n,     folder_n_num },
  { ED_FOLDER, ED_FOL_POLL,          NULL,         folder_p_num },
  { ED_FOLDER, ED_FOL_FILE_SIZE,     folder_s,     folder_s_num },
  { ED_FOLDER, ED_FOL_TAGGED,        folder_t,     folder_t_num },
  { ED_FOLDER, ED_FOL_FILE_OWNER,    folder_u,     NULL },
  { ED_FOLDER, ED_FOL_DATE_STRF,     folder_date,  folder_date_num },
  { ED_GLOBAL, ED_GLO_PADDING_SPACE, folder_space, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
