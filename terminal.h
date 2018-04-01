/**
 * @file
 * Set the terminal title/icon
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_TERMINAL_H
#define _MUTT_TERMINAL_H

#include <stdbool.h>

extern bool TsSupported;

bool mutt_ts_capability(void);
void mutt_ts_status(char *str);
void mutt_ts_icon(char *str);

#endif /* _MUTT_TERMINAL_H */
