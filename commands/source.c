/**
 * @file
 * Parse Source Commands
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
 * @page commands_source Parse Source Commands
 *
 * Parse Source Commands
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "source.h"
#include "parse/lib.h"
#include "muttlib.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/// LIFO designed to contain the list of config files that have been sourced and
/// avoid cyclic sourcing.
static struct ListHead MuttrcStack = STAILQ_HEAD_INITIALIZER(MuttrcStack);

#define MAX_ERRS 128

/**
 * source_rc - Read an initialization file
 * @param rcfile_path Path to initialization file
 * @param err         Buffer for error messages
 * @retval <0 NeoMutt should pause to let the user know
 */
int source_rc(const char *rcfile_path, struct Buffer *err)
{
  int lineno = 0, rc = 0, warnings = 0;
  enum CommandResult line_rc;
  struct Buffer *linebuf = NULL;
  char *line = NULL;
  char *currentline = NULL;
  char rcfile[PATH_MAX + 1] = { 0 };
  size_t linelen = 0;
  pid_t pid;

  mutt_str_copy(rcfile, rcfile_path, sizeof(rcfile));

  size_t rcfilelen = mutt_str_len(rcfile);
  if (rcfilelen == 0)
    return -1;

  bool ispipe = rcfile[rcfilelen - 1] == '|';

  if (!ispipe)
  {
    struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
    if (!mutt_path_to_absolute(rcfile, np ? NONULL(np->data) : ""))
    {
      mutt_error(_("Error: Can't build path of '%s'"), rcfile_path);
      return -1;
    }

    STAILQ_FOREACH(np, &MuttrcStack, entries)
    {
      if (mutt_str_equal(np->data, rcfile))
      {
        break;
      }
    }
    if (np)
    {
      mutt_error(_("Error: Cyclic sourcing of configuration file '%s'"), rcfile);
      return -1;
    }

    mutt_list_insert_head(&MuttrcStack, mutt_str_dup(rcfile));
  }

  mutt_debug(LL_DEBUG2, "Reading configuration file '%s'\n", rcfile);

  FILE *fp = mutt_open_read(rcfile, &pid);
  if (!fp)
  {
    buf_printf(err, "%s: %s", rcfile, strerror(errno));
    return -1;
  }

  linebuf = buf_pool_get();

  const char *const c_config_charset = cs_subset_string(NeoMutt->sub, "config_charset");
  const char *const c_charset = cc_charset();
  while ((line = mutt_file_read_line(line, &linelen, fp, &lineno, MUTT_RL_CONT)) != NULL)
  {
    const bool conv = c_config_charset && c_charset;
    if (conv)
    {
      currentline = mutt_str_dup(line);
      if (!currentline)
        continue;
      mutt_ch_convert_string(&currentline, c_config_charset, c_charset, MUTT_ICONV_NO_FLAGS);
    }
    else
    {
      currentline = line;
    }

    buf_strcpy(linebuf, currentline);

    buf_reset(err);
    line_rc = parse_rc_line(linebuf, err);
    if (line_rc == MUTT_CMD_ERROR)
    {
      mutt_error("%s:%d: %s", rcfile, lineno, buf_string(err));
      if (--rc < -MAX_ERRS)
      {
        if (conv)
          FREE(&currentline);
        break;
      }
    }
    else if (line_rc == MUTT_CMD_WARNING)
    {
      /* Warning */
      mutt_warning("%s:%d: %s", rcfile, lineno, buf_string(err));
      warnings++;
    }
    else if (line_rc == MUTT_CMD_FINISH)
    {
      if (conv)
        FREE(&currentline);
      break; /* Found "finish" command */
    }
    else
    {
      if (rc < 0)
        rc = -1;
    }
    if (conv)
      FREE(&currentline);
  }

  FREE(&line);
  mutt_file_fclose(&fp);
  if (pid != -1)
    filter_wait(pid);

  if (rc)
  {
    /* the neomuttrc source keyword */
    buf_reset(err);
    buf_printf(err, (rc >= -MAX_ERRS) ? _("source: errors in %s") : _("source: reading aborted due to too many errors in %s"),
               rcfile);
    rc = -1;
  }
  else
  {
    /* Don't alias errors with warnings */
    if (warnings > 0)
    {
      buf_printf(err, ngettext("source: %d warning in %s", "source: %d warnings in %s", warnings),
                 warnings, rcfile);
      rc = -2;
    }
  }

  if (!ispipe && !STAILQ_EMPTY(&MuttrcStack))
  {
    struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
    STAILQ_REMOVE_HEAD(&MuttrcStack, entries);
    FREE(&np->data);
    FREE(&np);
  }

  buf_pool_release(&linebuf);
  return rc;
}

/**
 * parse_source - Parse the 'source' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `source <filename> [ <filename> ... ]`
 */
enum CommandResult parse_source(const struct Command *cmd, struct Buffer *line,
                                struct ParseContext *pctx, struct ConfigParseError *perr)
{
  struct Buffer *err = buf_pool_get();
  
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    if (perr)
      config_parse_error_set(perr, MUTT_CMD_WARNING, NULL, 0, "%s", buf_string(err));
    buf_pool_release(&err);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  struct Buffer *path = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  do
  {
    if (parse_extract_token(token, line, TOKEN_BACKTICK_VARS) != 0)
    {
      buf_printf(err, _("source: error at %s"), line->dptr);
      if (perr)
        config_parse_error_set(perr, MUTT_CMD_ERROR, NULL, 0, "%s", buf_string(err));
      goto done;
    }
    buf_copy(path, token);
    expand_path(path, false);

    if (pctx)
    {
      /* Use context-aware sourcing */
      struct ConfigParseError src_err = { 0 };
      config_parse_error_init(&src_err);
      if (source_rc_ctx(buf_string(path), pctx, &src_err) < 0)
      {
        buf_printf(err, _("source: file %s could not be sourced"), buf_string(path));
        if (perr)
          config_parse_error_set(perr, MUTT_CMD_ERROR, NULL, 0, "%s", buf_string(err));
        config_parse_error_free(&src_err);
        goto done;
      }
      config_parse_error_free(&src_err);
    }
    else
    {
      /* Use legacy sourcing */
      if (source_rc(buf_string(path), err) < 0)
      {
        buf_printf(err, _("source: file %s could not be sourced"), buf_string(path));
        if (perr)
          config_parse_error_set(perr, MUTT_CMD_ERROR, NULL, 0, "%s", buf_string(err));
        goto done;
      }
    }

  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&path);
  buf_pool_release(&token);
  buf_pool_release(&err);
  return rc;
}

/**
 * source_stack_cleanup - Free memory from the stack used for the source command
 */
void source_stack_cleanup(void)
{
  mutt_list_free(&MuttrcStack);
}

/**
 * parse_rc_line_cwd - Parse and run a muttrc line in a relative directory
 * @param line   Line to be parsed
 * @param cwd    File relative where to run the line
 * @param err    Where to write error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult parse_rc_line_cwd(const char *line, char *cwd, struct Buffer *err)
{
  mutt_list_insert_head(&MuttrcStack, mutt_str_dup(NONULL(cwd)));

  struct Buffer *buf = buf_pool_get();
  buf_strcpy(buf, line);
  enum CommandResult ret = parse_rc_line(buf, err);
  buf_pool_release(&buf);

  struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
  STAILQ_REMOVE_HEAD(&MuttrcStack, entries);
  FREE(&np->data);
  FREE(&np);

  return ret;
}

/**
 * mutt_get_sourced_cwd - Get the current file path that is being parsed
 * @retval ptr File path that is being parsed or cwd at runtime
 *
 * @note Caller is responsible for freeing returned string
 */
char *mutt_get_sourced_cwd(void)
{
  struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
  if (np && np->data)
    return mutt_str_dup(np->data);

  // stack is empty, return our own dummy file relative to cwd
  struct Buffer *cwd = buf_pool_get();
  mutt_path_getcwd(cwd);
  buf_addstr(cwd, "/dummy.rc");
  char *ret = buf_strdup(cwd);
  buf_pool_release(&cwd);
  return ret;
}

/**
 * source_rc_ctx - Read an initialization file using ParseContext
 * @param rcfile_path Path to initialization file
 * @param pctx        Parse context for tracking file locations
 * @param perr        Error information structure (may be NULL)
 * @retval <0 NeoMutt should pause to let the user know
 *
 * This function provides the same functionality as source_rc() but uses
 * ParseContext for tracking file locations instead of the global MuttrcStack.
 * This allows for better error reporting and detection of cyclic sourcing.
 */
int source_rc_ctx(const char *rcfile_path, struct ParseContext *pctx, struct ConfigParseError *perr)
{
  if (!pctx)
    return -1;

  int lineno = 0, rc = 0, warnings = 0;
  enum CommandResult line_rc;
  struct Buffer *linebuf = NULL;
  char *line = NULL;
  char *currentline = NULL;
  char rcfile[PATH_MAX + 1] = { 0 };
  size_t linelen = 0;
  pid_t pid;

  mutt_str_copy(rcfile, rcfile_path, sizeof(rcfile));

  size_t rcfilelen = mutt_str_len(rcfile);
  if (rcfilelen == 0)
    return -1;

  bool ispipe = rcfile[rcfilelen - 1] == '|';

  if (!ispipe)
  {
    /* Build absolute path using the context's current directory */
    const char *ctx_cwd = parse_context_cwd(pctx);
    if (!mutt_path_to_absolute(rcfile, ctx_cwd ? ctx_cwd : ""))
    {
      if (perr)
      {
        config_parse_error_set(perr, MUTT_CMD_ERROR, NULL, 0,
                               _("Error: Can't build path of '%s'"), rcfile_path);
      }
      mutt_error(_("Error: Can't build path of '%s'"), rcfile_path);
      return -1;
    }

    /* Check for cyclic sourcing using the context stack */
    if (parse_context_contains(pctx, rcfile))
    {
      if (perr)
      {
        config_parse_error_set(perr, MUTT_CMD_ERROR, rcfile, 0,
                               _("Error: Cyclic sourcing of configuration file '%s'"), rcfile);
      }
      mutt_error(_("Error: Cyclic sourcing of configuration file '%s'"), rcfile);
      return -1;
    }

    /* Push this file onto the context stack */
    parse_context_push(pctx, rcfile, 0);
  }

  mutt_debug(LL_DEBUG2, "Reading configuration file '%s'\n", rcfile);

  FILE *fp = mutt_open_read(rcfile, &pid);
  if (!fp)
  {
    if (perr)
    {
      config_parse_error_set(perr, MUTT_CMD_ERROR, rcfile, 0,
                             "%s: %s", rcfile, strerror(errno));
    }
    if (!ispipe)
      parse_context_pop(pctx);
    return -1;
  }

  linebuf = buf_pool_get();

  const char *const c_config_charset = cs_subset_string(NeoMutt->sub, "config_charset");
  const char *const c_charset = cc_charset();
  while ((line = mutt_file_read_line(line, &linelen, fp, &lineno, MUTT_RL_CONT)) != NULL)
  {
    const bool conv = c_config_charset && c_charset;
    if (conv)
    {
      currentline = mutt_str_dup(line);
      if (!currentline)
        continue;
      mutt_ch_convert_string(&currentline, c_config_charset, c_charset, MUTT_ICONV_NO_FLAGS);
    }
    else
    {
      currentline = line;
    }

    buf_strcpy(linebuf, currentline);

    /* Update the current line number in the context */
    struct FileLocation *fl = parse_context_current(pctx);
    if (fl)
      fl->lineno = lineno;

    struct ConfigParseError line_err = { 0 };
    config_parse_error_init(&line_err);
    line_rc = parse_rc_line_ctx(linebuf, pctx, &line_err);

    if (line_rc == MUTT_CMD_ERROR)
    {
      mutt_error("%s:%d: %s", rcfile, lineno, buf_string(&line_err.message));
      if (--rc < -MAX_ERRS)
      {
        config_parse_error_free(&line_err);
        if (conv)
          FREE(&currentline);
        break;
      }
    }
    else if (line_rc == MUTT_CMD_WARNING)
    {
      /* Warning */
      mutt_warning("%s:%d: %s", rcfile, lineno, buf_string(&line_err.message));
      warnings++;
    }
    else if (line_rc == MUTT_CMD_FINISH)
    {
      config_parse_error_free(&line_err);
      if (conv)
        FREE(&currentline);
      break; /* Found "finish" command */
    }
    else
    {
      if (rc < 0)
        rc = -1;
    }
    config_parse_error_free(&line_err);
    if (conv)
      FREE(&currentline);
  }

  FREE(&line);
  mutt_file_fclose(&fp);
  if (pid != -1)
    filter_wait(pid);

  if (rc)
  {
    /* the neomuttrc source keyword */
    if (perr)
    {
      config_parse_error_set(perr, MUTT_CMD_ERROR, rcfile, 0,
                             (rc >= -MAX_ERRS) ? _("source: errors in %s") :
                             _("source: reading aborted due to too many errors in %s"),
                             rcfile);
    }
    rc = -1;
  }
  else
  {
    /* Don't alias errors with warnings */
    if (warnings > 0)
    {
      if (perr)
      {
        config_parse_error_set(perr, MUTT_CMD_WARNING, rcfile, 0,
                               ngettext("source: %d warning in %s",
                                        "source: %d warnings in %s", warnings),
                               warnings, rcfile);
      }
      rc = -2;
    }
  }

  if (!ispipe)
    parse_context_pop(pctx);

  buf_pool_release(&linebuf);
  return rc;
}

/**
 * parse_rc_line_cwd_ctx - Parse and run a muttrc line with context
 * @param line  Line to be parsed
 * @param cwd   File relative where to run the line
 * @param pctx  Parse context for tracking file locations
 * @param perr  Error information structure (may be NULL)
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 *
 * This function provides the same functionality as parse_rc_line_cwd() but
 * uses ParseContext for tracking file locations.
 */
enum CommandResult parse_rc_line_cwd_ctx(const char *line, const char *cwd,
                                         struct ParseContext *pctx,
                                         struct ConfigParseError *perr)
{
  if (!pctx)
  {
    struct Buffer *err = buf_pool_get();
    struct Buffer *buf = buf_pool_get();
    buf_strcpy(buf, line);
    enum CommandResult ret = parse_rc_line(buf, err);
    buf_pool_release(&buf);
    buf_pool_release(&err);
    return ret;
  }

  parse_context_push(pctx, cwd, 0);

  struct Buffer *buf = buf_pool_get();
  buf_strcpy(buf, line);
  enum CommandResult ret = parse_rc_line_ctx(buf, pctx, perr);
  buf_pool_release(&buf);

  parse_context_pop(pctx);

  return ret;
}
