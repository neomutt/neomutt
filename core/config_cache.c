/**
 * @file
 * Cache of config variables
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Pietro Cerutti <gahr@gahr.ch>
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
 * @page core_config_cache Cache of config variables
 *
 * Cache of config variables
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "config_cache.h"
#include "neomutt.h"

/// Is the cache enabled?
static bool CacheActive = false;
/// Cached value of $assumed_charset
static const struct Slist *CachedAssumedCharset = NULL;
/// Cached value of $charset
static const char *CachedCharset = NULL;
/// Cached value of $maildir_field_delimiter
static const char *CachedMaildirFieldDelimiter = NULL;

/**
 * cc_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int cc_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0; // LCOV_EXCL_LINE
  if (!nc->event_data)
    return -1; // LCOV_EXCL_LINE

  struct EventConfig *ev_c = nc->event_data;
  if (!ev_c->name || !ev_c->he)
    return 0; // LCOV_EXCL_LINE

  if (mutt_str_equal(ev_c->name, "assumed_charset"))
  {
    CachedAssumedCharset = (const struct Slist *) cs_subset_he_native_get(ev_c->sub,
                                                                          ev_c->he, NULL);
  }
  else if (mutt_str_equal(ev_c->name, "charset"))
  {
    CachedCharset = (const char *) cs_subset_he_native_get(ev_c->sub, ev_c->he, NULL);
  }
  else if (mutt_str_equal(ev_c->name, "maildir_field_delimiter"))
  {
    CachedMaildirFieldDelimiter = (const char *) cs_subset_he_native_get(ev_c->sub,
                                                                         ev_c->he, NULL);
  }

  mutt_debug(LL_DEBUG5, "config done\n");
  return 0;
}

/**
 * cache_setup - Setup a cache of some config variables
 */
static void cache_setup(void)
{
  if (CacheActive)
    return; // LCOV_EXCL_LINE

  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, cc_config_observer, NULL);

  CachedAssumedCharset = cs_subset_slist(NeoMutt->sub, "assumed_charset");
  CachedCharset = cs_subset_string(NeoMutt->sub, "charset");
  CachedMaildirFieldDelimiter = cs_subset_string(NeoMutt->sub, "maildir_field_delimiter");

  CacheActive = true;
}

/**
 * cc_assumed_charset - Get the cached value of $assumed_charset
 * @retval ptr Value of $assumed_charset
 */
const struct Slist *cc_assumed_charset(void)
{
  if (!CacheActive)
  {
    cache_setup();
    CachedAssumedCharset = cs_subset_slist(NeoMutt->sub, "assumed_charset");
  }

  return CachedAssumedCharset;
}

/**
 * cc_charset - Get the cached value of $charset
 * @retval ptr Value of $charset
 */
const char *cc_charset(void)
{
  if (!CacheActive)
  {
    cache_setup();
    CachedCharset = cs_subset_string(NeoMutt->sub, "charset");
  }

  return CachedCharset;
}

/**
 * cc_maildir_field_delimiter - Get the cached value of $maildir_field_delimiter
 * @retval ptr Value of $maildir_field_delimiter
 */
const char *cc_maildir_field_delimiter(void)
{
  if (!CacheActive)
  {
    cache_setup();
    CachedMaildirFieldDelimiter = cs_subset_string(NeoMutt->sub, "maildir_field_delimiter");
  }

  return CachedMaildirFieldDelimiter;
}

/**
 * config_cache_cleanup - Cleanup the cache of charset config variables
 */
void config_cache_cleanup(void)
{
  if (NeoMutt)
    notify_observer_remove(NeoMutt->sub->notify, cc_config_observer, NULL);

  // Don't free them, the config system owns the data
  CachedAssumedCharset = NULL;
  CachedCharset = NULL;
  CachedMaildirFieldDelimiter = NULL;

  CacheActive = false;
}
