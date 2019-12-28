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
 *
 * ## Notes
 *
 * - notmuch uses private Mailbox->data and private Email->data
 *
 * - all exported functions are usable within notmuch context only
 *
 * - all functions have to be covered by "mailbox->magic == MUTT_NOTMUCH" check
 *   (it's implemented in nm_mdata_get() and init_mailbox() functions).
 *
 * - exception are nm_nonctx_* functions -- these functions use nm_default_uri
 *   (or parse URI from another resource)
 */

/**
 * @page nm_notmuch Notmuch virtual mailbox type
 *
 * Notmuch virtual mailbox type
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <notmuch.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "notmuch_private.h"
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "mutt_notmuch.h"
#include "globals.h"
#include "hcache/hcache.h"
#include "index.h"
#include "maildir/lib.h"
#include "mutt_thread.h"
#include "mx.h"
#include "progress.h"
#include "protos.h"

const char NmUriProtocol[] = "notmuch://";
const int NmUriProtocolLen = sizeof(NmUriProtocol) - 1;

/* These Config Variables are only used in notmuch/mutt_notmuch.c */
int C_NmDbLimit;       ///< Config: (notmuch) Default limit for Notmuch queries
char *C_NmDefaultUri;  ///< Config: (notmuch) Path to the Notmuch database
char *C_NmExcludeTags; ///< Config: (notmuch) Exclude messages with these tags
int C_NmOpenTimeout;   ///< Config: (notmuch) Database timeout
char *C_NmQueryType; ///< Config: (notmuch) Default query type: 'threads' or 'messages'
int C_NmQueryWindowCurrentPosition; ///< Config: (notmuch) Position of current search window
char *C_NmQueryWindowTimebase; ///< Config: (notmuch) Units for the time duration
char *C_NmRecordTags; ///< Config: (notmuch) Tags to apply to the 'record' mailbox (sent mail)
char *C_NmUnreadTag;  ///< Config: (notmuch) Tag to use for unread messages
char *C_NmFlaggedTag; ///< Config: (notmuch) Tag to use for flagged messages
char *C_NmRepliedTag; ///< Config: (notmuch) Tag to use for replied messages

/**
 * nm_hcache_open - Open a header cache
 * @param m Mailbox
 * @retval ptr Header cache handle
 */
static header_cache_t *nm_hcache_open(struct Mailbox *m)
{
#ifdef USE_HCACHE
  return mutt_hcache_open(C_HeaderCache, mailbox_path(m), NULL);
#else
  return NULL;
#endif
}

/**
 * nm_hcache_close - Close the header cache
 * @param h Header cache handle
 */
static void nm_hcache_close(header_cache_t *h)
{
#ifdef USE_HCACHE
  mutt_hcache_close(h);
#endif
}

/**
 * string_to_query_type - Lookup a query type
 * @param str String to lookup
 * @retval num Query type, e.g. #NM_QUERY_TYPE_MESGS
 */
static enum NmQueryType string_to_query_type(const char *str)
{
  if (mutt_str_strcmp(str, "threads") == 0)
    return NM_QUERY_TYPE_THREADS;
  if (mutt_str_strcmp(str, "messages") == 0)
    return NM_QUERY_TYPE_MESGS;

  mutt_error(_("failed to parse notmuch query type: %s"), NONULL(str));
  return NM_QUERY_TYPE_MESGS;
}

/**
 * nm_adata_free - Release and clear storage in an NmAccountData structure
 * @param[out] ptr Nm Account data
 */
void nm_adata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NmAccountData *adata = *ptr;
  if (adata->db)
  {
    nm_db_free(adata->db);
    adata->db = NULL;
  }

  FREE(ptr);
}

/**
 * nm_adata_new - Allocate and initialise a new NmAccountData structure
 * @retval ptr New NmAccountData
 */
struct NmAccountData *nm_adata_new(void)
{
  struct NmAccountData *adata = mutt_mem_calloc(1, sizeof(struct NmAccountData));

  return adata;
}

/**
 * nm_adata_get - Get the Notmuch Account data
 * @param m Mailbox
 * @retval ptr  Success
 * @retval NULL Failure, not a Notmuch mailbox
 */
struct NmAccountData *nm_adata_get(struct Mailbox *m)
{
  if (!m || (m->magic != MUTT_NOTMUCH))
    return NULL;

  struct Account *a = m->account;
  if (!a)
    return NULL;

  return a->adata;
}

/**
 * nm_mdata_free - Free data attached to the Mailbox
 * @param[out] ptr Notmuch data
 *
 * The NmMboxData struct stores global Notmuch data, such as the connection to
 * the database.  This function will close the database, free the resources and
 * the struct itself.
 */
void nm_mdata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NmMboxData *mdata = *ptr;

  mutt_debug(LL_DEBUG1, "nm: freeing context data %p\n", mdata);

  url_free(&mdata->db_url);
  FREE(&mdata->db_query);
  FREE(ptr);
}

/**
 * nm_mdata_new - Create a new NmMboxData object from a query
 * @param uri Notmuch query string
 * @retval ptr New NmMboxData struct
 *
 * A new NmMboxData struct is created, then the query is parsed and saved
 * within it.  This should be freed using nm_mdata_free().
 */
struct NmMboxData *nm_mdata_new(const char *uri)
{
  if (!uri)
    return NULL;

  struct NmMboxData *mdata = mutt_mem_calloc(1, sizeof(struct NmMboxData));
  mutt_debug(LL_DEBUG1, "nm: initialize mailbox mdata %p\n", (void *) mdata);

  mdata->db_limit = C_NmDbLimit;
  mdata->query_type = string_to_query_type(C_NmQueryType);
  mdata->db_url = url_parse(uri);
  if (!mdata->db_url)
  {
    mutt_error(_("failed to parse notmuch uri: %s"), uri);
    FREE(&mdata);
    return NULL;
  }
  return mdata;
}

/**
 * nm_mdata_get - Get the Notmuch Mailbox data
 * @param m Mailbox
 * @retval ptr  Success
 * @retval NULL Failure, not a Notmuch mailbox
 */
struct NmMboxData *nm_mdata_get(struct Mailbox *m)
{
  if (!m || (m->magic != MUTT_NOTMUCH))
    return NULL;

  return m->mdata;
}

/**
 * nm_edata_free - Free data attached to an Email
 * @param[out] ptr Email data
 *
 * Each email has an attached NmEmailData, which contains things like the tags
 * (labels).
 */
void nm_edata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NmEmailData *edata = *ptr;
  mutt_debug(LL_DEBUG2, "nm: freeing email %p\n", (void *) edata);
  FREE(&edata->folder);
  FREE(&edata->oldpath);
  FREE(&edata->virtual_id);
  FREE(ptr);
}

/**
 * nm_edata_new - Create a new NmEmailData for an email
 * @retval ptr New NmEmailData struct
 */
struct NmEmailData *nm_edata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct NmEmailData));
}

/**
 * nm_get_default_data - Create a Mailbox with default Notmuch settings
 * @retval ptr  Mailbox with default Notmuch settings
 * @retval NULL Error, it's impossible to create an NmMboxData
 */
static struct NmMboxData *nm_get_default_data(void)
{
  // path to DB + query + URI "decoration"
  char uri[PATH_MAX + 1024 + 32];

  // Try to use C_NmDefaultUri or C_Folder.
  // If neither are set, it is impossible to create a Notmuch URI.
  if (C_NmDefaultUri)
    snprintf(uri, sizeof(uri), "%s", C_NmDefaultUri);
  else if (C_Folder)
    snprintf(uri, sizeof(uri), "notmuch://%s", C_Folder);
  else
    return NULL;

  return nm_mdata_new(uri);
}

/**
 * init_mailbox - Add Notmuch data to the Mailbox
 * @param m Mailbox
 * @retval  0 Success
 * @retval -1 Error Bad format
 *
 * Create a new NmMboxData struct and add it Mailbox::data.
 * Notmuch-specific data will be stored in this struct.
 * This struct can be freed using nm_edata_free().
 */
static int init_mailbox(struct Mailbox *m)
{
  if (!m || (m->magic != MUTT_NOTMUCH))
    return -1;

  if (m->mdata)
    return 0;

  m->mdata = nm_mdata_new(mailbox_path(m));
  if (!m->mdata)
    return -1;

  m->free_mdata = nm_mdata_free;
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
  return (e && e->edata) ? ((struct NmEmailData *) e->edata)->virtual_id : NULL;
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
 * query_type_to_string - Turn a query type into a string
 * @param query_type Query type
 * @retval ptr String
 *
 * @note This is a static string and must not be freed.
 */
static const char *query_type_to_string(enum NmQueryType query_type)
{
  if (query_type == NM_QUERY_TYPE_THREADS)
    return "threads";
  return "messages";
}

/**
 * query_window_check_timebase - Checks if a given timebase string is valid
 * @param[in] timebase: string containing a time base
 * @retval true if the given time base is valid
 *
 * This function returns whether a given timebase string is valid or not,
 * which is used to validate the user settable configuration setting:
 *
 *     nm_query_window_timebase
 */
static bool query_window_check_timebase(const char *timebase)
{
  if ((strcmp(timebase, "hour") == 0) || (strcmp(timebase, "day") == 0) ||
      (strcmp(timebase, "week") == 0) || (strcmp(timebase, "month") == 0) ||
      (strcmp(timebase, "year") == 0))
  {
    return true;
  }
  return false;
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
  cs_str_native_set(NeoMutt->sub->cs, "nm_query_window_current_position", 0, NULL);
}

/**
 * windowed_query_from_query - transforms a vfolder search query into a windowed one
 * @param[in]  query vfolder search string
 * @param[out] buf   allocated string buffer to receive the modified search query
 * @param[in]  buflen allocated maximum size of the buf string buffer
 * @retval true  Transformed search query is available as a string in buf
 * @retval false Search query shall not be transformed
 *
 * This is where the magic of windowed queries happens. Taking a vfolder search
 * query string as parameter, it will use the following two user settings:
 *
 * - `nm_query_window_duration` and
 * - `nm_query_window_timebase`
 *
 * to amend given vfolder search window. Then using a third parameter:
 *
 * - `nm_query_window_current_position`
 *
 * it will generate a proper notmuch `date:` parameter. For example, given a
 * duration of `2`, a timebase set to `week` and a position defaulting to `0`,
 * it will prepend to the 'tag:inbox' notmuch search query the following string:
 *
 * - `query`: `tag:inbox`
 * - `buf`:   `date:2week..now and tag:inbox`
 *
 * If the position is set to `4`, with `duration=3` and `timebase=month`:
 *
 * - `query`: `tag:archived`
 * - `buf`:   `date:12month..9month and tag:archived`
 *
 * The window won't be applied:
 *
 * - If the duration of the search query is set to `0` this function will be disabled.
 * - If the timebase is invalid, it will show an error message and do nothing.
 *
 * If there's no search registered in `nm_query_window_current_search` or this is
 * a new search, it will reset the window and do the search.
 */
static bool windowed_query_from_query(const char *query, char *buf, size_t buflen)
{
  mutt_debug(LL_DEBUG2, "nm: %s\n", query);

  int beg = C_NmQueryWindowDuration * (C_NmQueryWindowCurrentPosition + 1);
  int end = C_NmQueryWindowDuration * C_NmQueryWindowCurrentPosition;

  /* if the duration is a non positive integer, disable the window */
  if (C_NmQueryWindowDuration <= 0)
  {
    query_window_reset();
    return false;
  }

  /* if the query has changed, reset the window position */
  if (!C_NmQueryWindowCurrentSearch || (strcmp(query, C_NmQueryWindowCurrentSearch) != 0))
    query_window_reset();

  if (!query_window_check_timebase(C_NmQueryWindowTimebase))
  {
    mutt_message(_("Invalid nm_query_window_timebase value (valid values are: "
                   "hour, day, week, month or year)"));
    mutt_debug(LL_DEBUG2, "Invalid nm_query_window_timebase value\n");
    return false;
  }

  if (end == 0)
  {
    // Open-ended date allows mail from the future.
    // This may occur is the sender's time settings are off.
    snprintf(buf, buflen, "date:%d%s.. and %s", beg, C_NmQueryWindowTimebase,
             C_NmQueryWindowCurrentSearch);
  }
  else
  {
    snprintf(buf, buflen, "date:%d%s..%d%s and %s", beg, C_NmQueryWindowTimebase,
             end, C_NmQueryWindowTimebase, C_NmQueryWindowCurrentSearch);
  }

  mutt_debug(LL_DEBUG2, "nm: %s -> %s\n", query, buf);

  return true;
}

/**
 * get_query_string - builds the notmuch vfolder search string
 * @param mdata Notmuch Mailbox data
 * @param window If true enable application of the window on the search string
 * @retval ptr  String containing a notmuch search query
 * @retval NULL If none can be generated
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
  if (mdata->db_query)
    return mdata->db_query;

  mdata->query_type = string_to_query_type(C_NmQueryType); /* user's default */

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
      mdata->query_type = string_to_query_type(item->value);
    else if (strcmp(item->name, "query") == 0)
      mdata->db_query = mutt_str_strdup(item->value);
  }

  if (!mdata->db_query)
    return NULL;

  if (window)
  {
    char buf[1024];
    mutt_str_replace(&C_NmQueryWindowCurrentSearch, mdata->db_query);

    /* if a date part is defined, do not apply windows (to avoid the risk of
     * having a non-intersected date frame). A good improvement would be to
     * accept if they intersect */
    if (!strstr(mdata->db_query, "date:") &&
        windowed_query_from_query(mdata->db_query, buf, sizeof(buf)))
    {
      mdata->db_query = mutt_str_strdup(buf);
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
  if (!C_NmExcludeTags || !query)
    return;

  char *end = NULL, *tag = NULL;

  char *buf = mutt_str_strdup(C_NmExcludeTags);

  for (char *p = buf; p && (p[0] != '\0'); p++)
  {
    if (!tag && isspace(*p))
      continue;
    if (!tag)
      tag = p; /* begin of the tag */
    if ((p[0] == ',') || (p[0] == ' '))
      end = p; /* terminate the tag */
    else if (p[1] == '\0')
      end = p + 1; /* end of optstr */
    if (!tag || !end)
      continue;
    if (tag >= end)
      break;
    *end = '\0';

    mutt_debug(LL_DEBUG2, "nm: query exclude tag '%s'\n", tag);
    notmuch_query_add_tag_exclude(query, tag);
    end = NULL;
    tag = NULL;
  }
  notmuch_query_set_omit_excluded(query, 1);
  FREE(&buf);
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
  struct NmEmailData *edata = e->edata;
  char *new_tags = NULL;
  char *old_tags = NULL;

  mutt_debug(LL_DEBUG2, "nm: tags update requested (%s)\n", edata->virtual_id);

  for (notmuch_tags_t *tags = notmuch_message_get_tags(msg);
       tags && notmuch_tags_valid(tags); notmuch_tags_move_to_next(tags))
  {
    const char *t = notmuch_tags_get(tags);
    if (!t || !*t)
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
  struct NmEmailData *edata = e->edata;

  mutt_debug(LL_DEBUG2, "nm: path update requested path=%s, (%s)\n", path, edata->virtual_id);

  char *p = strrchr(path, '/');
  if (p && ((p - path) > 3) &&
      ((strncmp(p - 3, "cur", 3) == 0) || (strncmp(p - 3, "new", 3) == 0) ||
       (strncmp(p - 3, "tmp", 3) == 0)))
  {
    edata->magic = MUTT_MAILDIR;

    FREE(&e->path);
    FREE(&edata->folder);

    p -= 3; /* skip subfolder (e.g. "new") */
    e->path = mutt_str_strdup(p);

    for (; (p > path) && (*(p - 1) == '/'); p--)
      ;

    edata->folder = mutt_str_substr_dup(path, p);

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
      ((strncmp(p - 3, "cur", 3) == 0) || (strncmp(p - 3, "new", 3) == 0) ||
       (strncmp(p - 3, "tmp", 3) == 0)))
  {
    p -= 3;
    for (; (p > path) && (*(p - 1) == '/'); p--)
      ;

    return mutt_str_substr_dup(path, p);
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
  if (e->edata)
    return 0;

  struct NmEmailData *edata = nm_edata_new();
  e->edata = edata;
  e->free_edata = nm_edata_free;

  /* Notmuch ensures that message Id exists (if not notmuch Notmuch will
   * generate an ID), so it's more safe than use neomutt Email->env->id */
  const char *id = notmuch_message_get_message_id(msg);
  edata->virtual_id = mutt_str_strdup(id);

  mutt_debug(LL_DEBUG2, "nm: [e=%p, edata=%p] (%s)\n", (void *) e, (void *) e->edata, id);

  char *nm_msg_id = nm2mutt_message_id(id);
  if (!e->env->message_id)
  {
    e->env->message_id = nm_msg_id;
  }
  else if (mutt_str_strcmp(e->env->message_id, nm_msg_id) != 0)
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
 * progress_reset - Reset the progress counter
 * @param m Mailbox
 */
static void progress_reset(struct Mailbox *m)
{
  if (m->quiet)
    return;

  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return;

  memset(&mdata->progress, 0, sizeof(mdata->progress));
  mdata->oldmsgcount = m->msg_count;
  mdata->ignmsgcount = 0;
  mdata->noprogress = false;
  mdata->progress_ready = false;
}

/**
 * progress_update - Update the progress counter
 * @param m Mailbox
 * @param q   Notmuch query
 */
static void progress_update(struct Mailbox *m, notmuch_query_t *q)
{
  struct NmMboxData *mdata = nm_mdata_get(m);

  if (m->quiet || !mdata || mdata->noprogress)
    return;

  if (!mdata->progress_ready && q)
  {
    // The total mail count is in oldmsgcount, so use that instead of recounting.
    mutt_progress_init(&mdata->progress, _("Reading messages..."),
                       MUTT_PROGRESS_READ, mdata->oldmsgcount);
    mdata->progress_ready = true;
  }

  if (mdata->progress_ready)
  {
    mutt_progress_update(&mdata->progress, m->msg_count + mdata->ignmsgcount, -1);
  }
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
static void append_message(header_cache_t *h, struct Mailbox *m,
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
    progress_update(m, q);
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
  void *from_cache = mutt_hcache_fetch(h, path, mutt_str_strlen(path));
  if (from_cache)
  {
    e = mutt_hcache_restore(from_cache);
  }
  else
#endif
  {
    if (access(path, F_OK) == 0)
      e = maildir_parse_message(MUTT_MAILDIR, path, false, NULL);
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
  }

  if (!e)
  {
    mutt_debug(LL_DEBUG1, "nm: failed to parse message: %s\n", path);
    goto done;
  }

#ifdef USE_HCACHE

  if (from_cache)
  {
    mutt_hcache_free(h, &from_cache);
  }
  else
  {
    mutt_hcache_store(h, newpath ? newpath : path,
                      mutt_str_strlen(newpath ? newpath : path), e, 0);
  }
#endif
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
    struct NmEmailData *edata = e->edata;

    if (edata)
    {
      mutt_debug(LL_DEBUG1, "nm: remember obsolete path: %s\n", path);
      edata->oldpath = mutt_str_strdup(path);
    }
  }
  progress_update(m, q);
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
static void append_replies(header_cache_t *h, struct Mailbox *m,
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
static void append_thread(header_cache_t *h, struct Mailbox *m,
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

  header_cache_t *h = nm_hcache_open(m);

  for (; notmuch_messages_valid(msgs) && ((limit == 0) || (m->msg_count < limit));
       notmuch_messages_move_to_next(msgs))
  {
    if (SigInt == 1)
    {
      nm_hcache_close(h);
      SigInt = 0;
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

  header_cache_t *h = nm_hcache_open(m);

  for (; notmuch_threads_valid(threads) && ((limit == 0) || (m->msg_count < limit));
       notmuch_threads_move_to_next(threads))
  {
    if (SigInt == 1)
    {
      nm_hcache_close(h);
      SigInt = 0;
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
    if (mutt_str_strcmp(possible_match_tag, tag) == 0)
    {
      return true;
    }
  }
  return false;
}

/**
 * update_tags - Update the tags on a message
 * @param msg  Notmuch message
 * @param tags String of tags (space separated)
 * @retval  0 Success
 * @retval -1 Failure
 */
static int update_tags(notmuch_message_t *msg, const char *tags)
{
  char *buf = mutt_str_strdup(tags);
  if (!buf)
    return -1;

  notmuch_message_freeze(msg);

  char *tag = NULL, *end = NULL;
  for (char *p = buf; p && *p; p++)
  {
    if (!tag && isspace(*p))
      continue;
    if (!tag)
      tag = p; /* begin of the tag */
    if ((p[0] == ',') || (p[0] == ' '))
      end = p; /* terminate the tag */
    else if (p[1] == '\0')
      end = p + 1; /* end of optstr */
    if (!tag || !end)
      continue;
    if (tag >= end)
      break;

    end[0] = '\0';

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
    end = NULL;
    tag = NULL;
  }

  notmuch_message_thaw(msg);
  FREE(&buf);
  return 0;
}

/**
 * update_email_flags - Update the Email's flags
 * @param m   Mailbox
 * @param e   Email
 * @param tags String of tags (space separated)
 * @retval  0 Success
 * @retval -1 Failure
 *
 * TODO: extract parsing of string to separate function, join
 * update_email_tags and update_email_flags, which are given an array of
 * tags.
 */
static int update_email_flags(struct Mailbox *m, struct Email *e, const char *tags)
{
  char *buf = mutt_str_strdup(tags);
  if (!buf)
    return -1;

  char *tag = NULL, *end = NULL;
  for (char *p = buf; p && *p; p++)
  {
    if (!tag && isspace(*p))
      continue;
    if (!tag)
      tag = p; /* begin of the tag */
    if ((p[0] == ',') || (p[0] == ' '))
      end = p; /* terminate the tag */
    else if (p[1] == '\0')
      end = p + 1; /* end of optstr */
    if (!tag || !end)
      continue;
    if (tag >= end)
      break;

    end[0] = '\0';

    if (tag[0] == '-')
    {
      tag++;
      if (strcmp(tag, C_NmUnreadTag) == 0)
        mutt_set_flag(m, e, MUTT_READ, true);
      else if (strcmp(tag, C_NmRepliedTag) == 0)
        mutt_set_flag(m, e, MUTT_REPLIED, false);
      else if (strcmp(tag, C_NmFlaggedTag) == 0)
        mutt_set_flag(m, e, MUTT_FLAG, false);
    }
    else
    {
      tag = (tag[0] == '+') ? tag + 1 : tag;
      if (strcmp(tag, C_NmUnreadTag) == 0)
        mutt_set_flag(m, e, MUTT_READ, false);
      else if (strcmp(tag, C_NmRepliedTag) == 0)
        mutt_set_flag(m, e, MUTT_REPLIED, true);
      else if (strcmp(tag, C_NmFlaggedTag) == 0)
        mutt_set_flag(m, e, MUTT_FLAG, true);
    }
    end = NULL;
    tag = NULL;
  }

  FREE(&buf);
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

  mutt_str_strfcpy(folder, old, sizeof(folder));
  char *p = strrchr(folder, '/');
  if (p)
  {
    *p = '\0';
    p++;
  }
  else
    p = folder;

  mutt_str_strfcpy(filename, p, sizeof(filename));

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
  return (e && e->edata) ? ((struct NmEmailData *) e->edata)->folder : NULL;
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

  progress_reset(m);
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
  return rc;
}

/**
 * nm_parse_type_from_query - Parse a query type out of a query
 * @param mdata Mailbox, used for the query_type
 * @param buf   Buffer for URI
 *
 * If a user writes a query for a vfolder and includes a type= statement, that
 * type= will be encoded, which Notmuch will treat as part of the query=
 * statement. This method will remove the type= and set it within the Mailbox
 * struct.
 */
void nm_parse_type_from_query(struct NmMboxData *mdata, char *buf)
{
  // The six variations of how type= could appear.
  const char *variants[6] = { "&type=threads", "&type=messages",
                              "type=threads&", "type=messages&",
                              "type=threads",  "type=messages" };

  int variants_size = mutt_array_size(variants);
  for (int i = 0; i < variants_size; i++)
  {
    if (mutt_str_strcasestr(buf, variants[i]) != NULL)
    {
      // variants[] is setup such that type can be determined via modulo 2.
      mdata->query_type = ((i % 2) == 0) ? NM_QUERY_TYPE_THREADS : NM_QUERY_TYPE_MESGS;

      mutt_str_remall_strcasestr(buf, variants[i]);
    }
  }
}

/**
 * nm_uri_from_query - Turn a query into a URI
 * @param m      Mailbox
 * @param buf    Buffer for URI
 * @param buflen Length of buffer
 * @retval ptr  Query as a URI
 * @retval NULL Error
 */
char *nm_uri_from_query(struct Mailbox *m, char *buf, size_t buflen)
{
  mutt_debug(LL_DEBUG2, "(%s)\n", buf);
  struct NmMboxData *mdata = nm_mdata_get(m);
  char uri[PATH_MAX + 1024 + 32]; /* path to DB + query + URI "decoration" */
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

  nm_parse_type_from_query(mdata, buf);

  if (get_limit(mdata) == C_NmDbLimit)
  {
    added = snprintf(uri, sizeof(uri), "%s%s?type=%s&query=", NmUriProtocol,
                     nm_db_get_filename(m), query_type_to_string(mdata->query_type));
  }
  else
  {
    added = snprintf(uri, sizeof(uri), "%s%s?type=%s&limit=%d&query=", NmUriProtocol,
                     nm_db_get_filename(m),
                     query_type_to_string(mdata->query_type), get_limit(mdata));
  }

  if (added >= sizeof(uri))
  {
    // snprintf output was truncated, so can't create URI
    return NULL;
  }

  url_pct_encode(&uri[added], sizeof(uri) - added, buf);

  mutt_str_strfcpy(buf, uri, buflen);
  buf[buflen - 1] = '\0';

  if (using_default_data)
    nm_mdata_free((void **) &mdata);

  mutt_debug(LL_DEBUG1, "nm: uri from query '%s'\n", buf);
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
  if (C_NmQueryWindowCurrentPosition != 0)
    C_NmQueryWindowCurrentPosition--;

  mutt_debug(LL_DEBUG2, "(%d)\n", C_NmQueryWindowCurrentPosition);
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
  C_NmQueryWindowCurrentPosition++;
  mutt_debug(LL_DEBUG2, "(%d)\n", C_NmQueryWindowCurrentPosition);
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

  if (!old_file && e && e->edata)
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
 * nm_mbox_check_stats - Check the Mailbox statistics - Implements MxOps::check_stats()
 */
static int nm_mbox_check_stats(struct Mailbox *m, int flags)
{
  struct UrlQuery *item = NULL;
  struct Url *url = NULL;
  char *db_filename = NULL, *db_query = NULL;
  notmuch_database_t *db = NULL;
  int rc = -1;
  int limit = C_NmDbLimit;
  mutt_debug(LL_DEBUG1, "nm: count\n");

  url = url_parse(mailbox_path(m));
  if (!url)
  {
    mutt_error(_("failed to parse notmuch uri: %s"), mailbox_path(m));
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
    if (C_NmDefaultUri)
    {
      if (nm_path_probe(C_NmDefaultUri, NULL) == MUTT_NOTMUCH)
        db_filename = C_NmDefaultUri + NmUriProtocolLen;
      else
        db_filename = C_NmDefaultUri;
    }
    else if (C_Folder)
      db_filename = C_Folder;
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
  mutt_str_asprintf(&qstr, "( %s ) tag:%s", db_query, C_NmUnreadTag);
  m->msg_unread = count_query(db, qstr, limit);
  FREE(&qstr);

  // flagged messages
  mutt_str_asprintf(&qstr, "( %s ) tag:%s", db_query, C_NmFlaggedTag);
  m->msg_flagged = count_query(db, qstr, limit);
  FREE(&qstr);

  rc = (m->msg_new > 0);
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
    if (C_NmRecordTags)
      update_tags(msg, C_NmRecordTags);
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
        tag_list[*tag_count] = mutt_str_strdup(tag);
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
 * nm_ac_find - Find an Account that matches a Mailbox path - Implements MxOps::ac_find()
 */
static struct Account *nm_ac_find(struct Account *a, const char *path)
{
  if (!a || (a->magic != MUTT_NOTMUCH) || !path)
    return NULL;

  return a;
}

/**
 * nm_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add()
 */
static int nm_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m || (m->magic != MUTT_NOTMUCH))
    return -1;

  if (a->adata)
    return 0;

  struct NmAccountData *adata = nm_adata_new();
  a->adata = adata;
  a->free_adata = nm_adata_free;

  return 0;
}

/**
 * nm_mbox_open - Open a Mailbox - Implements MxOps::mbox_open()
 */
static int nm_mbox_open(struct Mailbox *m)
{
  if (init_mailbox(m) != 0)
    return -1;

  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return -1;

  mutt_debug(LL_DEBUG1, "nm: reading messages...[current count=%d]\n", m->msg_count);

  progress_reset(m);

  int rc = -1;

  notmuch_query_t *q = get_query(m, false);
  if (q)
  {
    rc = 0;
    switch (mdata->query_type)
    {
      case NM_QUERY_TYPE_MESGS:
        if (!read_mesgs_query(m, q, false))
          rc = -2;
        break;
      case NM_QUERY_TYPE_THREADS:
        if (!read_threads_query(m, q, false, get_limit(mdata)))
          rc = -2;
        break;
    }
    notmuch_query_destroy(q);
  }

  nm_db_release(m);

  m->mtime.tv_sec = mutt_date_epoch();
  m->mtime.tv_nsec = 0;

  mdata->oldmsgcount = 0;

  mutt_debug(LL_DEBUG1, "nm: reading messages... done [rc=%d, count=%d]\n", rc, m->msg_count);
  return rc;
}

/**
 * nm_mbox_check - Check for new mail - Implements MxOps::mbox_check()
 * @param m           Mailbox
 * @param index_hint  Remember our place in the index
 * @retval -1 Error
 * @retval  0 Success
 * @retval #MUTT_NEW_MAIL New mail has arrived
 * @retval #MUTT_REOPENED Mailbox closed and reopened
 * @retval #MUTT_FLAGS    Flags have changed
 */
static int nm_mbox_check(struct Mailbox *m, int *index_hint)
{
  if (!m)
    return -1;

  struct NmMboxData *mdata = nm_mdata_get(m);
  time_t mtime = 0;
  if (!mdata || (nm_db_get_mtime(m, &mtime) != 0))
    return -1;

  int new_flags = 0;
  bool occult = false;

  if (m->mtime.tv_sec >= mtime)
  {
    mutt_debug(LL_DEBUG2, "nm: check unnecessary (db=%lu mailbox=%lu)\n", mtime,
               m->mtime.tv_sec);
    return 0;
  }

  mutt_debug(LL_DEBUG1, "nm: checking (db=%lu mailbox=%lu)\n", mtime, m->mtime.tv_sec);

  notmuch_query_t *q = get_query(m, false);
  if (!q)
    goto done;

  mutt_debug(LL_DEBUG1, "nm: start checking (count=%d)\n", m->msg_count);
  mdata->oldmsgcount = m->msg_count;
  mdata->noprogress = true;

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
    return false;
#elif LIBNOTMUCH_CHECK_VERSION(4, 3, 0)
  if (!msgs)
    goto done;
#endif

  header_cache_t *h = nm_hcache_open(m);

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

    if (mutt_str_strcmp(old_file, new_file) != 0)
      update_message_path(e, new_file);

    if (!e->changed)
    {
      /* if the user hasn't modified the flags on this message, update the
       * flags we just detected.  */
      struct Email e_tmp = { 0 };
      maildir_parse_flags(&e_tmp, new_file);
      maildir_update_flags(m, e, &e_tmp);
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

  return occult ? MUTT_REOPENED :
                  (m->msg_count > mdata->oldmsgcount) ? MUTT_NEW_MAIL :
                                                        new_flags ? MUTT_FLAGS : 0;
}

/**
 * nm_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync()
 */
static int nm_mbox_sync(struct Mailbox *m, int *index_hint)
{
  if (!m)
    return -1;

  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!mdata)
    return -1;

  int rc = 0;
  struct Progress progress;
  char *uri = mutt_str_strdup(mailbox_path(m));
  bool changed = false;

  mutt_debug(LL_DEBUG1, "nm: sync start\n");

  if (!m->quiet)
  {
    /* all is in this function so we don't use data->progress here */
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Writing %s..."), mailbox_path(m));
    mutt_progress_init(&progress, msg, MUTT_PROGRESS_WRITE, m->msg_count);
  }

  header_cache_t *h = nm_hcache_open(m);

  for (int i = 0; i < m->msg_count; i++)
  {
    char old_file[PATH_MAX], new_file[PATH_MAX];
    struct Email *e = m->emails[i];
    if (!e)
      break;

    struct NmEmailData *edata = e->edata;

    if (!m->quiet)
      mutt_progress_update(&progress, i, -1);

    *old_file = '\0';
    *new_file = '\0';

    if (edata->oldpath)
    {
      mutt_str_strfcpy(old_file, edata->oldpath, sizeof(old_file));
      old_file[sizeof(old_file) - 1] = '\0';
      mutt_debug(LL_DEBUG2, "nm: fixing obsolete path '%s'\n", old_file);
    }
    else
      email_get_fullpath(e, old_file, sizeof(old_file));

    mutt_buffer_strcpy(&m->pathbuf, edata->folder);
    m->magic = edata->magic;
    rc = mh_sync_mailbox_message(m, i, h);
    mutt_buffer_strcpy(&m->pathbuf, uri);
    m->magic = MUTT_NOTMUCH;

    if (rc)
      break;

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

  mutt_buffer_strcpy(&m->pathbuf, uri);
  m->magic = MUTT_NOTMUCH;

  nm_db_release(m);

  if (changed)
  {
    m->mtime.tv_sec = mutt_date_epoch();
    m->mtime.tv_nsec = 0;
  }

  nm_hcache_close(h);

  FREE(&uri);
  mutt_debug(LL_DEBUG1, "nm: .... sync done [rc=%d]\n", rc);
  return rc;
}

/**
 * nm_mbox_close - Close a Mailbox - Implements MxOps::mbox_close()
 *
 * Nothing to do.
 */
static int nm_mbox_close(struct Mailbox *m)
{
  return 0;
}

/**
 * nm_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open()
 */
static int nm_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  if (!m || !m->emails || (msgno >= m->msg_count) || !msg)
    return -1;

  struct Email *e = m->emails[msgno];
  if (!e)
    return -1;

  char path[PATH_MAX];
  char *folder = nm_email_get_folder(e);

  snprintf(path, sizeof(path), "%s/%s", folder, e->path);

  msg->fp = fopen(path, "r");
  if (!msg->fp && (errno == ENOENT) &&
      ((m->magic == MUTT_MAILDIR) || (m->magic == MUTT_NOTMUCH)))
  {
    msg->fp = maildir_open_find_message(folder, e->path, NULL);
  }

  if (!msg->fp)
    return -1;

  return 0;
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
  if (!msg)
    return -1;
  mutt_file_fclose(&(msg->fp));
  return 0;
}

/**
 * nm_tags_edit - Prompt and validate new messages tags - Implements MxOps::tags_edit()
 */
static int nm_tags_edit(struct Mailbox *m, const char *tags, char *buf, size_t buflen)
{
  *buf = '\0';
  if (mutt_get_field("Add/remove labels: ", buf, buflen, MUTT_NM_TAG) != 0)
    return -1;
  return 1;
}

/**
 * nm_tags_commit - Save the tags to a message - Implements MxOps::tags_commit()
 */
static int nm_tags_commit(struct Mailbox *m, struct Email *e, char *buf)
{
  if (!m)
    return -1;

  struct NmMboxData *mdata = nm_mdata_get(m);
  if (!buf || (*buf == '\0') || !mdata)
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
  if (!path || !mutt_str_startswith(path, NmUriProtocol, CASE_IGNORE))
    return MUTT_UNKNOWN;

  return MUTT_NOTMUCH;
}

/**
 * nm_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon()
 */
static int nm_path_canon(char *buf, size_t buflen)
{
  if (!buf)
    return -1;

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
  .magic            = MUTT_NOTMUCH,
  .name             = "notmuch",
  .ac_find          = nm_ac_find,
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
};
// clang-format on
