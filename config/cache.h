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

#ifndef MUTT_CONFIG_CACHE_H
#define MUTT_CONFIG_CACHE_H

const struct Slist *cc_assumed_charset        (void);
const char *        cc_charset                (void);
const char *        cc_maildir_field_delimiter(void);

void config_cache_cleanup(void);

#endif /* MUTT_CONFIG_CACHE_H */
