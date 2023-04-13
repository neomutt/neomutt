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
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "lib.h"

/// Cached value of $assumed_charset
const struct Slist *CachedAssumedCharset = NULL;
/// Cached value of $charset
const char *CachedCharset = NULL;

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
void charset_cache_setup(void)
{
  notify_observer_add(NeoMutt->notify, NT_CONFIG, cc_config_observer, NULL);

  CachedAssumedCharset = cs_subset_slist(NeoMutt->sub, "assumed_charset");
  CachedCharset = cs_subset_string(NeoMutt->sub, "charset");
}

/**
 * charset_cache_free - Cleanup the cache of charset config variables
 */
void charset_cache_free(void)
{
  if (NeoMutt)
    notify_observer_remove(NeoMutt->notify, cc_config_observer, NULL);

  // Don't free them, the config system owns the data
  CachedAssumedCharset = NULL;
  CachedCharset = NULL;
}
