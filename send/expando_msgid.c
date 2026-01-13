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

/**
 * @page send_expando_msgid Message Id Expando definitions
 *
 * Message Id Expando definitions
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "expando_msgid.h"
#include "expando/lib.h"
#include "globals.h"
#include "sendlib.h"

/**
 * msgid_counter - Message Id: Step Counter - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void msgid_counter(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  static char Counter = 'A';

  buf_addch(buf, Counter);
  Counter = (Counter == 'Z') ? 'A' : Counter + 1;
}

/**
 * msgid_day_num - Message Id: Day - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long msgid_day_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  struct MsgIdData *mid = data;

  return mid->tm.tm_mday;
}

/**
 * msgid_hostname - Message Id: Hostname - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void msgid_hostname(const struct ExpandoNode *node, void *data,
                           MuttFormatFlags flags, struct Buffer *buf)
{
  struct MsgIdData *mid = data;

  buf_strcpy(buf, mid->fqdn);
}

/**
 * msgid_hour_num - Message Id: Hour - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long msgid_hour_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  struct MsgIdData *mid = data;

  return mid->tm.tm_hour;
}

/**
 * msgid_minute_num - Message Id: Minute - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long msgid_minute_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  struct MsgIdData *mid = data;

  return mid->tm.tm_min;
}

/**
 * msgid_month_num - Message Id: Month - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long msgid_month_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  struct MsgIdData *mid = data;

  return mid->tm.tm_mon + 1;
}

/**
 * msgid_pid_num - Message Id: Process Id - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long msgid_pid_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  return getpid();
}

/**
 * msgid_random_1 - Message Id: 1 Random Hex Byte - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void msgid_random_1(const struct ExpandoNode *node, void *data,
                           MuttFormatFlags flags, struct Buffer *buf)
{
  char raw[2] = { 0 };

  // hex encoded random byte
  mutt_randbuf(raw, 1);
  buf_printf(buf, "%02x", (unsigned char) raw[0]);
}

/**
 * msgid_random_3 - Message Id: 3 Random Bytes of Base64 - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void msgid_random_3(const struct ExpandoNode *node, void *data,
                           MuttFormatFlags flags, struct Buffer *buf)
{
  char raw[3] = { 0 };
  char enc[6] = { 0 };

  mutt_randbuf(raw, sizeof(raw));
  mutt_b64_encode_urlsafe(raw, sizeof(raw), enc, sizeof(enc));

  buf_strcpy(buf, enc);
}

/**
 * msgid_random_12 - Message Id: Timestamp + 8 Random Bytes of Base64 - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void msgid_random_12(const struct ExpandoNode *node, void *data,
                            MuttFormatFlags flags, struct Buffer *buf)
{
  char raw[12] = { 0 };
  char enc[20] = { 0 };

  struct MsgIdData *mid = data;

  // Convert the four least significant bytes of our timestamp and put it in
  // raw, with proper endianness (for humans) taken into account
  for (int i = 0; i < 4; i++)
    raw[i] = (uint8_t) (mid->now >> (3 - i) * 8u);

  mutt_randbuf(raw + 4, sizeof(raw) - 4);
  mutt_b64_encode_urlsafe(raw, sizeof(raw), enc, sizeof(enc));

  buf_strcpy(buf, enc);
}

/**
 * msgid_second_num - Message Id: Second - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long msgid_second_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  struct MsgIdData *mid = data;

  return mid->tm.tm_sec;
}

/**
 * msgid_year_num - Message Id: Year (4 digit) - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long msgid_year_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  struct MsgIdData *mid = data;

  return mid->tm.tm_year + 1900;
}

/**
 * MsgIdRenderCallbacks - Callbacks for Message Id Expandos
 *
 * @sa MsgIdFormatDef, ExpandoDataEnvelope
 */
const struct ExpandoRenderCallback MsgIdRenderCallbacks[] = {
  // clang-format off
  { ED_MSG_ID, ED_MSG_COUNTER,   msgid_counter,   NULL             },
  { ED_MSG_ID, ED_MSG_DAY,       NULL,            msgid_day_num    },
  { ED_MSG_ID, ED_MSG_HOSTNAME,  msgid_hostname,  NULL             },
  { ED_MSG_ID, ED_MSG_HOUR,      NULL,            msgid_hour_num   },
  { ED_MSG_ID, ED_MSG_MINUTE,    NULL,            msgid_minute_num },
  { ED_MSG_ID, ED_MSG_MONTH,     NULL,            msgid_month_num  },
  { ED_MSG_ID, ED_MSG_PID,       NULL,            msgid_pid_num    },
  { ED_MSG_ID, ED_MSG_RANDOM_1,  msgid_random_1,  NULL             },
  { ED_MSG_ID, ED_MSG_RANDOM_3,  msgid_random_3,  NULL             },
  { ED_MSG_ID, ED_MSG_RANDOM_12, msgid_random_12, NULL             },
  { ED_MSG_ID, ED_MSG_SECOND,    NULL,            msgid_second_num },
  { ED_MSG_ID, ED_MSG_YEAR,      NULL,            msgid_year_num   },
  { -1, -1, NULL, NULL },
  // clang-format on
};

/**
 * msgid_gen_random - Generate a random Message ID
 * @retval ptr Message ID
 *
 * The length of the message id is chosen such that it is maximal and fits in
 * the recommended 78 character line length for the headers Message-ID:,
 * References:, and In-Reply-To:, this leads to 62 available characters
 * (excluding `@` and `>`).  Since we choose from 32 letters, we have 32^62
 * = 2^310 different message ids.
 *
 * Examples:
 * ```
 * Message-ID: <12345678901111111111222222222233333333334444444444@123456789011>
 * In-Reply-To: <12345678901111111111222222222233333333334444444444@123456789011>
 * References: <12345678901111111111222222222233333333334444444444@123456789011>
 * ```
 *
 * The distribution of the characters to left-of-@ and right-of-@ was arbitrary.
 * The choice was made to put more into the left-id and shorten the right-id to
 * slightly mimic a common length domain name.
 *
 * @note The caller should free the string
 */
static char *msgid_gen_random(void)
{
  const int ID_LEFT_LEN = 50;
  const int ID_RIGHT_LEN = 12;
  char rnd_id_left[ID_LEFT_LEN + 1];
  char rnd_id_right[ID_RIGHT_LEN + 1];
  char buf[128] = { 0 };

  mutt_rand_base32(rnd_id_left, sizeof(rnd_id_left) - 1);
  mutt_rand_base32(rnd_id_right, sizeof(rnd_id_right) - 1);
  rnd_id_left[ID_LEFT_LEN] = 0;
  rnd_id_right[ID_RIGHT_LEN] = 0;

  snprintf(buf, sizeof(buf), "<%s@%s>", rnd_id_left, rnd_id_right);
  return mutt_str_dup(buf);
}

/**
 * msgid_generate - Generate a Message-Id
 * @retval ptr Message ID
 *
 * @note The caller should free the string
 */
char *msgid_generate(void)
{
  const struct Expando *c_message_id_format = cs_subset_expando(NeoMutt->sub, "message_id_format");
  if (!c_message_id_format)
    return msgid_gen_random();

  struct MsgIdData mid = { 0 };

  mid.now = time(NULL);
  mid.tm = mutt_date_gmtime(mid.now);

  mid.fqdn = mutt_fqdn(false, NeoMutt->sub);
  if (!mid.fqdn)
    mid.fqdn = NONULL(ShortHostname);

  struct Buffer *buf = buf_pool_get();

  expando_filter(c_message_id_format, MsgIdRenderCallbacks, &mid,
                 MUTT_FORMAT_NO_FLAGS, buf->dsize, NeoMutt->env, buf);
  if (buf_is_empty(buf))
  {
    buf_pool_release(&buf);
    return msgid_gen_random();
  }

  if (buf_at(buf, 0) != '<')
    buf_insert(buf, 0, "<");

  const int last = buf_len(buf) - 1;
  if (buf_at(buf, last) != '>')
    buf_addch(buf, '>');

  char *msgid = buf_strdup(buf);
  buf_pool_release(&buf);

  return msgid;
}
