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
#include <stddef.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "remailer.h"

/**
 * remailer_free - Free a Remailer
 * @param[out] ptr Remailer to free
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
 * mix_add_entry - Add an entry to the Remailer list
 * @param[out] type2_list Remailer list to add to
 * @param[in]  entry      Remailer to add
 * @param[out] slots      Total number of slots
 * @param[out] used       Number of slots used
 */
static void mix_add_entry(struct Remailer ***type2_list, struct Remailer *entry,
                          size_t *slots, size_t *used)
{
  if (*used == *slots)
  {
    *slots += 5;
    mutt_mem_realloc(type2_list, sizeof(struct Remailer *) * (*slots));
  }

  (*type2_list)[(*used)++] = entry;
  if (entry)
    entry->num = *used;
}

/**
 * remailer_get_hosts - Parse the type2.list as given by mixmaster -T
 * @param[out] l Length of list
 * @retval ptr type2.list
 */
struct Remailer **remailer_get_hosts(size_t *l)
{
  if (!l)
    return NULL;

  FILE *fp = NULL;
  char line[8192];
  char *t = NULL;

  struct Remailer **type2_list = NULL;
  struct Remailer *p = NULL;
  size_t slots = 0, used = 0;

  int fd_null = open("/dev/null", O_RDWR);
  if (fd_null == -1)
    return NULL;

  const char *const c_mixmaster = cs_subset_string(NeoMutt->sub, "mixmaster");
  if (!c_mixmaster)
    return NULL;

  struct Buffer *cmd = mutt_buffer_pool_get();
  mutt_buffer_printf(cmd, "%s -T", c_mixmaster);

  pid_t mm_pid = filter_create_fd(mutt_buffer_string(cmd), NULL, &fp, NULL,
                                  fd_null, -1, fd_null);
  window_invalidate_all();
  if (mm_pid == -1)
  {
    mutt_buffer_pool_release(&cmd);
    close(fd_null);
    return NULL;
  }

  mutt_buffer_pool_release(&cmd);

  /* first, generate the "random" remailer */

  p = remailer_new();
  p->shortname = mutt_str_dup(_("<random>"));
  mix_add_entry(&type2_list, p, &slots, &used);

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

    mix_add_entry(&type2_list, p, &slots, &used);
    continue;

  problem:
    remailer_free(&p);
  }

  *l = used;

  mix_add_entry(&type2_list, NULL, &slots, &used);
  filter_wait(mm_pid);

  close(fd_null);

  return type2_list;
}

/**
 * remailer_clear_hosts - Free a Remailer List
 * @param[out] ttlp Remailer List to free
 */
void remailer_clear_hosts(struct Remailer ***ttlp)
{
  struct Remailer **type2_list = *ttlp;

  for (int i = 0; type2_list[i]; i++)
    remailer_free(&type2_list[i]);

  FREE(ttlp);
}
