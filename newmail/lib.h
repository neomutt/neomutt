/**
 * @file
 * New mail notification observer.
 *
 * @authors
 * Copyright (C) 2022 Michal Siedlaczek <michal@siedlaczek.me>
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

#ifndef MUTT_NEW_MAIL_H
#define MUTT_NEW_MAIL_H

#include <stdint.h>
#include <stdio.h>
#include "mutt/notify.h"
#include "format_flags.h"

typedef int(Execute)(const char *cmd);

const char *new_mail_format_str(char *buf, size_t buflen, size_t col, int cols,
                                char op, const char *src, const char *prec,
                                const char *if_str, const char *else_str,
                                intptr_t data, MuttFormatFlags flags);
int handle_new_mail_event(const char *cmd, struct NotifyCallback *nc, Execute *execute);
int new_mail_observer(struct NotifyCallback *nc);

#endif
