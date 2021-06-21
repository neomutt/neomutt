/**
 * @file
 * Notmuch virtual mailbox type
 *
 * @authors
 * Copyright (C) 2011-2016 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2016-2018 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2016 Kevin Velghe <kevin@paretje.be>
 * Copyright (C) 2017 Bernard 'Guyzmo' Pratz <guyzmo+github+pub@m0g.net>
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
 * @page nm_notmuch Notmuch virtual mailbox type
 *
 * Notmuch virtual mailbox type
 *
 * ## Notes
 *
 * - notmuch uses private Mailbox->data and private Email->data
 *
 * - all exported functions are usable within notmuch context only
 *
 * - all functions have to be covered by "mailbox->type == MUTT_NOTMUCH" check
 *   (it's implemented in nm_mdata_get() and init_mailbox() functions).
 *
 * - exception are nm_nonctx_* functions -- these functions use nm_default_url
 *   (or parse URL from another resource)
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <notmuch.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "hcache/lib.h"
#include "index/lib.h"
#include "maildir/lib.h"
#include "progress/lib.h"
#include "adata.h"
#include "command_parse.h"
#include "edata.h"
#include "maildir/edata.h"
#include "mdata.h"
#include "mutt_commands.h"
#include "mutt_globals.h"
#include "mutt_thread.h"
#include "mx.h"
#include "protos.h"
#include "query.h"
#include "tag.h"

struct stat;

static const struct Command nm_commands[] = {
  // clang-format off
  { "unvirtual-mailboxes", parse_unmailboxes, MUTT_COMMAND_DEPRECATED },
  { "virtual-mailboxes",   parse_mailboxes,   MUTT_COMMAND_DEPRECATED|MUTT_NAMED },
  // clang-format on
};

const char NmUrlProtocol[] = "notmuch://";
const int NmUrlProtocolLen = sizeof(NmUrlProtocol) - 1;

/**
 * nm_init - Setup feature commands
 */
void nm_init(void)
{
  COMMANDS_REGISTER(nm_commands);
}

/**
 * nm_hcache_open - Open a header cache
 * @param m Mailbox
 * @retval ptr Header cache handle
 */
static struct HeaderCache *nm_hcache_open(struct Mailbox *m)
{
#ifdef USE_HCACHE
  const char *const c_header_cache =
      cs_subset_path(NeoMutt->sub, "header_cache");
  return mutt_hcache_open(c_header_cache, mailbox_path(m), NULL);
#else
  return NULL;
#endif
}

/**
 * nm_hcache_close - Close the header cache
 * @param h Header cache handle
 */
static void nm_hcache_close(struct HeaderCache *h)
{
#ifdef USE_HCACHE
  mutt_hcache_close(h);
#endif
}

/**
 * nm_get_default_url - Create a Mailbox with default Notmuch settings
 * @retval ptr  Mailbox with default Notmuch settings
 * @retval NULL Error, it's impossible to create an NmMboxData
 */
static char *nm_get_default_url(void)
{
  // path to DB + query + url "decoration"
  size_t len = PATH_MAX + 1024 + 32;
  char *url = mutt_mem_malloc(len);

  // Try to use `$nm_default_url` or `$folder`.
  // If neither are set, it is impossible to create a Notmuch URL.
  const char *const c_nm_default_url =
      cs_subset_string(NeoMutt->sub, "nm_default_url");
  const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
  if (c_nm_default_url)
    snprintf(url, len, "%s", c_nm_default_url);
  else if (c_folder)
    snprintf(url, len, "notmuch://%s", c_folder);
  else
  {
    FREE(&url);
    return NULL;
  }

  return url;
}

/**
 * nm_get_default_data - Create a Mailbox with default Notmuch settings
 * @retval ptr  Mailbox with default Notmuch settings
 * @retval NULL Error, it's impossible to create an NmMboxData
 */
static struct NmMboxData *nm_get_default_data(void)
{
  // path to DB + query + url "decoration"
  char *url = nm_get_default_url();
  if (!url)
    return NULL;

  struct NmMboxData *default_data = nm_mdata_new(url);
  FREE(&url);

  return default_data;
}

/**
 * init_mailbox - Add Notmuch data to the Mailbox
 * @param m Mailbox
 * @retval  0 Success
 * @retval -1 Error Bad format
 *
 * Create a new NmMboxData struct and add it Mailbox::data.
 * Notmuch-specific data will be stored in this struct.
 * This struct can be freed using nm_mdata_free().
 */
static int init_mailbox(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_NOTMUCH))
    return -1;

  if (m->mdata)
    return 0;

  m->mdata = nm_mdata_new(mailbox_path(m));
  if (!m->mdata)
    return -1;

  m->mdata_free = nm_mdata_free;
  return 0;
}

/**
 * email_get_id - Get the unique Notmuch Id
 * @param e Email
 * @retval ptr  ID string
 * @retval NULL Error
 */
static char *email_get_id(struct Email *e)
{
  struct NmEmailData *edata = nm_edata_get(e);
  if (!edata)
    return NULL;

  return edata->virtual_id;
}

/**
 * email_get_fullpath - Get the full path of an email
 * @param e      Email
 * @param buf    Buffer for the path
 * @param buflen Length of the buffer
 * @retval ptr Path string
 */
static char *email_get_fullpath(struct Email *e, char *buf, size_t buflen)
{
  snprintf(buf, buflen, "%s/%s", nm_email_get_folder(e), e->path);
  return buf;
}

/**
 * query_window_reset - Restore vfolder's search window to its original position
 *
 * After moving a vfolder search window backward and forward, calling this function
 * will reset the search position to its original value, setting to 0 the user settable
 * variable:
 *
 *     nm_query_window_current_position
 */
static void query_window_reset(void)
{
  mutt_debug(LL_DEBUG2, "entering\n");
  cs_subset_str_native_set(NeoMutt->sub, "nm_query_window_current_position", 0, NULL);
}

/**
 * windowed_query_from_query - transforms a vfolder search query into a windowed one
 * @param[in]  query vfolder search string
 * @param[out] buf   allocated string buffer to receive the modified search query
 * @param[in]  buflen allocated maximum size of the buf string buffer
 * @retval true  Transformed search query is available as a string in buf
 * @retval false Search query shall not be transformed
 *
 * Creates a `date:` search term window from the following user settings:
 *
 * - `nm_query_window_enable` (only required for `nm_query_window_duration = 0`)
 * - `nm_query_window_duration`
 * - `nm_query_window_timebase`
 * - `nm_query_window_current_position`
 *
 * The window won't be applied:
 *
 * - If the duration of the search query is set to `0` this function will be
 *   disabled unless a user explicitly enables windowed queries.
 * - If the timebase is invalid, it will show an error message and do nothing.
 *
 * If there's no search registered in `nm_query_window_current_search` or this is
 * a new search, it will reset the window and do the search.
 */
static bool windowed_query_from_query(const char *query, char *buf, size_t buflen)
{
  mutt_debug(LL_DEBUG2, "nm: %s\n", query);

  const bool c_nm_query_window_enable =
      cs_subset_bool(NeoMutt->sub, "nm_query_window_enable");
  const short c_nm_query_window_duration =
      cs_subset_number(NeoMutt->sub, "nm_query_window_duration");
  const short c_nm_query_window_current_position =
      cs_subset_number(NeoMutt->sub, "nm_query_window_current_position");
  const char *const c_nm_query_window_current_search =
      cs_subset_string(NeoMutt->sub, "nm_query_window_current_search");
  const char *const c_nm_query_window_timebase =
      cs_subset_string(NeoMutt->sub, "nm_query_window_timebase");

  /* if the query has changed, reset the window position */
  if (!c_nm_query_window_current_search ||
      (strcmp(query, c_nm_query_window_current_search) != 0))
  {
    query_window_reset();
  }

  enum NmWindowQueryRc rc = nm_windowed_query_from_query(
      buf, buflen, c_nm_query_window_enable, c_nm_query_window_duration,
      c_nm_query_window_current_position, c_nm_query_window_current_search,
      c_nm_query_window_timebase);

  switch (rc)
  {
    case NM_WINDOW_QUERY_SUCCESS:
    {
      mutt_debug(LL_DEBUG2, "nm: %s -> %s\n", query, buf);
      break;
    }
    case NM_WINDOW_QUERY_INVALID_DURATION:
    {
      query_window_reset();
      return false;
    }
    case NM_WINDOW_QUERY_INVALID_TIMEBASE:
    {
      mutt_message(
          // L10N: The values 'hour', 'day', 'week', 'month', 'year' are literal.
          //       They should not be translated.
          _("Invalid nm_query_window_timebase value (valid values are: "
            "hour, day, week, month, year)"));
      mutt_debug(LL_DEBUG2, "Invalid nm_query_window_timebase value\n");
      return false;
    }
  }

  return true;
}

/**
 * get_query_string - builds the notmuch vfolder search string
 * @param mdata Notmuch Mailbox data
 * @param window If true enable application of the window on the search string
 * @retval ptr  String containing a notmuch search query
 * @retval NULL None can be generated
 *
 * This function parses the internal representation of a search, and returns
 * a search query string ready to be fed to the notmuch API, given the search
 * is valid.
 *
 * @note The window parameter here is here to decide contextually whether we
 * want to return a search query with window applied (for the actual search
 * result in mailbox) or not (for the count in the sidebar). It is not aimed at
 * enabling/disabling the feature.
 */
static char *get_query_string(struct NmMboxData *mdata, bool window)
{
  mutt_debug(LL_DEBUG2, "nm: %s\n", window ? "true" : "false");

  if (!mdata)
    return NULL;
  if (mdata->db_query && !window)
    return mdata->db_query;

  const char *const c_nm_query_type =
      cs_subset_string(NeoMutt->sub, "nm_query_type");
  mdata->query_type = nm_string_to_query_type(c_nm_query_type); /* user's default */

  struct UrlQuery *item = NULL;
  STAILQ_FOREACH(item, &mdata->db_url->query_strings, entries)
  {
    if (!item->value || !item->name)
      continue;

    if (strcmp(item->name, "limit") == 0)
    {
      if (mutt_str_atoi(item->value, &mdata->db_limit))
        mutt_error(_("failed to parse notmuch limit: %s"), item->value);
    }
    else if (strcmp(item->name, "type") == 0)
      mdata->query_type = nm_string_to_query_type(item->value);
    else if (strcmp(item->name, "query") == 0)
      mdata->db_query = mutt_str_dup(item->value);
  }

  if (!mdata->db_query)
    return NULL;

  if (window)
  {
    char buf[1024];
    cs_subset_str_string_set(NeoMutt->sub, "nm_query_window_current_search",
                             mdata->db_query, NULL);

    /* if a date part is defined, do not apply windows (to avoid the risk of
     * having a non-intersected date frame). A good improvement would be to
     * accept if they intersect */
    if (!strstr(mdata->db_query, "date:") &&
        windowed_query_from_query(mdata->db_query, buf, sizeof(buf)))
    {
      mdata->db_query = mutt_str_dup(buf);
    }

    mutt_debug(LL_DEBUG2, "nm: query (windowed) '%s'\n", mdata->db_query);
  }
  else
    mutt_debug(LL_DEBUG2, "nm: query '%s'\n", mdata->db_query);

  return mdata->db_query;
}

/**
 * get_limit - Get the database limit
 * @param mdata Notmuch Mailbox data
 * @retval num Current limit
 */
static int get_limit(struct NmMboxData *mdata)
{
  return mdata ? mdata->db_limit : 0;
}

/**
 * apply_exclude_tags - Exclude the configured tags
 * @param query Notmuch query
 */
static void apply_exclude_tags(notmuch_query_t *query)
{
  const char *const c_nm_exclude_tags =
      cs_subset_string(NeoMutt->sub, "nm_exclude_tags");
  if (!c_nm_exclude_tags || !query)
    return;

  struct TagArray tags = nm_tag_str_to_tags(c_nm_exclude_tags);

  char **tag = NULL;
  ARRAY_FOREACH(tag, &tags.tags)
  {
    mutt_debug(LL_DEBUG2, "nm: query exclude tag '%s'\n", *tag);
    notmuch_query_add_tag_exclude(query, *tag);
  }

  notmuch_query_set_omit_excluded(query, 1);
  nm_tag_array_free(&tags);
}

/**
 * get_query - Create a new query
 * @param m        Mailbox
 * @param writable Should the query be updateable?
 * @retval ptr  Notmuch query
 * @retval NULL Error
 */
static notmuch_query_t *get_query(struct Mailbox *m, bool writable)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return NULL;

  notmuch_database_t *db = nm_db_get(m, writable);
  const char *str = get_query_string(mdata, true);

  if (!db || !str)
    goto err;

  notmuch_query_t *q = notmuch_query_create(db, str);
  if (!q)
    goto err;

  apply_exclude_tags(q);
  notmuch_query_set_sort(q, NOTMUCH_SORT_NEWEST_FIRST);
  mutt_debug(LL_DEBUG2, "nm: query successfully initialized (%s)\n", str);
  return q;
err:
  nm_db_release(m);
  return NULL;
}

/**
 * update_email_tags - Update the Email's tags from Notmuch
 * @param e   Email
 * @param msg Notmuch message
 * @retval 0 Success
 * @retval 1 Tags unchanged
 */
static int update_email_tags(struct Email *e, notmuch_message_t *msg)
{
  struct NmEmailData *edata = nm_edata_get(e);
  char *new_tags = NULL;
  char *old_tags = NULL;

  mutt_debug(LL_DEBUG2, "nm: tags update requested (%s)\n", edata->virtual_id);

  for (notmuch_tags_t *tags = notmuch_message_get_tags(msg);
       tags && notmuch_tags_valid(tags); notmuch_tags_move_to_next(tags))
  {
    const char *t = notmuch_tags_get(tags);
    if (!t || (*t == '\0'))
      continue;

    mutt_str_append_item(&new_tags, t, ' ');
  }

  old_tags = driver_tags_get(&e->tags);

  if (new_tags && old_tags && (strcmp(old_tags, new_tags) == 0))
  {
    FREE(&old_tags);
    FREE(&new_tags);
    mutt_debug(LL_DEBUG2, "nm: tags unchanged\n");
    return 1;
  }

  /* new version */
  driver_tags_replace(&e->tags, new_tags);
  FREE(&new_tags);

  new_tags = driver_tags_get_transformed(&e->tags);
  mutt_debug(LL_DEBUG2, "nm: new tags: '%s'\n", new_tags);
  FREE(&new_tags);

  new_tags = driver_tags_get(&e->tags);
  mutt_debug(LL_DEBUG2, "nm: new tag transforms: '%s'\n", new_tags);
  FREE(&new_tags);

  return 0;
}

/**
 * update_message_path - Set the path for a message
 * @param e    Email
 * @param path Path
 * @retval 0 Success
 * @retval 1 Failure
 */
static int update_message_path(struct Email *e, const char *path)
{
  struct NmEmailData *edata = nm_edata_get(e);

  mutt_debug(LL_DEBUG2, "nm: path update requested path=%s, (%s)\n", path, edata->virtual_id);

  char *p = strrchr(path, '/');
  if (p && ((p - path) > 3) &&
      (mutt_strn_equal(p - 3, "cur", 3) || mutt_strn_equal(p - 3, "new", 3) ||
       mutt_strn_equal(p - 3, "tmp", 3)))
  {
    edata->type = MUTT_MAILDIR;

    FREE(&e->path);
    FREE(&edata->folder);

    p -= 3; /* skip subfolder (e.g. "new") */
    e->old = mutt_str_startswith(p, "cur");
    e->path = mutt_str_dup(p);

    for (; (p > path) && (*(p - 1) == '/'); p--)
      ; // do nothing

    edata->folder = mutt_strn_dup(path, p - path);

    mutt_debug(LL_DEBUG2, "nm: folder='%s', file='%s'\n", edata->folder, e->path);
    return 0;
  }

  return 1;
}

/**
 * get_folder_from_path - Find an email's folder from its path
 * @param path Path
 * @retval ptr  Path string
 * @retval NULL Error
 */
static char *get_folder_from_path(const char *path)
{
  char *p = strrchr(path, '/');

  if (p && ((p - path) > 3) &&
      (mutt_strn_equal(p - 3, "cur", 3) || mutt_strn_equal(p - 3, "new", 3) ||
       mutt_strn_equal(p - 3, "tmp", 3)))
  {
    p -= 3;
    for (; (p > path) && (*(p - 1) == '/'); p--)
      ; // do nothing

    return mutt_strn_dup(path, p - path);
  }

  return NULL;
}

/**
 * nm2mutt_message_id - converts notmuch message Id to neomutt message Id
 * @param id Notmuch ID to convert
 * @retval ptr NeoMutt message ID
 *
 * Caller must free the NeoMutt Message ID
 */
static char *nm2mutt_message_id(const char *id)
{
  size_t sz;
  char *mid = NULL;

  if (!id)
    return NULL;
  sz = strlen(id) + 3;
  mid = mutt_mem_malloc(sz);

  snprintf(mid, sz, "<%s>", id);
  return mid;
}

/**
 * init_email - Set up an email's Notmuch data
 * @param e    Email
 * @param path Path to email
 * @param msg  Notmuch message
 * @retval  0 Success
 * @retval -1 Failure
 */
static int init_email(struct Email *e, const char *path, notmuch_message_t *msg)
{
  if (nm_edata_get(e))
    return 0;

  struct NmEmailData *edata = nm_edata_new();
  e->nm_edata = edata;

  /* Notmuch ensures that message Id exists (if not notmuch Notmuch will
   * generate an ID), so it's more safe than use neomutt Email->env->id */
  const char *id = notmuch_message_get_message_id(msg);
  edata->virtual_id = mutt_str_dup(id);

  mutt_debug(LL_DEBUG2, "nm: [e=%p, edata=%p] (%s)\n", (void *) e, (void *) edata, id);

  char *nm_msg_id = nm2mutt_message_id(id);
  if (!e->env->message_id)
  {
    e->env->message_id = nm_msg_id;
  }
  else if (!mutt_str_equal(e->env->message_id, nm_msg_id))
  {
    FREE(&e->env->message_id);
    e->env->message_id = nm_msg_id;
  }
  else
  {
    FREE(&nm_msg_id);
  }

  if (update_message_path(e, path) != 0)
    return -1;

  update_email_tags(e, msg);

  return 0;
}

/**
 * get_message_last_filename - Get a message's last filename
 * @param msg Notmuch message
 * @retval ptr  Filename
 * @retval NULL Error
 */
static const char *get_message_last_filename(notmuch_message_t *msg)
{
  const char *name = NULL;

  for (notmuch_filenames_t *ls = notmuch_message_get_filenames(msg);
       ls && notmuch_filenames_valid(ls); notmuch_filenames_move_to_next(ls))
  {
    name = notmuch_filenames_get(ls);
  }

  return name;
}

/**
 * progress_setup - Set up the Progress Bar
 * @param m Mailbox
 */
static void progress_setup(struct Mailbox *m)
{
  if (!m->verbose)
    return;

  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return;

  mdata->oldmsgcount = m->msg_count;
  mdata->ignmsgcount = 0;
  mdata->progress =
      progress_new(_("Reading messages..."), MUTT_PROGRESS_READ, mdata->oldmsgcount);
}

/**
 * nm_progress_update - Update the progress counter
 * @param m Mailbox
 */
static void nm_progress_update(struct Mailbox *m)
{
  struct NmMboxData *mdata = nm_mdata_get(m);

  if (!m->verbose || !mdata || !mdata->progress)
    return;

  progress_update(mdata->progress, m->msg_count + mdata->ignmsgcount, -1);
}

/**
 * get_mutt_email - Get the Email of a Notmuch message
 * @param m Mailbox
 * @param msg Notmuch message
 * @retval ptr  Email
 * @retval NULL Error
 */
static struct Email *get_mutt_email(struct Mailbox *m, notmuch_message_t *msg)
{
  if (!m || !msg)
    return NULL;

  const char *id = notmuch_message_get_message_id(msg);
  if (!id)
    return NULL;

  mutt_debug(LL_DEBUG2, "nm: neomutt email, id='%s'\n", id);

  if (!m->id_hash)
  {
    mutt_debug(LL_DEBUG2, "nm: init hash\n");
    m->id_hash = mutt_make_id_hash(m);
    if (!m->id_hash)
      return NULL;
  }

  char *mid = nm2mutt_message_id(id);
  mutt_debug(LL_DEBUG2, "nm: neomutt id='%s'\n", mid);

  struct Email *e = mutt_hash_find(m->id_hash, mid);
  FREE(&mid);
  return e;
}

/**
 * append_message - Associate a message
 * @param h     Header cache handle
 * @param m     Mailbox
 * @param q     Notmuch query
 * @param msg   Notmuch message
 * @param dedup De-duplicate results
 */
static void append_message(struct HeaderCache *h, struct Mailbox *m,
                           notmuch_query_t *q, notmuch_message_t *msg, bool dedup)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return;

  char *newpath = NULL;
  struct Email *e = NULL;

  /* deduplicate */
  if (dedup && get_mutt_email(m, msg))
  {
    mdata->ignmsgcount++;
    nm_progress_update(m);
    mutt_debug(LL_DEBUG2, "nm: ignore id=%s, already in the m\n",
               notmuch_message_get_message_id(msg));
    return;
  }

  const char *path = get_message_last_filename(msg);
  if (!path)
    return;

  mutt_debug(LL_DEBUG2, "nm: appending message, i=%d, id=%s, path=%s\n",
             m->msg_count, notmuch_message_get_message_id(msg), path);

  if (m->msg_count >= m->email_max)
  {
    mutt_debug(LL_DEBUG2, "nm: allocate mx memory\n");
    mx_alloc_memory(m);
  }

#ifdef USE_HCACHE
  e = mutt_hcache_fetch(h, path, mutt_str_len(path), 0).email;
  if (!e)
#endif
  {
    if (access(path, F_OK) == 0)
    {
      /* We pass is_old=false as argument here, but e->old will be updated later
       * by update_message_path() (called by init_email() below).  */
      e = maildir_parse_message(MUTT_MAILDIR, path, false, NULL);
    }
    else
    {
      /* maybe moved try find it... */
      char *folder = get_folder_from_path(path);

      if (folder)
      {
        FILE *fp = maildir_open_find_message(folder, path, &newpath);
        if (fp)
        {
          e = maildir_parse_stream(MUTT_MAILDIR, fp, newpath, false, NULL);
          mutt_file_fclose(&fp);

          mutt_debug(LL_DEBUG1, "nm: not up-to-date: %s -> %s\n", path, newpath);
        }
      }
      FREE(&folder);
    }

    if (!e)
    {
      mutt_debug(LL_DEBUG1, "nm: failed to parse message: %s\n", path);
      goto done;
    }

#ifdef USE_HCACHE
    mutt_hcache_store(h, newpath ? newpath : path,
                      mutt_str_len(newpath ? newpath : path), e, 0);
#endif
  }

  if (init_email(e, newpath ? newpath : path, msg) != 0)
  {
    email_free(&e);
    mutt_debug(LL_DEBUG1, "nm: failed to append email!\n");
    goto done;
  }

  e->active = true;
  e->index = m->msg_count;
  mailbox_size_add(m, e);
  m->emails[m->msg_count] = e;
  m->msg_count++;

  if (newpath)
  {
    /* remember that file has been moved -- nm_mbox_sync() will update the DB */
    struct NmEmailData *edata = nm_edata_get(e);
    if (edata)
    {
      mutt_debug(LL_DEBUG1, "nm: remember obsolete path: %s\n", path);
      edata->oldpath = mutt_str_dup(path);
    }
  }
  nm_progress_update(m);
done:
  FREE(&newpath);
}

/**
 * append_replies - add all the replies to a given messages into the display
 * @param h     Header cache handle
 * @param m     Mailbox
 * @param q     Notmuch query
 * @param top   Notmuch message
 * @param dedup De-duplicate the results
 *
 * Careful, this calls itself recursively to make sure we get everything.
 */
static void append_replies(struct HeaderCache *h, struct Mailbox *m,
                           notmuch_query_t *q, notmuch_message_t *top, bool dedup)
{
  notmuch_messages_t *msgs = NULL;

  for (msgs = notmuch_message_get_replies(top); notmuch_messages_valid(msgs);
       notmuch_messages_move_to_next(msgs))
  {
    notmuch_message_t *nm = notmuch_messages_get(msgs);
    append_message(h, m, q, nm, dedup);
    /* recurse through all the replies to this message too */
    append_replies(h, m, q, nm, dedup);
    notmuch_message_destroy(nm);
  }
}

/**
 * append_thread - add each top level reply in the thread
 * @param h      Header cache handle
 * @param m      Mailbox
 * @param q      Notmuch query
 * @param thread Notmuch thread
 * @param dedup  De-duplicate the results
 *
 * add each top level reply in the thread, and then add each reply to the top
 * level replies
 */
static void append_thread(struct HeaderCache *h, struct Mailbox *m,
                          notmuch_query_t *q, notmuch_thread_t *thread, bool dedup)
{
  notmuch_messages_t *msgs = NULL;

  for (msgs = notmuch_thread_get_toplevel_messages(thread);
       notmuch_messages_valid(msgs); notmuch_messages_move_to_next(msgs))
  {
    notmuch_message_t *nm = notmuch_messages_get(msgs);
    append_message(h, m, q, nm, dedup);
    append_replies(h, m, q, nm, dedup);
    notmuch_message_destroy(nm);
  }
}

/**
 * get_messages - load messages for a query
 * @param query Notmuch query
 * @retval ptr  Messages matching query
 * @retval NULL Error occurred
 *
 * This helper method is to be the single point for retrieving messages. It
 * handles version specific calls, which will make maintenance easier.
 */
static notmuch_messages_t *get_messages(notmuch_query_t *query)
{
  if (!query)
    return NULL;

  notmuch_messages_t *msgs = NULL;

#if LIBNOTMUCH_CHECK_VERSION(5, 0, 0)
  if (notmuch_query_search_messages(query, &msgs) != NOTMUCH_STATUS_SUCCESS)
    return NULL;
#elif LIBNOTMUCH_CHECK_VERSION(4, 3, 0)
  if (notmuch_query_search_messages_st(query, &msgs) != NOTMUCH_STATUS_SUCCESS)
    return NULL;
#else
  msgs = notmuch_query_search_messages(query);
#endif

  return msgs;
}

/**
 * read_mesgs_query - Search for matching messages
 * @param m     Mailbox
 * @param q     Notmuch query
 * @param dedup De-duplicate the results
 * @retval true  Success
 * @retval false Failure
 */
static bool read_mesgs_query(struct Mailbox *m, notmuch_query_t *q, bool dedup)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return false;

  int limit = get_limit(mdata);

  notmuch_messages_t *msgs = get_messages(q);

  if (!msgs)
    return false;

  struct HeaderCache *h = nm_hcache_open(m);

  for (; notmuch_messages_valid(msgs) && ((limit == 0) || (m->msg_count < limit));
       notmuch_messages_move_to_next(msgs))
  {
    if (SigInt)
    {
      nm_hcache_close(h);
      SigInt = false;
      return false;
    }
    notmuch_message_t *nm = notmuch_messages_get(msgs);
    append_message(h, m, q, nm, dedup);
    notmuch_message_destroy(nm);
  }

  nm_hcache_close(h);
  return true;
}

/**
 * get_threads - load threads for a query
 * @param query Notmuch query
 * @retval ptr Threads matching query
 * @retval NULL Error occurred
 *
 * This helper method is to be the single point for retrieving messages. It
 * handles version specific calls, which will make maintenance easier.
 */
static notmuch_threads_t *get_threads(notmuch_query_t *query)
{
  if (!query)
    return NULL;

  notmuch_threads_t *threads = NULL;
#if LIBNOTMUCH_CHECK_VERSION(5, 0, 0)
  if (notmuch_query_search_threads(query, &threads) != NOTMUCH_STATUS_SUCCESS)
    return false;
#elif LIBNOTMUCH_CHECK_VERSION(4, 3, 0)
  if (notmuch_query_search_threads_st(query, &threads) != NOTMUCH_STATUS_SUCCESS)
    return false;
#else
  threads = notmuch_query_search_threads(query);
#endif

  return threads;
}

/**
 * read_threads_query - Perform a query with threads
 * @param m     Mailbox
 * @param q     Query type
 * @param dedup Should the results be de-duped?
 * @param limit Maximum number of results
 * @retval true  Success
 * @retval false Failure
 */
static bool read_threads_query(struct Mailbox *m, notmuch_query_t *q, bool dedup, int limit)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return false;

  notmuch_threads_t *threads = get_threads(q);
  if (!threads)
    return false;

  struct HeaderCache *h = nm_hcache_open(m);

  for (; notmuch_threads_valid(threads) && ((limit == 0) || (m->msg_count < limit));
       notmuch_threads_move_to_next(threads))
  {
    if (SigInt)
    {
      nm_hcache_close(h);
      SigInt = false;
      return false;
    }
    notmuch_thread_t *thread = notmuch_threads_get(threads);
    append_thread(h, m, q, thread, dedup);
    notmuch_thread_destroy(thread);
  }

  nm_hcache_close(h);
  return true;
}

/**
 * get_nm_message - Find a Notmuch message
 * @param db  Notmuch database
 * @param e Email
 * @retval ptr Handle to the Notmuch message
 */
static notmuch_message_t *get_nm_message(notmuch_database_t *db, struct Email *e)
{
  notmuch_message_t *msg = NULL;
  char *id = email_get_id(e);

  mutt_debug(LL_DEBUG2, "nm: find message (%s)\n", id);

  if (id && db)
    notmuch_database_find_message(db, id, &msg);

  return msg;
}

/**
 * nm_message_has_tag - Does a message have this tag?
 * @param msg Notmuch message
 * @param tag Tag
 * @retval true It does
 */
static bool nm_message_has_tag(notmuch_message_t *msg, char *tag)
{
  const char *possible_match_tag = NULL;
  notmuch_tags_t *tags = NULL;

  for (tags = notmuch_message_get_tags(msg); notmuch_tags_valid(tags);
       notmuch_tags_move_to_next(tags))
  {
    possible_match_tag = notmuch_tags_get(tags);
    if (mutt_str_equal(possible_match_tag, tag))
    {
      return true;
    }
  }
  return false;
}

/**
 * sync_email_path_with_nm - Synchronize Neomutt's Email path with notmuch.
 * @param e Email in Neomutt
 * @param msg Email from notmuch
 */
static void sync_email_path_with_nm(struct Email *e, notmuch_message_t *msg)
{
  const char *new_file = get_message_last_filename(msg);
  char old_file[PATH_MAX];
  email_get_fullpath(e, old_file, sizeof(old_file));

  if (!mutt_str_equal(old_file, new_file))
    update_message_path(e, new_file);
}

/**
 * update_tags - Update the tags on a message
 * @param msg     Notmuch message
 * @param tag_str String of tags (space separated)
 * @retval  0 Success
 * @retval -1 Failure
 */
static int update_tags(notmuch_message_t *msg, const char *tag_str)
{
  if (!tag_str)
    return -1;

  notmuch_message_freeze(msg);

  struct TagArray tags = nm_tag_str_to_tags(tag_str);
  char **tag_elem = NULL;
  ARRAY_FOREACH(tag_elem, &tags.tags)
  {
    char *tag = *tag_elem;

    if (tag[0] == '-')
    {
      mutt_debug(LL_DEBUG1, "nm: remove tag: '%s'\n", tag + 1);
      notmuch_message_remove_tag(msg, tag + 1);
    }
    else if (tag[0] == '!')
    {
      mutt_debug(LL_DEBUG1, "nm: toggle tag: '%s'\n", tag + 1);
      if (nm_message_has_tag(msg, tag + 1))
      {
        notmuch_message_remove_tag(msg, tag + 1);
      }
      else
      {
        notmuch_message_add_tag(msg, tag + 1);
      }
    }
    else
    {
      mutt_debug(LL_DEBUG1, "nm: add tag: '%s'\n", (tag[0] == '+') ? tag + 1 : tag);
      notmuch_message_add_tag(msg, (tag[0] == '+') ? tag + 1 : tag);
    }
  }

  notmuch_message_thaw(msg);
  nm_tag_array_free(&tags);

  return 0;
}

/**
 * update_email_flags - Update the Email's flags
 * @param m       Mailbox
 * @param e       Email
 * @param tag_str String of tags (space separated)
 * @retval  0 Success
 * @retval -1 Failure
 *
 * TODO: join update_email_tags and update_email_flags, which are given an
 * array of tags.
 */
static int update_email_flags(struct Mailbox *m, struct Email *e, const char *tag_str)
{
  if (!tag_str)
    return -1;

  const char *const c_nm_unread_tag =
      cs_subset_string(NeoMutt->sub, "nm_unread_tag");
  const char *const c_nm_replied_tag =
      cs_subset_string(NeoMutt->sub, "nm_replied_tag");
  const char *const c_nm_flagged_tag =
      cs_subset_string(NeoMutt->sub, "nm_flagged_tag");

  struct TagArray tags = nm_tag_str_to_tags(tag_str);
  char **tag_elem = NULL;
  ARRAY_FOREACH(tag_elem, &tags.tags)
  {
    char *tag = *tag_elem;

    if (tag[0] == '-')
    {
      tag++;
      if (strcmp(tag, c_nm_unread_tag) == 0)
        mutt_set_flag(m, e, MUTT_READ, true);
      else if (strcmp(tag, c_nm_replied_tag) == 0)
        mutt_set_flag(m, e, MUTT_REPLIED, false);
      else if (strcmp(tag, c_nm_flagged_tag) == 0)
        mutt_set_flag(m, e, MUTT_FLAG, false);
    }
    else
    {
      tag = (tag[0] == '+') ? tag + 1 : tag;
      if (strcmp(tag, c_nm_unread_tag) == 0)
        mutt_set_flag(m, e, MUTT_READ, false);
      else if (strcmp(tag, c_nm_replied_tag) == 0)
        mutt_set_flag(m, e, MUTT_REPLIED, true);
      else if (strcmp(tag, c_nm_flagged_tag) == 0)
        mutt_set_flag(m, e, MUTT_FLAG, true);
    }
  }

  nm_tag_array_free(&tags);

  return 0;
}

/**
 * rename_maildir_filename - Rename a Maildir file
 * @param old    Old path
 * @param buf    Buffer for new path
 * @param buflen Length of buffer
 * @param e      Email
 * @retval  0 Success, renamed
 * @retval  1 Success, no change
 * @retval -1 Failure
 */
static int rename_maildir_filename(const char *old, char *buf, size_t buflen, struct Email *e)
{
  char filename[PATH_MAX];
  char suffix[PATH_MAX];
  char folder[PATH_MAX];

  mutt_str_copy(folder, old, sizeof(folder));
  char *p = strrchr(folder, '/');
  if (p)
  {
    *p = '\0';
    p++;
  }
  else
    p = folder;

  mutt_str_copy(filename, p, sizeof(filename));

  /* remove (new,cur,...) from folder path */
  p = strrchr(folder, '/');
  if (p)
    *p = '\0';

  /* remove old flags from filename */
  p = strchr(filename, ':');
  if (p)
    *p = '\0';

  /* compose new flags */
  maildir_gen_flags(suffix, sizeof(suffix), e);

  snprintf(buf, buflen, "%s/%s/%s%s", folder,
           (e->read || e->old) ? "cur" : "new", filename, suffix);

  if (strcmp(old, buf) == 0)
    return 1;

  if (rename(old, buf) != 0)
  {
    mutt_debug(LL_DEBUG1, "nm: rename(2) failed %s -> %s\n", old, buf);
    return -1;
  }

  return 0;
}

/**
 * remove_filename - Delete a file
 * @param m Mailbox
 * @param path Path of file
 * @retval  0 Success
 * @retval -1 Failure
 */
static int remove_filename(struct Mailbox *m, const char *path)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return -1;

  mutt_debug(LL_DEBUG2, "nm: remove filename '%s'\n", path);

  notmuch_database_t *db = nm_db_get(m, true);
  if (!db)
    return -1;

  notmuch_message_t *msg = NULL;
  notmuch_status_t st = notmuch_database_find_message_by_filename(db, path, &msg);
  if (st || !msg)
    return -1;

  int trans = nm_db_trans_begin(m);
  if (trans < 0)
    return -1;

  /* note that unlink() is probably unnecessary here, it's already removed
   * by mh_sync_mailbox_message(), but for sure...  */
  notmuch_filenames_t *ls = NULL;
  st = notmuch_database_remove_message(db, path);
  switch (st)
  {
    case NOTMUCH_STATUS_SUCCESS:
      mutt_debug(LL_DEBUG2, "nm: remove success, call unlink\n");
      unlink(path);
      break;
    case NOTMUCH_STATUS_DUPLICATE_MESSAGE_ID:
      mutt_debug(LL_DEBUG2, "nm: remove success (duplicate), call unlink\n");
      unlink(path);
      for (ls = notmuch_message_get_filenames(msg);
           ls && notmuch_filenames_valid(ls); notmuch_filenames_move_to_next(ls))
      {
        path = notmuch_filenames_get(ls);

        mutt_debug(LL_DEBUG2, "nm: remove duplicate: '%s'\n", path);
        unlink(path);
        notmuch_database_remove_message(db, path);
      }
      break;
    default:
      mutt_debug(LL_DEBUG1, "nm: failed to remove '%s' [st=%d]\n", path, (int) st);
      break;
  }

  notmuch_message_destroy(msg);
  if (trans)
    nm_db_trans_end(m);
  return 0;
}

/**
 * rename_filename - Rename the file
 * @param m        Notmuch Mailbox data
 * @param old_file Old filename
 * @param new_file New filename
 * @param e        Email
 * @retval  0      Success
 * @retval -1      Failure
 */
static int rename_filename(struct Mailbox *m, const char *old_file,
                           const char *new_file, struct Email *e)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return -1;

  notmuch_database_t *db = nm_db_get(m, true);
  if (!db || !new_file || !old_file || (access(new_file, F_OK) != 0))
    return -1;

  int rc = -1;
  notmuch_status_t st;
  notmuch_filenames_t *ls = NULL;
  notmuch_message_t *msg = NULL;

  mutt_debug(LL_DEBUG1, "nm: rename filename, %s -> %s\n", old_file, new_file);
  int trans = nm_db_trans_begin(m);
  if (trans < 0)
    return -1;

  mutt_debug(LL_DEBUG2, "nm: rename: add '%s'\n", new_file);
#ifdef HAVE_NOTMUCH_DATABASE_INDEX_FILE
  st = notmuch_database_index_file(db, new_file, NULL, &msg);
#else
  st = notmuch_database_add_message(db, new_file, &msg);
#endif

  if ((st != NOTMUCH_STATUS_SUCCESS) && (st != NOTMUCH_STATUS_DUPLICATE_MESSAGE_ID))
  {
    mutt_debug(LL_DEBUG1, "nm: failed to add '%s' [st=%d]\n", new_file, (int) st);
    goto done;
  }

  mutt_debug(LL_DEBUG2, "nm: rename: rem '%s'\n", old_file);
  st = notmuch_database_remove_message(db, old_file);
  switch (st)
  {
    case NOTMUCH_STATUS_SUCCESS:
      break;
    case NOTMUCH_STATUS_DUPLICATE_MESSAGE_ID:
      mutt_debug(LL_DEBUG2, "nm: rename: syncing duplicate filename\n");
      notmuch_message_destroy(msg);
      msg = NULL;
      notmuch_database_find_message_by_filename(db, new_file, &msg);

      for (ls = notmuch_message_get_filenames(msg);
           msg && ls && notmuch_filenames_valid(ls); notmuch_filenames_move_to_next(ls))
      {
        const char *path = notmuch_filenames_get(ls);
        char newpath[PATH_MAX];

        if (strcmp(new_file, path) == 0)
          continue;

        mutt_debug(LL_DEBUG2, "nm: rename: syncing duplicate: %s\n", path);

        if (rename_maildir_filename(path, newpath, sizeof(newpath), e) == 0)
        {
          mutt_debug(LL_DEBUG2, "nm: rename dup %s -> %s\n", path, newpath);
          notmuch_database_remove_message(db, path);
#ifdef HAVE_NOTMUCH_DATABASE_INDEX_FILE
          notmuch_database_index_file(db, newpath, NULL, NULL);
#else
          notmuch_database_add_message(db, newpath, NULL);
#endif
        }
      }
      notmuch_message_destroy(msg);
      msg = NULL;
      notmuch_database_find_message_by_filename(db, new_file, &msg);
      st = NOTMUCH_STATUS_SUCCESS;
      break;
    default:
      mutt_debug(LL_DEBUG1, "nm: failed to remove '%s' [st=%d]\n", old_file, (int) st);
      break;
  }

  if ((st == NOTMUCH_STATUS_SUCCESS) && e && msg)
  {
    notmuch_message_maildir_flags_to_tags(msg);
    update_email_tags(e, msg);

    char *tags = driver_tags_get(&e->tags);
    update_tags(msg, tags);
    FREE(&tags);
  }

  rc = 0;
done:
  if (msg)
    notmuch_message_destroy(msg);
  if (trans)
    nm_db_trans_end(m);
  return rc;
}

/**
 * count_query - Count the results of a query
 * @param db    Notmuch database
 * @param qstr  Query to execute
 * @param limit Maximum number of results
 * @retval num Number of results
 */
static unsigned int count_query(notmuch_database_t *db, const char *qstr, int limit)
{
  notmuch_query_t *q = notmuch_query_create(db, qstr);
  if (!q)
    return 0;

  unsigned int res = 0;

  apply_exclude_tags(q);
#if LIBNOTMUCH_CHECK_VERSION(5, 0, 0)
  if (notmuch_query_count_messages(q, &res) != NOTMUCH_STATUS_SUCCESS)
    res = 0; /* may not be defined on error */
#elif LIBNOTMUCH_CHECK_VERSION(4, 3, 0)
  if (notmuch_query_count_messages_st(q, &res) != NOTMUCH_STATUS_SUCCESS)
    res = 0; /* may not be defined on error */
#else
  res = notmuch_query_count_messages(q);
#endif
  notmuch_query_destroy(q);
  mutt_debug(LL_DEBUG1, "nm: count '%s', result=%d\n", qstr, res);

  if ((limit > 0) && (res > limit))
    res = limit;

  return res;
}

/**
 * nm_email_get_folder - Get the folder for a Email
 * @param e Email
 * @retval ptr  Folder containing email
 * @retval NULL Error
 */
char *nm_email_get_folder(struct Email *e)
{
  struct NmEmailData *edata = nm_edata_get(e);
  if (!edata)
    return NULL;

  return edata->folder;
}

/**
 * nm_email_get_folder_rel_db - Get the folder for a Email from the same level as the notmuch database
 * @param m Mailbox containing Email
 * @param e Email
 * @retval ptr  Folder containing email from the same level as the notmuch db
 * @retval NULL Error
 *
 * Instead of returning a path like /var/mail/account/Inbox, this returns
 * account/Inbox. If wanting the full path, use nm_email_get_folder().
 */
char *nm_email_get_folder_rel_db(struct Mailbox *m, struct Email *e)
{
  char *full_folder = nm_email_get_folder(e);
  if (!full_folder)
    return NULL;

  const char *db_path = nm_db_get_filename(m);
  if (!db_path)
    return NULL;

  return full_folder + strlen(db_path);
}

/**
 * nm_read_entire_thread - Get the entire thread of an email
 * @param m Mailbox
 * @param e   Email
 * @retval  0 Success
 * @retval -1 Failure
 */
int nm_read_entire_thread(struct Mailbox *m, struct Email *e)
{
  if (!m)
    return -1;

  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return -1;

  notmuch_query_t *q = NULL;
  notmuch_database_t *db = NULL;
  notmuch_message_t *msg = NULL;
  int rc = -1;

  if (!(db = nm_db_get(m, false)) || !(msg = get_nm_message(db, e)))
    goto done;

  mutt_debug(LL_DEBUG1, "nm: reading entire-thread messages...[current count=%d]\n",
             m->msg_count);

  progress_setup(m);
  const char *id = notmuch_message_get_thread_id(msg);
  if (!id)
    goto done;

  char *qstr = NULL;
  mutt_str_append_item(&qstr, "thread:", '\0');
  mutt_str_append_item(&qstr, id, '\0');

  q = notmuch_query_create(db, qstr);
  FREE(&qstr);
  if (!q)
    goto done;
  apply_exclude_tags(q);
  notmuch_query_set_sort(q, NOTMUCH_SORT_NEWEST_FIRST);

  read_threads_query(m, q, true, 0);
  m->mtime.tv_sec = mutt_date_epoch();
  m->mtime.tv_nsec = 0;
  rc = 0;

  if (m->msg_count > mdata->oldmsgcount)
    mailbox_changed(m, NT_MAILBOX_INVALID);
done:
  if (q)
    notmuch_query_destroy(q);

  nm_db_release(m);

  if (m->msg_count == mdata->oldmsgcount)
    mutt_message(_("No more messages in the thread"));

  mdata->oldmsgcount = 0;
  mutt_debug(LL_DEBUG1, "nm: reading entire-thread messages... done [rc=%d, count=%d]\n",
             rc, m->msg_count);
  progress_free(&mdata->progress);
  return rc;
}

/**
 * nm_url_from_query - Turn a query into a URL
 * @param m      Mailbox
 * @param buf    Buffer for URL
 * @param buflen Length of buffer
 * @retval ptr  Query as a URL
 * @retval NULL Error
 */
char *nm_url_from_query(struct Mailbox *m, char *buf, size_t buflen)
{
  mutt_debug(LL_DEBUG2, "(%s)\n", buf);
  struct NmMboxData *mdata = nm_mdata_get(m);
  char url[PATH_MAX + 1024 + 32]; /* path to DB + query + URL "decoration" */
  int added;
  bool using_default_data = false;

  // No existing data. Try to get a default NmMboxData.
  if (!mdata)
  {
    mdata = nm_get_default_data();

    // Failed to get default data.
    if (!mdata)
      return NULL;

    using_default_data = true;
  }

  mdata->query_type = nm_parse_type_from_query(buf);

  const short c_nm_db_limit = cs_subset_number(NeoMutt->sub, "nm_db_limit");
  if (get_limit(mdata) == c_nm_db_limit)
  {
    added = snprintf(url, sizeof(url), "%s%s?type=%s&query=", NmUrlProtocol,
                     nm_db_get_filename(m), nm_query_type_to_string(mdata->query_type));
  }
  else
  {
    added = snprintf(url, sizeof(url), "%s%s?type=%s&limit=%d&query=", NmUrlProtocol,
                     nm_db_get_filename(m),
                     nm_query_type_to_string(mdata->query_type), get_limit(mdata));
  }

  if (added >= sizeof(url))
  {
    // snprintf output was truncated, so can't create URL
    return NULL;
  }

  url_pct_encode(&url[added], sizeof(url) - added, buf);

  mutt_str_copy(buf, url, buflen);
  buf[buflen - 1] = '\0';

  if (using_default_data)
    nm_mdata_free((void **) &mdata);

  mutt_debug(LL_DEBUG1, "nm: url from query '%s'\n", buf);
  return buf;
}

/**
 * nm_query_window_forward - Function to move the current search window forward in time
 *
 * Updates `nm_query_window_current_position` by decrementing it by 1, or does nothing
 * if the current window already is set to 0.
 *
 * The lower the value of `nm_query_window_current_position` is, the more recent the
 * result will be.
 */
void nm_query_window_forward(void)
{
  const short c_nm_query_window_current_position =
      cs_subset_number(NeoMutt->sub, "nm_query_window_current_position");
  if (c_nm_query_window_current_position != 0)
  {
    cs_subset_str_native_set(NeoMutt->sub, "nm_query_window_current_position",
                             c_nm_query_window_current_position - 1, NULL);
  }

  mutt_debug(LL_DEBUG2, "(%d)\n", c_nm_query_window_current_position - 1);
}

/**
 * nm_query_window_backward - Function to move the current search window backward in time
 *
 * Updates `nm_query_window_current_position` by incrementing it by 1
 *
 * The higher the value of `nm_query_window_current_position` is, the less recent the
 * result will be.
 */
void nm_query_window_backward(void)
{
  const short c_nm_query_window_current_position =
      cs_subset_number(NeoMutt->sub, "nm_query_window_current_position");
  cs_subset_str_native_set(NeoMutt->sub, "nm_query_window_current_position",
                           c_nm_query_window_current_position + 1, NULL);
  mutt_debug(LL_DEBUG2, "(%d)\n", c_nm_query_window_current_position + 1);
}

/**
 * nm_message_is_still_queried - Is a message still visible in the query?
 * @param m Mailbox
 * @param e Email
 * @retval true Message is still in query
 */
bool nm_message_is_still_queried(struct Mailbox *m, struct Email *e)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  notmuch_database_t *db = nm_db_get(m, false);
  char *orig_str = get_query_string(mdata, true);

  if (!db || !orig_str)
    return false;

  char *new_str = NULL;
  bool rc = false;
  if (mutt_str_asprintf(&new_str, "id:%s and (%s)", email_get_id(e), orig_str) < 0)
    return false;

  mutt_debug(LL_DEBUG2, "nm: checking if message is still queried: %s\n", new_str);

  notmuch_query_t *q = notmuch_query_create(db, new_str);

  switch (mdata->query_type)
  {
    case NM_QUERY_TYPE_UNKNOWN: // UNKNOWN should never occur, but MESGS is default
    case NM_QUERY_TYPE_MESGS:
    {
      notmuch_messages_t *messages = get_messages(q);

      if (!messages)
        return false;

      rc = notmuch_messages_valid(messages);
      notmuch_messages_destroy(messages);
      break;
    }
    case NM_QUERY_TYPE_THREADS:
    {
      notmuch_threads_t *threads = get_threads(q);

      if (!threads)
        return false;

      rc = notmuch_threads_valid(threads);
      notmuch_threads_destroy(threads);
      break;
    }
  }

  notmuch_query_destroy(q);

  mutt_debug(LL_DEBUG2, "nm: checking if message is still queried: %s = %s\n",
             new_str, rc ? "true" : "false");

  return rc;
}

/**
 * nm_update_filename - Change the filename
 * @param m        Mailbox
 * @param old_file Old filename
 * @param new_file New filename
 * @param e        Email
 * @retval  0      Success
 * @retval -1      Failure
 */
int nm_update_filename(struct Mailbox *m, const char *old_file,
                       const char *new_file, struct Email *e)
{
  char buf[PATH_MAX];
  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata || !new_file)
    return -1;

  if (!old_file && nm_edata_get(e))
  {
    email_get_fullpath(e, buf, sizeof(buf));
    old_file = buf;
  }

  int rc = rename_filename(m, old_file, new_file, e);

  nm_db_release(m);
  m->mtime.tv_sec = mutt_date_epoch();
  m->mtime.tv_nsec = 0;
  return rc;
}

/**
 * nm_mbox_check_stats - Check the Mailbox statistics - Implements MxOps::mbox_check_stats()
 */
static enum MxStatus nm_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  struct UrlQuery *item = NULL;
  struct Url *url = NULL;
  const char *db_filename = NULL;
  char *db_query = NULL;
  notmuch_database_t *db = NULL;
  enum MxStatus rc = MX_STATUS_ERROR;
  const short c_nm_db_limit = cs_subset_number(NeoMutt->sub, "nm_db_limit");
  int limit = c_nm_db_limit;
  mutt_debug(LL_DEBUG1, "nm: count\n");

  url = url_parse(mailbox_path(m));
  if (!url)
  {
    mutt_error(_("failed to parse notmuch url: %s"), mailbox_path(m));
    goto done;
  }

  STAILQ_FOREACH(item, &url->query_strings, entries)
  {
    if (item->value && (strcmp(item->name, "query") == 0))
      db_query = item->value;
    else if (item->value && (strcmp(item->name, "limit") == 0))
    {
      // Try to parse the limit
      if (mutt_str_atoi(item->value, &limit) != 0)
      {
        mutt_error(_("failed to parse limit: %s"), item->value);
        goto done;
      }
    }
  }

  if (!db_query)
    goto done;

  db_filename = url->path;
  if (!db_filename)
  {
    const char *const c_nm_default_url =
        cs_subset_string(NeoMutt->sub, "nm_default_url");
    const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
    if (c_nm_default_url)
    {
      if (nm_path_probe(c_nm_default_url, NULL) == MUTT_NOTMUCH)
        db_filename = c_nm_default_url + NmUrlProtocolLen;
      else
        db_filename = c_nm_default_url;
    }
    else if (c_folder)
      db_filename = c_folder;
  }

  /* don't be verbose about connection, as we're called from
   * sidebar/mailbox very often */
  db = nm_db_do_open(db_filename, false, false);
  if (!db)
    goto done;

  /* all emails */
  m->msg_count = count_query(db, db_query, limit);
  while (m->email_max < m->msg_count)
    mx_alloc_memory(m);

  // holder variable for extending query to unread/flagged
  char *qstr = NULL;

  // unread messages
  const char *const c_nm_unread_tag =
      cs_subset_string(NeoMutt->sub, "nm_unread_tag");
  mutt_str_asprintf(&qstr, "( %s ) tag:%s", db_query, c_nm_unread_tag);
  m->msg_unread = count_query(db, qstr, limit);
  FREE(&qstr);

  // flagged messages
  const char *const c_nm_flagged_tag =
      cs_subset_string(NeoMutt->sub, "nm_flagged_tag");
  mutt_str_asprintf(&qstr, "( %s ) tag:%s", db_query, c_nm_flagged_tag);
  m->msg_flagged = count_query(db, qstr, limit);
  FREE(&qstr);

  rc = (m->msg_new > 0) ? MX_STATUS_NEW_MAIL : MX_STATUS_OK;
done:
  if (db)
  {
    nm_db_free(db);
    mutt_debug(LL_DEBUG1, "nm: count close DB\n");
  }
  url_free(&url);

  mutt_debug(LL_DEBUG1, "nm: count done [rc=%d]\n", rc);
  return rc;
}

/**
 * get_default_mailbox - Get Mailbox for notmuch without any parameters.
 * @retval ptr Mailbox pointer.
 */
static struct Mailbox *get_default_mailbox(void)
{
  // Create a new notmuch mailbox from scratch and add plumbing for DB access.
  char *default_url = nm_get_default_url();
  struct Mailbox *m = mx_path_resolve(default_url);

  FREE(&default_url);

  // These are no-ops for an initialized mailbox.
  init_mailbox(m);
  mx_mbox_ac_link(m);

  return m;
}

/**
 * nm_record_message - Add a message to the Notmuch database
 * @param m    Mailbox
 * @param path Path of the email
 * @param e    Email
 * @retval  0 Success
 * @retval -1 Failure
 */
int nm_record_message(struct Mailbox *m, char *path, struct Email *e)
{
  notmuch_database_t *db = NULL;
  notmuch_status_t st;
  notmuch_message_t *msg = NULL;
  int rc = -1;

  struct NmMboxData *mdata = nm_mdata_get(m);

  // If no notmuch data, fall back to the default mailbox.
  //
  // IMPORTANT: DO NOT FREE THIS MAILBOX. Two reasons:
  // 1) If user has default mailbox in config, we'll be removing it. That's not
  //    good program behavior!
  // 2) If not in user's config, keep mailbox around for future nm_record calls.
  //    It saves NeoMutt from allocating/deallocating repeatedly.
  if (!mdata)
  {
    mutt_debug(LL_DEBUG1, "nm: non-nm mailbox. trying the default nm mailbox.");
    m = get_default_mailbox();
    mdata = nm_mdata_get(m);
  }

  if (!path || !mdata || (access(path, F_OK) != 0))
    return 0;
  db = nm_db_get(m, true);
  if (!db)
    return -1;

  mutt_debug(LL_DEBUG1, "nm: record message: %s\n", path);
  int trans = nm_db_trans_begin(m);
  if (trans < 0)
    goto done;

#ifdef HAVE_NOTMUCH_DATABASE_INDEX_FILE
  st = notmuch_database_index_file(db, path, NULL, &msg);
#else
  st = notmuch_database_add_message(db, path, &msg);
#endif

  if ((st != NOTMUCH_STATUS_SUCCESS) && (st != NOTMUCH_STATUS_DUPLICATE_MESSAGE_ID))
  {
    mutt_debug(LL_DEBUG1, "nm: failed to add '%s' [st=%d]\n", path, (int) st);
    goto done;
  }

  if ((st == NOTMUCH_STATUS_SUCCESS) && msg)
  {
    notmuch_message_maildir_flags_to_tags(msg);
    if (e)
    {
      char *tags = driver_tags_get(&e->tags);
      update_tags(msg, tags);
      FREE(&tags);
    }
    const char *const c_nm_record_tags =
        cs_subset_string(NeoMutt->sub, "nm_record_tags");
    if (c_nm_record_tags)
      update_tags(msg, c_nm_record_tags);
  }

  rc = 0;
done:
  if (msg)
    notmuch_message_destroy(msg);
  if (trans == 1)
    nm_db_trans_end(m);

  nm_db_release(m);

  return rc;
}

/**
 * nm_get_all_tags - Fill a list with all notmuch tags
 * @param[in]  m         Mailbox
 * @param[out] tag_list  List of tags
 * @param[out] tag_count Number of tags
 * @retval  0 Success
 * @retval -1 Failure
 *
 * If tag_list is NULL, just count the tags.
 */
int nm_get_all_tags(struct Mailbox *m, char **tag_list, int *tag_count)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return -1;

  notmuch_database_t *db = NULL;
  notmuch_tags_t *tags = NULL;
  const char *tag = NULL;
  int rc = -1;

  if (!(db = nm_db_get(m, false)) || !(tags = notmuch_database_get_all_tags(db)))
    goto done;

  *tag_count = 0;
  mutt_debug(LL_DEBUG1, "nm: get all tags\n");

  while (notmuch_tags_valid(tags))
  {
    tag = notmuch_tags_get(tags);
    /* Skip empty string */
    if (*tag)
    {
      if (tag_list)
        tag_list[*tag_count] = mutt_str_dup(tag);
      (*tag_count)++;
    }
    notmuch_tags_move_to_next(tags);
  }

  rc = 0;
done:
  if (tags)
    notmuch_tags_destroy(tags);

  nm_db_release(m);

  mutt_debug(LL_DEBUG1, "nm: get all tags done [rc=%d tag_count=%u]\n", rc, *tag_count);
  return rc;
}

/**
 * nm_ac_owns_path - Check whether an Account owns a Mailbox path - Implements MxOps::ac_owns_path()
 */
static bool nm_ac_owns_path(struct Account *a, const char *path)
{
  return true;
}

/**
 * nm_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add()
 */
static bool nm_ac_add(struct Account *a, struct Mailbox *m)
{
  if (a->adata)
    return true;

  struct NmAccountData *adata = nm_adata_new();
  a->adata = adata;
  a->adata_free = nm_adata_free;

  return true;
}

/**
 * nm_mbox_open - Open a Mailbox - Implements MxOps::mbox_open()
 */
static enum MxOpenReturns nm_mbox_open(struct Mailbox *m)
{
  if (init_mailbox(m) != 0)
    return MX_OPEN_ERROR;

  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return MX_OPEN_ERROR;

  mutt_debug(LL_DEBUG1, "nm: reading messages...[current count=%d]\n", m->msg_count);

  progress_setup(m);
  enum MxOpenReturns rc = MX_OPEN_ERROR;

  notmuch_query_t *q = get_query(m, false);
  if (q)
  {
    rc = MX_OPEN_OK;
    switch (mdata->query_type)
    {
      case NM_QUERY_TYPE_UNKNOWN: // UNKNOWN should never occur, but MESGS is default
      case NM_QUERY_TYPE_MESGS:
        if (!read_mesgs_query(m, q, false))
          rc = MX_OPEN_ABORT;
        break;
      case NM_QUERY_TYPE_THREADS:
        if (!read_threads_query(m, q, false, get_limit(mdata)))
          rc = MX_OPEN_ABORT;
        break;
    }
    notmuch_query_destroy(q);
  }

  nm_db_release(m);

  m->mtime.tv_sec = mutt_date_epoch();
  m->mtime.tv_nsec = 0;

  mdata->oldmsgcount = 0;

  mutt_debug(LL_DEBUG1, "nm: reading messages... done [rc=%d, count=%d]\n", rc, m->msg_count);
  progress_free(&mdata->progress);
  return rc;
}

/**
 * nm_mbox_check - Check for new mail - Implements MxOps::mbox_check()
 * @param m Mailbox
 * @retval enum #MxStatus
 */
static enum MxStatus nm_mbox_check(struct Mailbox *m)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  time_t mtime = 0;
  if (!mdata || (nm_db_get_mtime(m, &mtime) != 0))
    return MX_STATUS_ERROR;

  int new_flags = 0;
  bool occult = false;

  if (m->mtime.tv_sec >= mtime)
  {
    mutt_debug(LL_DEBUG2, "nm: check unnecessary (db=%lu mailbox=%lu)\n", mtime,
               m->mtime.tv_sec);
    return MX_STATUS_OK;
  }

  mutt_debug(LL_DEBUG1, "nm: checking (db=%lu mailbox=%lu)\n", mtime, m->mtime.tv_sec);

  notmuch_query_t *q = get_query(m, false);
  if (!q)
    goto done;

  mutt_debug(LL_DEBUG1, "nm: start checking (count=%d)\n", m->msg_count);
  mdata->oldmsgcount = m->msg_count;

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    e->active = false;
  }

  int limit = get_limit(mdata);

  notmuch_messages_t *msgs = get_messages(q);

  // TODO: Analyze impact of removing this version guard.
#if LIBNOTMUCH_CHECK_VERSION(5, 0, 0)
  if (!msgs)
    return MX_STATUS_OK;
#elif LIBNOTMUCH_CHECK_VERSION(4, 3, 0)
  if (!msgs)
    goto done;
#endif

  struct HeaderCache *h = nm_hcache_open(m);

  for (int i = 0; notmuch_messages_valid(msgs) && ((limit == 0) || (i < limit));
       notmuch_messages_move_to_next(msgs), i++)
  {
    notmuch_message_t *msg = notmuch_messages_get(msgs);
    struct Email *e = get_mutt_email(m, msg);

    if (!e)
    {
      /* new email */
      append_message(h, m, NULL, msg, false);
      notmuch_message_destroy(msg);
      continue;
    }

    /* message already exists, merge flags */
    e->active = true;

    /* Check to see if the message has moved to a different subdirectory.
     * If so, update the associated filename.  */
    const char *new_file = get_message_last_filename(msg);
    char old_file[PATH_MAX];
    email_get_fullpath(e, old_file, sizeof(old_file));

    if (!mutt_str_equal(old_file, new_file))
      update_message_path(e, new_file);

    if (!e->changed)
    {
      /* if the user hasn't modified the flags on this message, update the
       * flags we just detected.  */
      struct Email e_tmp = { 0 };
      e_tmp.edata = maildir_edata_new();
      maildir_parse_flags(&e_tmp, new_file);
      maildir_update_flags(m, e, &e_tmp);
      maildir_edata_free(&e_tmp.edata);
    }

    if (update_email_tags(e, msg) == 0)
      new_flags++;

    notmuch_message_destroy(msg);
  }

  nm_hcache_close(h);

  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    if (!e->active)
    {
      occult = true;
      break;
    }
  }

  if (m->msg_count > mdata->oldmsgcount)
    mailbox_changed(m, NT_MAILBOX_INVALID);
done:
  if (q)
    notmuch_query_destroy(q);

  nm_db_release(m);

  m->mtime.tv_sec = mutt_date_epoch();
  m->mtime.tv_nsec = 0;

  mutt_debug(LL_DEBUG1, "nm: ... check done [count=%d, new_flags=%d, occult=%d]\n",
             m->msg_count, new_flags, occult);

  if (occult)
    return MX_STATUS_REOPENED;
  if (m->msg_count > mdata->oldmsgcount)
    return MX_STATUS_NEW_MAIL;
  if (new_flags)
    return MX_STATUS_FLAGS;
  return MX_STATUS_OK;
}

/**
 * nm_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync()
 */
static enum MxStatus nm_mbox_sync(struct Mailbox *m)
{
  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return MX_STATUS_ERROR;

  enum MxStatus rc = MX_STATUS_OK;
  struct Progress *progress = NULL;
  char *url = mutt_str_dup(mailbox_path(m));
  bool changed = false;

  mutt_debug(LL_DEBUG1, "nm: sync start\n");

  if (m->verbose)
  {
    /* all is in this function so we don't use data->progress here */
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Writing %s..."), mailbox_path(m));
    progress = progress_new(msg, MUTT_PROGRESS_WRITE, m->msg_count);
  }

  struct HeaderCache *h = nm_hcache_open(m);

  int mh_sync_errors = 0;
  for (int i = 0; i < m->msg_count; i++)
  {
    char old_file[PATH_MAX], new_file[PATH_MAX];
    struct Email *e = m->emails[i];
    if (!e)
      break;

    struct NmEmailData *edata = nm_edata_get(e);

    if (m->verbose)
      progress_update(progress, i, -1);

    *old_file = '\0';
    *new_file = '\0';

    if (edata->oldpath)
    {
      mutt_str_copy(old_file, edata->oldpath, sizeof(old_file));
      old_file[sizeof(old_file) - 1] = '\0';
      mutt_debug(LL_DEBUG2, "nm: fixing obsolete path '%s'\n", old_file);
    }
    else
      email_get_fullpath(e, old_file, sizeof(old_file));

    mutt_buffer_strcpy(&m->pathbuf, edata->folder);
    m->type = edata->type;

    bool ok = maildir_sync_mailbox_message(m, i, h);
    if (!ok)
    {
      // Syncing file failed, query notmuch for new filepath.
      m->type = MUTT_NOTMUCH;
      notmuch_database_t *db = nm_db_get(m, true);
      if (db)
      {
        notmuch_message_t *msg = get_nm_message(db, e);

        sync_email_path_with_nm(e, msg);

        mutt_buffer_strcpy(&m->pathbuf, edata->folder);
        m->type = edata->type;
        ok = maildir_sync_mailbox_message(m, i, h);
        m->type = MUTT_NOTMUCH;
      }
      nm_db_release(m);
      m->type = edata->type;
    }

    mutt_buffer_strcpy(&m->pathbuf, url);
    m->type = MUTT_NOTMUCH;

    if (!ok)
    {
      mh_sync_errors += 1;
      continue;
    }

    if (!e->deleted)
      email_get_fullpath(e, new_file, sizeof(new_file));

    if (e->deleted || (strcmp(old_file, new_file) != 0))
    {
      if (e->deleted && (remove_filename(m, old_file) == 0))
        changed = true;
      else if (*new_file && *old_file && (rename_filename(m, old_file, new_file, e) == 0))
        changed = true;
    }

    FREE(&edata->oldpath);
  }

  if (mh_sync_errors > 0)
  {
    mutt_error(
        ngettext(
            "Unable to sync %d message due to external mailbox modification",
            "Unable to sync %d messages due to external mailbox modification", mh_sync_errors),
        mh_sync_errors);
  }

  mutt_buffer_strcpy(&m->pathbuf, url);
  m->type = MUTT_NOTMUCH;

  nm_db_release(m);

  if (changed)
  {
    m->mtime.tv_sec = mutt_date_epoch();
    m->mtime.tv_nsec = 0;
  }

  nm_hcache_close(h);

  progress_free(&progress);
  FREE(&url);
  mutt_debug(LL_DEBUG1, "nm: .... sync done [rc=%d]\n", rc);
  return rc;
}

/**
 * nm_mbox_close - Close a Mailbox - Implements MxOps::mbox_close()
 *
 * Nothing to do.
 */
static enum MxStatus nm_mbox_close(struct Mailbox *m)
{
  return MX_STATUS_OK;
}

/**
 * nm_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open()
 */
static bool nm_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  struct Email *e = m->emails[msgno];
  if (!e)
    return false;

  char path[PATH_MAX];
  char *folder = nm_email_get_folder(e);

  snprintf(path, sizeof(path), "%s/%s", folder, e->path);

  msg->fp = fopen(path, "r");
  if (!msg->fp && (errno == ENOENT) && ((m->type == MUTT_MAILDIR) || (m->type == MUTT_NOTMUCH)))
  {
    msg->fp = maildir_open_find_message(folder, e->path, NULL);
  }

  return msg->fp != NULL;
}

/**
 * nm_msg_commit - Save changes to an email - Implements MxOps::msg_commit()
 * @retval -1 Always
 */
static int nm_msg_commit(struct Mailbox *m, struct Message *msg)
{
  mutt_error(_("Can't write to virtual folder"));
  return -1;
}

/**
 * nm_msg_close - Close an email - Implements MxOps::msg_close()
 */
static int nm_msg_close(struct Mailbox *m, struct Message *msg)
{
  mutt_file_fclose(&(msg->fp));
  return 0;
}

/**
 * nm_tags_edit - Prompt and validate new messages tags - Implements MxOps::tags_edit()
 */
static int nm_tags_edit(struct Mailbox *m, const char *tags, char *buf, size_t buflen)
{
  *buf = '\0';
  if (mutt_get_field("Add/remove labels: ", buf, buflen, MUTT_NM_TAG, false, NULL, NULL) != 0)
  {
    return -1;
  }
  return 1;
}

/**
 * nm_tags_commit - Save the tags to a message - Implements MxOps::tags_commit()
 */
static int nm_tags_commit(struct Mailbox *m, struct Email *e, char *buf)
{
  if (*buf == '\0')
    return 0; /* no tag change, so nothing to do */

  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return -1;

  notmuch_database_t *db = NULL;
  notmuch_message_t *msg = NULL;
  int rc = -1;

  if (!(db = nm_db_get(m, true)) || !(msg = get_nm_message(db, e)))
    goto done;

  mutt_debug(LL_DEBUG1, "nm: tags modify: '%s'\n", buf);

  update_tags(msg, buf);
  update_email_flags(m, e, buf);
  update_email_tags(e, msg);
  mutt_set_header_color(m, e);

  rc = 0;
  e->changed = true;
done:
  nm_db_release(m);
  if (e->changed)
  {
    m->mtime.tv_sec = mutt_date_epoch();
    m->mtime.tv_nsec = 0;
  }
  mutt_debug(LL_DEBUG1, "nm: tags modify done [rc=%d]\n", rc);
  return rc;
}

/**
 * nm_path_probe - Is this a Notmuch Mailbox? - Implements MxOps::path_probe()
 */
enum MailboxType nm_path_probe(const char *path, const struct stat *st)
{
  if (!mutt_istr_startswith(path, NmUrlProtocol))
    return MUTT_UNKNOWN;

  return MUTT_NOTMUCH;
}

/**
 * nm_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon()
 */
static int nm_path_canon(char *buf, size_t buflen)
{
  return 0;
}

/**
 * nm_path_pretty - Abbreviate a Mailbox path - Implements MxOps::path_pretty()
 */
static int nm_path_pretty(char *buf, size_t buflen, const char *folder)
{
  /* Succeed, but don't do anything, for now */
  return 0;
}

/**
 * nm_path_parent - Find the parent of a Mailbox path - Implements MxOps::path_parent()
 */
static int nm_path_parent(char *buf, size_t buflen)
{
  /* Succeed, but don't do anything, for now */
  return 0;
}

// clang-format off
/**
 * MxNotmuchOps - Notmuch Mailbox - Implements ::MxOps
 */
struct MxOps MxNotmuchOps = {
  .type            = MUTT_NOTMUCH,
  .name             = "notmuch",
  .is_local         = false,
  .ac_owns_path     = nm_ac_owns_path,
  .ac_add           = nm_ac_add,
  .mbox_open        = nm_mbox_open,
  .mbox_open_append = NULL,
  .mbox_check       = nm_mbox_check,
  .mbox_check_stats = nm_mbox_check_stats,
  .mbox_sync        = nm_mbox_sync,
  .mbox_close       = nm_mbox_close,
  .msg_open         = nm_msg_open,
  .msg_open_new     = maildir_msg_open_new,
  .msg_commit       = nm_msg_commit,
  .msg_close        = nm_msg_close,
  .msg_padding_size = NULL,
  .msg_save_hcache  = NULL,
  .tags_edit        = nm_tags_edit,
  .tags_commit      = nm_tags_commit,
  .path_probe       = nm_path_probe,
  .path_canon       = nm_path_canon,
  .path_pretty      = nm_path_pretty,
  .path_parent      = nm_path_parent,
  .path_is_empty    = NULL,
};
// clang-format on
