/**
 * @file
 * Parse Setenv Commands
 *
 * @authors
 * Copyright (C) 1996-2016 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Aditya De Saha <adityadesaha@gmail.com>
 * Copyright (C) 2020 Matthew Hughes <matthewhughes934@gmail.com>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2022 Marco Sirabella <marco@sirabella.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page commands_setenv Parse Setenv Commands
 *
 * Parse Setenv Commands
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "setenv.h"
#include "pager/lib.h"
#include "parse/lib.h"

/**
 * envlist_sort - Compare two environment strings - Implements ::sort_t - @ingroup sort_api
 */
static int envlist_sort(const void *a, const void *b, void *sdata)
{
  return strcmp(*(const char **) a, *(const char **) b);
}

/**
 * parse_setenv - Parse the 'setenv' and 'unsetenv' commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `setenv { <variable>? | <variable> <value> }`
 * - `unsetenv <variable>`
 */
enum CommandResult parse_setenv(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  struct Buffer *token = buf_pool_get();
  struct Buffer *tempfile = NULL;
  enum CommandResult rc = MUTT_CMD_WARNING;

  char **envp = NeoMutt->env;

  bool query = false;
  bool prefix = false;
  bool unset = (cmd->id == CMD_UNSETENV);

  if (!MoreArgs(line))
  {
    if (!StartupComplete)
    {
      buf_printf(err, _("%s: too few arguments"), cmd->name);
      goto done;
    }

    tempfile = buf_pool_get();
    buf_mktemp(tempfile);

    FILE *fp_out = mutt_file_fopen(buf_string(tempfile), "w");
    if (!fp_out)
    {
      // L10N: '%s' is the file name of the temporary file
      buf_printf(err, _("Could not create temporary file %s"), buf_string(tempfile));
      rc = MUTT_CMD_ERROR;
      goto done;
    }

    int count = 0;
    for (char **env = NeoMutt->env; *env; env++)
      count++;

    mutt_qsort_r(NeoMutt->env, count, sizeof(char *), envlist_sort, NULL);

    for (char **env = NeoMutt->env; *env; env++)
      fprintf(fp_out, "%s\n", *env);

    mutt_file_fclose(&fp_out);

    struct PagerData pdata = { 0 };
    struct PagerView pview = { &pdata };

    pdata.fname = buf_string(tempfile);

    pview.banner = cmd->name;
    pview.flags = MUTT_PAGER_NO_FLAGS;
    pview.mode = PAGER_MODE_OTHER;

    mutt_do_pager(&pview, NULL);

    rc = MUTT_CMD_SUCCESS;
    goto done;
  }

  if (*line->dptr == '?')
  {
    query = true;
    prefix = true;

    if (unset)
    {
      buf_printf(err, _("Can't query option with the '%s' command"), cmd->name);
      goto done;
    }

    line->dptr++;
  }

  /* get variable name */
  parse_extract_token(token, line, TOKEN_EQUAL | TOKEN_QUESTION);

  // Validate variable name: must match [A-Z_][A-Z0-9_]*
  const char *name = buf_string(token);
  if (!buf_is_empty(token))
  {
    // First character must be uppercase letter or underscore
    if (!isupper(name[0]) && (name[0] != '_'))
    {
      buf_printf(err, _("%s: invalid variable name '%s'"), cmd->name, name);
      goto done;
    }
    // Subsequent characters must be uppercase letter, digit, or underscore
    for (size_t i = 1; name[i] != '\0'; i++)
    {
      if (!isupper(name[i]) && !mutt_isdigit(name[i]) && (name[i] != '_'))
      {
        buf_printf(err, _("%s: invalid variable name '%s'"), cmd->name, name);
        goto done;
      }
    }
  }

  if (*line->dptr == '?')
  {
    if (unset)
    {
      buf_printf(err, _("Can't query option with the '%s' command"), cmd->name);
      goto done;
    }

    if (prefix)
    {
      buf_printf(err, _("Can't use a prefix when querying a variable"));
      goto done;
    }

    query = true;
    line->dptr++;
  }

  if (query)
  {
    bool found = false;
    while (envp && *envp)
    {
      /* This will display all matches for "^QUERY" */
      if (mutt_str_startswith(*envp, buf_string(token)))
      {
        if (!found)
        {
          mutt_endwin();
          found = true;
        }
        puts(*envp);
      }
      envp++;
    }

    if (found)
    {
      mutt_any_key_to_continue(NULL);
      rc = MUTT_CMD_SUCCESS;
      goto done;
    }

    buf_printf(err, _("%s is unset"), buf_string(token));
    goto done;
  }

  if (unset)
  {
    if (envlist_unset(&NeoMutt->env, buf_string(token)))
      rc = MUTT_CMD_SUCCESS;
    else
      buf_printf(err, _("%s is unset"), buf_string(token));

    goto done;
  }

  /* set variable */

  if (*line->dptr == '=')
  {
    line->dptr++;
    SKIPWS(line->dptr);
  }

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    goto done;
  }

  char *varname = mutt_str_dup(buf_string(token));
  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  envlist_set(&NeoMutt->env, varname, buf_string(token), true);
  FREE(&varname);

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  buf_pool_release(&tempfile);
  return rc;
}
