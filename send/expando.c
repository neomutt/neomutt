/**
 * @file
 * Greeting Expando definitions
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page send_expando Greeting Expando definitions
 *
 * Greeting Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "expando.h"
#include "expando/lib.h"

/**
 * greeting_real_name - Greeting: Real name - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void greeting_real_name(const struct ExpandoNode *node, void *data,
                               MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Email *e = data;
  const struct Address *to = TAILQ_FIRST(&e->env->to);

  const char *s = mutt_get_name(to);
  buf_strcpy(buf, s);
}

/**
 * greeting_login_name - Greeting: Login name - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void greeting_login_name(const struct ExpandoNode *node, void *data,
                                MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Email *e = data;
  const struct Address *to = TAILQ_FIRST(&e->env->to);

  char tmp[128] = { 0 };
  char *p = NULL;

  if (to)
  {
    mutt_str_copy(tmp, mutt_addr_for_display(to), sizeof(tmp));
    if ((p = strpbrk(tmp, "%@")))
    {
      *p = '\0';
    }
  }

  buf_strcpy(buf, tmp);
}

/**
 * greeting_first_name - Greeting: First name - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void greeting_first_name(const struct ExpandoNode *node, void *data,
                                MuttFormatFlags flags, struct Buffer *buf)
{
  const struct Email *e = data;
  const struct Address *to = TAILQ_FIRST(&e->env->to);
  const struct Address *cc = TAILQ_FIRST(&e->env->cc);

  char tmp[128] = { 0 };
  char *p = NULL;

  if (to)
  {
    const char *s = mutt_get_name(to);
    mutt_str_copy(tmp, s, sizeof(tmp));
  }
  else if (cc)
  {
    const char *s = mutt_get_name(cc);
    mutt_str_copy(tmp, s, sizeof(tmp));
  }

  if ((p = strpbrk(tmp, " %@")))
    *p = '\0';

  buf_strcpy(buf, tmp);
}

/**
 * GreetingRenderCallbacks - Callbacks for Greeting Expandos
 *
 * @sa GreetingFormatDef, ExpandoDataEnvelope
 */
const struct ExpandoRenderCallback GreetingRenderCallbacks[] = {
  // clang-format off
  { ED_ENVELOPE, ED_ENV_REAL_NAME,  greeting_real_name,  NULL },
  { ED_ENVELOPE, ED_ENV_USER_NAME,  greeting_login_name, NULL },
  { ED_ENVELOPE, ED_ENV_FIRST_NAME, greeting_first_name, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
