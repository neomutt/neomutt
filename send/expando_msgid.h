/**
 * @file
 * Message Id Expando definitions
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_SEND_EXPANDO_MSGID_H
#define MUTT_SEND_EXPANDO_MSGID_H

#include <time.h>
#include "expando/lib.h" // IWYU pragma: keep

/**
 * struct MsgIdData - Data to generate a Message-Id
 */
struct MsgIdData
{
  time_t      now;      ///< Time Now (seconds)
  struct tm   tm;       ///< Time Now (tm)
  const char *fqdn;     ///< Fully-qualified Domain Name
};

/**
 * ExpandoDataMsgId - Expando Fields for the Message-Id
 *
 * @sa ED_MSG, ExpandoDomain
 */
enum ExpandoDataMsgId
{
  ED_MSG_COUNTER = 1,   ///< Step counter looping from 'A' to 'Z'
  ED_MSG_DAY,           ///< Current day of the month (GMT)
  ED_MSG_HOSTNAME,      ///< $hostname
  ED_MSG_HOUR,          ///< Current hour using a 24-hour clock (GMT)
  ED_MSG_MINUTE,        ///< Current month number (GMT)
  ED_MSG_MONTH,         ///< Current minute of the hour (GMT)
  ED_MSG_PID,           ///< PID of the running mutt process
  ED_MSG_RANDOM_1,      ///< 3 bytes of pseudo-random data encoded in Base64
  ED_MSG_RANDOM_3,      ///< Current second of the minute (GMT)
  ED_MSG_RANDOM_12,     ///< 1 byte of pseudo-random data hex encoded (example: '1b')
  ED_MSG_SECOND,        ///< Current year using 4 digits (GMT)
  ED_MSG_YEAR,          ///< 4 byte timestamp + 8 bytes of pseudo-random data encoded in Base64
};

extern const struct ExpandoRenderCallback MsgIdRenderCallbacks[];

char *msgid_generate(void);

#endif /* MUTT_SEND_EXPANDO_MSGID_H */
