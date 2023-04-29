/**
 * @file
 * Cache of config variables
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
 * @page config_cache Cache of config variables
 *
 * Cache of config variables
 */

#include "config.h"
#include <stdbool.h>
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "lib.h"

/// Is the cache enabled?
static bool CacheActive = false;
/// Cached value of $assumed_charset
static const struct Slist *CachedAssumedCharset = NULL;
/// Is $assumed_charset active?
static bool CacheActiveAssumedCharset = false;
/// Cached value of $charset
static const char *CachedCharset = NULL;
/// Is $charset active?
static bool CacheActiveCharset = false;

/**
 * cc_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int cc_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;  // LCOV_EXCL_LINE
  if (!nc->event_data)
    return -1; // LCOV_EXCL_LINE

  struct EventConfig *ev_c = nc->event_data;
  if (!ev_c->name || !ev_c->he)
    return 0;

  if ((nc->event_subtype == NT_CONFIG_DELETED) && !ev_c->name)
  {
    // Shutdown
    CacheActive = false;
    CacheActiveAssumedCharset = false;
    CacheActiveCharset = false;
    CachedAssumedCharset = NULL;
    CachedCharset = NULL;

    return 0;
  }

  if (mutt_str_equal(ev_c->name, "assumed_charset"))
  {
    CachedAssumedCharset = (const struct Slist *) cs_subset_he_native_get(ev_c->sub,
                                                                          ev_c->he, NULL);
  }
  else if (mutt_str_equal(ev_c->name, "charset"))
  {
    CachedCharset = (const char *) cs_subset_he_native_get(ev_c->sub, ev_c->he, NULL);
  }

  mutt_debug(LL_DEBUG5, "config done\n");
  return 0;
}

/**
 * charset_cache_setup - Setup a cache of some charset config variables
 */
static void charset_cache_setup(void)
{
  if (CacheActive)
    return;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, cc_config_observer, NULL);
  CacheActive = true;
}

/**
 * cc_assumed_charset - Get the cached value of $assumed_charset
 * @retval ptr Value of $assumed_charset
 */
const struct Slist *cc_assumed_charset(void)
{
  if (!CacheActiveAssumedCharset)
  {
    charset_cache_setup();
    CachedAssumedCharset = cs_subset_slist(NeoMutt->sub, "assumed_charset");
    CacheActiveAssumedCharset = true;
  }

  return CachedAssumedCharset;
}

/**
 * cc_charset - Get the cached value of $charset
 * @retval ptr Value of $charset
 */
const char *cc_charset(void)
{
  if (!CacheActiveCharset)
  {
    charset_cache_setup();
    CachedCharset = cs_subset_string(NeoMutt->sub, "charset");
    CacheActiveCharset = true;
  }

  return CachedCharset;
}
