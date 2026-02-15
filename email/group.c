/**
 * @file
 * Parse Group/Lists Commands
 *
 * @authors
 * Copyright (C) 1996-2016 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019-2026 Richard Russon <rich@flatcap.org>
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
 * @page email_group Parse Group/Lists Commands
 *
 * Parse Group/Lists Commands
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "core/lib.h"
#include "group.h"
#include "parse/lib.h"
#include "module_data.h"

/**
 * parse_grouplist - Parse a group context
 * @param gl     GroupList to add to
 * @param token  Temporary Buffer space
 * @param line   Buffer containing string to be parsed
 * @param err    Buffer for error messages
 * @param groups Groups HashTable
 * @retval  0 Success
 * @retval -1 Error
 */
int parse_grouplist(struct GroupList *gl, struct Buffer *token, struct Buffer *line,
                    struct Buffer *err, struct HashTable *groups)
{
  while (mutt_istr_equal(token->data, "-group"))
  {
    if (!MoreArgs(line))
    {
      buf_strcpy(err, _("-group: no group name"));
      return -1;
    }

    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    grouplist_add_group(gl, groups_get_group(groups, token->data));

    if (!MoreArgs(line))
    {
      buf_strcpy(err, _("out of arguments"));
      return -1;
    }

    parse_extract_token(token, line, TOKEN_NO_FLAGS);
  }

  return 0;
}

/**
 * parse_group - Parse the 'group' and 'ungroup' commands - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `group [ -group <name> ... ] { -rx <regex> ... | -addr <address> ... }`
 * - `ungroup [ -group <name> ... ] { * | -rx <regex> ... | -addr <address> ... }`
 */
enum CommandResult parse_group(const struct Command *cmd, struct Buffer *line,
                               const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);
  enum GroupState gstate = GS_NONE;
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    if (parse_grouplist(&gl, token, line, err, NeoMutt->groups) == -1)
      goto done;

    if ((cmd->id == CMD_UNGROUP) && mutt_istr_equal(buf_string(token), "*"))
    {
      groups_remove_grouplist(NeoMutt->groups, &gl);
      rc = MUTT_CMD_SUCCESS;
      goto done;
    }

    if (mutt_istr_equal(buf_string(token), "-rx"))
    {
      gstate = GS_RX;
    }
    else if (mutt_istr_equal(buf_string(token), "-addr"))
    {
      gstate = GS_ADDR;
    }
    else
    {
      switch (gstate)
      {
        case GS_NONE:
          buf_printf(err, _("%s: missing -rx or -addr"), cmd->name);
          rc = MUTT_CMD_WARNING;
          goto done;

        case GS_RX:
          if ((cmd->id == CMD_GROUP) &&
              (grouplist_add_regex(&gl, buf_string(token), REG_ICASE, err) != 0))
          {
            goto done;
          }
          else if ((cmd->id == CMD_UNGROUP) &&
                   (groups_remove_regex(NeoMutt->groups, &gl, buf_string(token)) < 0))
          {
            goto done;
          }
          break;

        case GS_ADDR:
        {
          char *estr = NULL;
          struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
          mutt_addrlist_parse2(&al, buf_string(token));
          if (TAILQ_EMPTY(&al))
            goto done;
          if (mutt_addrlist_to_intl(&al, &estr))
          {
            buf_printf(err, _("%s: warning: bad IDN '%s'"), cmd->name, estr);
            mutt_addrlist_clear(&al);
            FREE(&estr);
            goto done;
          }
          if (cmd->id == CMD_GROUP)
            grouplist_add_addrlist(&gl, &al);
          else if (cmd->id == CMD_UNGROUP)
            groups_remove_addrlist(NeoMutt->groups, &gl, &al);
          mutt_addrlist_clear(&al);
          break;
        }
      }
    }
  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  grouplist_destroy(&gl);
  return rc;
}

/**
 * parse_lists - Parse the 'lists' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `lists [ -group <name> ... ] <regex> [ <regex> ... ]`
 */
enum CommandResult parse_lists(const struct Command *cmd, struct Buffer *line,
                               const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  struct EmailModuleData *md = neomutt_get_module_data(NeoMutt, MODULE_ID_EMAIL);
  ASSERT(md);

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    if (parse_grouplist(&gl, token, line, err, NeoMutt->groups) == -1)
      goto done;

    mutt_regexlist_remove(&md->unmail, buf_string(token));

    if (mutt_regexlist_add(&md->mail, buf_string(token), REG_ICASE, err) != 0)
      goto done;

    if (grouplist_add_regex(&gl, buf_string(token), REG_ICASE, err) != 0)
      goto done;
  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  grouplist_destroy(&gl);
  return rc;
}

/**
 * parse_subscribe - Parse the 'subscribe' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `subscribe [ -group <name> ... ] <regex> [ <regex> ... ]`
 */
enum CommandResult parse_subscribe(const struct Command *cmd, struct Buffer *line,
                                   const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  struct EmailModuleData *md = neomutt_get_module_data(NeoMutt, MODULE_ID_EMAIL);
  ASSERT(md);

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    if (parse_grouplist(&gl, token, line, err, NeoMutt->groups) == -1)
      goto done;

    mutt_regexlist_remove(&md->unmail, buf_string(token));
    mutt_regexlist_remove(&md->unsubscribed, buf_string(token));

    if (mutt_regexlist_add(&md->mail, buf_string(token), REG_ICASE, err) != 0)
      goto done;

    if (mutt_regexlist_add(&md->subscribed, buf_string(token), REG_ICASE, err) != 0)
      goto done;

    if (grouplist_add_regex(&gl, buf_string(token), REG_ICASE, err) != 0)
      goto done;
  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  grouplist_destroy(&gl);
  return rc;
}

/**
 * parse_unlists - Parse the 'unlists' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unlists { * | <regex> ... }`
 */
enum CommandResult parse_unlists(const struct Command *cmd, struct Buffer *line,
                                 const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  struct EmailModuleData *md = neomutt_get_module_data(NeoMutt, MODULE_ID_EMAIL);
  ASSERT(md);

  mutt_hash_free(&md->auto_subscribe_cache);
  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    mutt_regexlist_remove(&md->subscribed, buf_string(token));
    mutt_regexlist_remove(&md->mail, buf_string(token));

    if (!mutt_str_equal(buf_string(token), "*") &&
        (mutt_regexlist_add(&md->unmail, buf_string(token), REG_ICASE, err) != 0))
    {
      goto done;
    }
  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_unsubscribe - Parse the 'unsubscribe' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unsubscribe { * | <regex> ... }`
 */
enum CommandResult parse_unsubscribe(const struct Command *cmd, struct Buffer *line,
                                     const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct EmailModuleData *md = neomutt_get_module_data(NeoMutt, MODULE_ID_EMAIL);
  ASSERT(md);

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  mutt_hash_free(&md->auto_subscribe_cache);
  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    mutt_regexlist_remove(&md->subscribed, buf_string(token));

    if (!mutt_str_equal(buf_string(token), "*") &&
        (mutt_regexlist_add(&md->unsubscribed, buf_string(token), REG_ICASE, err) != 0))
    {
      goto done;
    }
  } while (MoreArgs(line));

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}
