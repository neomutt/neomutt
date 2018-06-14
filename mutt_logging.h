/**
 * @file
 * Mutt Logging
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

#ifndef _LOGGING2_H
#define _LOGGING2_H

#include <stdbool.h>
#include <time.h>
#include "config/lib.h"

extern short DebugLevel;
extern char *DebugFile;
extern bool LogAllowDebugSet;

int log_disp_curses(time_t stamp, const char *file, int line, const char *function, int level, ...);

void mutt_log_prep(void);
int  mutt_log_start(void);
void mutt_log_stop(void);
int  mutt_log_set_level(int level, bool verbose);
int  mutt_log_set_file(const char *file, bool verbose);
bool mutt_log_listener(const struct ConfigSet *cs, struct HashElem *he, const char *name, enum ConfigEvent ev);
int  level_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef, intptr_t value, struct Buffer *err);

void mutt_clear_error(void);

#endif /* _LOGGING2_H */
