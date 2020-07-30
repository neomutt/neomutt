/**
 * @file
 * Help Bar
 * Convenience wrapper for the Help Bar headers
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page helpbar Convenience wrapper for the Help Bar headers
 *
 * Convenience wrapper for the Help Bar headers
 *
 * | File              | Description              |
 * | :---------------- | :----------------------- |
 * | helpbar/config.c  | @subpage helpbar_config  |
 * | helpbar/helpbar.c | @subpage helpbar_helpbar |
 * | helpbar/wdata.c   | @subpage helpbar_wdata   |
 */

#ifndef MUTT_HELPBAR_LIB_H
#define MUTT_HELPBAR_LIB_H

#include <stdbool.h>

struct ConfigSet;

struct MuttWindow *helpbar_create(void);

bool config_init_helpbar(struct ConfigSet *cs);

#endif /* MUTT_HELPBAR_LIB_H */
