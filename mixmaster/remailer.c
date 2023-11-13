/**
 * @file
 * Mixmaster Remailer
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page mixmaster_remailer Mixmaster Remailer
 *
 * Mixmaster Remailer
 */

#include "config.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "remailer.h"
#include "globals.h"

/**
 * remailer_free - Free a Remailer
 * @param ptr Remailer to free
 */
void remailer_free(struct Remailer **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Remailer *r = *ptr;
  FREE(&r->shortname);
  FREE(&r->addr);
  FREE(&r->ver);

  FREE(ptr);
}

/**
 * remailer_new - Create a new Remailer
 * @retval ptr Newly allocated Remailer
 */
struct Remailer *remailer_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct Remailer));
}

/**
 * mix_get_caps - Get Mixmaster Capabilities
 * @param capstr Capability string to parse
 * @retval num Capabilities, see #MixCapFlags
 */
static MixCapFlags mix_get_caps(const char *capstr)
{
  MixCapFlags caps = MIX_CAP_NO_FLAGS;

  while (*capstr)
  {
    switch (*capstr)
    {
      case 'C':
        caps |= MIX_CAP_COMPRESS;
        break;

      case 'M':
        caps |= MIX_CAP_MIDDLEMAN;
        break;

      case 'N':
      {
        switch (*++capstr)
        {
          case 'm':
            caps |= MIX_CAP_NEWSMAIL;
            break;

          case 'p':
            caps |= MIX_CAP_NEWSPOST;
            break;
        }
      }
    }

    if (*capstr)
      capstr++;
  }

  return caps;
}

/**
 * remailer_get_hosts - Parse the type2.list as given by mixmaster -T
 * @retval obj Array of Remailer Hosts
 */
struct RemailerArray remailer_get_hosts(void)
{
  struct RemailerArray ra = ARRAY_HEAD_INITIALIZER;
  FILE *fp = NULL;
  char line[8192] = { 0 };
  char *t = NULL;
  struct Remailer *p = NULL;

  const char *const c_mixmaster = cs_subset_string(NeoMutt->sub, "mixmaster");
  if (!c_mixmaster)
    return ra;

  int fd_null = open("/dev/null", O_RDWR);
  if (fd_null == -1)
    return ra;

  struct Buffer *cmd = buf_pool_get();
  buf_printf(cmd, "%s -T", c_mixmaster);

  pid_t mm_pid = filter_create_fd(buf_string(cmd), NULL, &fp, NULL, fd_null, -1,
                                  fd_null, EnvList);
  window_invalidate_all();
  if (mm_pid == -1)
  {
    buf_pool_release(&cmd);
    close(fd_null);
    return ra;
  }

  buf_pool_release(&cmd);

  /* first, generate the "random" remailer */

  p = remailer_new();
  p->shortname = mutt_str_dup(_("<random>"));
  p->num = 0;
  ARRAY_ADD(&ra, p);

  while (fgets(line, sizeof(line), fp))
  {
    p = remailer_new();

    t = strtok(line, " \t\n");
    if (!t)
      goto problem;

    p->shortname = mutt_str_dup(t);

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    p->addr = mutt_str_dup(t);

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    p->ver = mutt_str_dup(t);

    t = strtok(NULL, " \t\n");
    if (!t)
      goto problem;

    p->caps = mix_get_caps(t);

    p->num = ARRAY_SIZE(&ra);
    ARRAY_ADD(&ra, p);
    continue;

  problem:
    remailer_free(&p);
  }

  filter_wait(mm_pid);

  close(fd_null);

  return ra;
}

/**
 * remailer_clear_hosts - Clear a Remailer List
 * @param ra Array of Remailer hosts to clear
 *
 * @note The empty array is not freed
 */
void remailer_clear_hosts(struct RemailerArray *ra)
{
  struct Remailer **r = NULL;
  ARRAY_FOREACH(r, ra)
  {
    remailer_free(r);
  }

  ARRAY_FREE(ra);
}
