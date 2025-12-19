/**
 * @file
 * Alias commands
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * @page alias_commands Alias commands
 *
 * Alias commands
 */

#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "commands.h"
#include "lib.h"
#include "parse/lib.h"
#include "alias.h"
#include "reverse.h"

/**
 * alias_tags_to_buffer - Write a comma-separated list of tags to a Buffer
 * @param tl  Tags
 * @param buf Buffer for the result
 */
void alias_tags_to_buffer(struct TagList *tl, struct Buffer *buf)
{
  struct Tag *tag = NULL;
  STAILQ_FOREACH(tag, tl, entries)
  {
    buf_addstr(buf, tag->name);
    if (STAILQ_NEXT(tag, entries))
      buf_addch(buf, ',');
  }
}

/**
 * parse_alias_tags - Parse a comma-separated list of tags
 * @param tags Comma-separated string
 * @param tl   TagList for the results
 */
void parse_alias_tags(const char *tags, struct TagList *tl)
{
  if (!tags || !tl)
    return;

  struct Slist *sl = slist_parse(tags, D_SLIST_SEP_COMMA);
  if (slist_is_empty(sl))
  {
    slist_free(&sl);
    return;
  }

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &sl->head, entries)
  {
    struct Tag *tag = tag_new();
    tag->name = np->data; // Transfer string
    np->data = NULL;
    STAILQ_INSERT_TAIL(tl, tag, entries);
  }
  slist_free(&sl);
}

/**
 * parse_alias_comments - Parse the alias/query comment field
 * @param alias Alias for the result
 * @param com   Comment string
 *
 * If the comment contains a 'tags:' field, the result will be put in alias.tags
 */
void parse_alias_comments(struct Alias *alias, const char *com)
{
  if (!com || (com[0] == '\0'))
    return;

  const regmatch_t *match = mutt_prex_capture(PREX_ALIAS_TAGS, com);
  if (match)
  {
    const regmatch_t *pre = &match[PREX_ALIAS_TAGS_MATCH_PRE];
    const regmatch_t *tags = &match[PREX_ALIAS_TAGS_MATCH_TAGS];
    const regmatch_t *post = &match[PREX_ALIAS_TAGS_MATCH_POST];

    struct Buffer *tmp = buf_pool_get();

    // Extract the tags
    buf_addstr_n(tmp, com + mutt_regmatch_start(tags),
                 mutt_regmatch_end(tags) - mutt_regmatch_start(tags));
    parse_alias_tags(buf_string(tmp), &alias->tags);
    buf_reset(tmp);

    // Collect all the other text as "comments"
    buf_addstr_n(tmp, com + mutt_regmatch_start(pre),
                 mutt_regmatch_end(pre) - mutt_regmatch_start(pre));
    buf_addstr_n(tmp, com + mutt_regmatch_start(post),
                 mutt_regmatch_end(post) - mutt_regmatch_start(post));
    alias->comment = buf_strdup(tmp);

    buf_pool_release(&tmp);
  }
  else
  {
    alias->comment = mutt_str_dup(com);
  }
}

/**
 * parse_alias - Parse the 'alias' command - Implements Command::parse() - @ingroup command_parse
 *
 * e.g. "alias jim James Smith <js@example.com> # Pointy-haired boss"
 */
enum CommandResult parse_alias(const struct Command *cmd, struct Buffer *token_,
                               struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Alias *tmp = NULL;
  enum NotifyAlias event;
  struct GroupList gl = STAILQ_HEAD_INITIALIZER(gl);
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  /* name */
  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  mutt_debug(LL_DEBUG5, "First token is '%s'\n", buf_string(token));
  if (parse_grouplist(&gl, token, line, err, NeoMutt->groups) == -1)
    goto done;

  char *name = mutt_str_dup(buf_string(token));

  /* address list */
  parse_extract_token(token, line, TOKEN_QUOTE | TOKEN_SPACE | TOKEN_SEMICOLON);
  mutt_debug(LL_DEBUG5, "Second token is '%s'\n", buf_string(token));
  struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
  int parsed = mutt_addrlist_parse2(&al, buf_string(token));
  if (parsed == 0)
  {
    buf_printf(err, _("Warning: Bad address '%s' in alias '%s'"), buf_string(token), name);
    FREE(&name);
    goto done;
  }

  /* IDN */
  char *estr = NULL;
  if (mutt_addrlist_to_intl(&al, &estr))
  {
    buf_printf(err, _("Warning: Bad IDN '%s' in alias '%s'"), estr, name);
    FREE(&name);
    FREE(&estr);
    goto done;
  }

  /* check to see if an alias with this name already exists */
  TAILQ_FOREACH(tmp, &Aliases, entries)
  {
    if (mutt_istr_equal(tmp->name, name))
      break;
  }

  if (tmp)
  {
    FREE(&name);
    alias_reverse_delete(tmp);
    /* override the previous value */
    mutt_addrlist_clear(&tmp->addr);
    FREE(&tmp->comment);
    event = NT_ALIAS_CHANGE;
  }
  else
  {
    /* create a new alias */
    tmp = alias_new();
    tmp->name = name;
    TAILQ_INSERT_TAIL(&Aliases, tmp, entries);
    event = NT_ALIAS_ADD;
  }
  tmp->addr = al;

  grouplist_add_addrlist(&gl, &tmp->addr);

  const short c_debug_level = cs_subset_number(NeoMutt->sub, "debug_level");
  if (c_debug_level > LL_DEBUG4)
  {
    /* A group is terminated with an empty address, so check a->mailbox */
    struct Address *a = NULL;
    TAILQ_FOREACH(a, &tmp->addr, entries)
    {
      if (!a->mailbox)
        break;

      if (a->group)
        mutt_debug(LL_DEBUG5, "  Group %s\n", buf_string(a->mailbox));
      else
        mutt_debug(LL_DEBUG5, "  %s\n", buf_string(a->mailbox));
    }
  }

  if (!MoreArgs(line) && (line->dptr[0] == '#'))
  {
    line->dptr++; // skip over the "# "
    if (*line->dptr == ' ')
      line->dptr++;

    parse_alias_comments(tmp, line->dptr);
    *line->dptr = '\0'; // We're done parsing
  }

  alias_reverse_add(tmp);

  mutt_debug(LL_NOTIFY, "%s: %s\n",
             (event == NT_ALIAS_ADD) ? "NT_ALIAS_ADD" : "NT_ALIAS_CHANGE", tmp->name);
  struct EventAlias ev_a = { tmp };
  notify_send(NeoMutt->notify, NT_ALIAS, event, &ev_a);

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  grouplist_destroy(&gl);
  return rc;
}

/**
 * parse_unalias - Parse the 'unalias' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult parse_unalias(const struct Command *cmd, struct Buffer *token_,
                                 struct Buffer *line, struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    struct Alias *np = NULL;
    if (mutt_str_equal("*", buf_string(token)))
    {
      TAILQ_FOREACH(np, &Aliases, entries)
      {
        alias_reverse_delete(np);
      }

      aliaslist_clear(&Aliases);
      goto done;
    }

    TAILQ_FOREACH(np, &Aliases, entries)
    {
      if (!mutt_istr_equal(buf_string(token), np->name))
        continue;

      TAILQ_REMOVE(&Aliases, np, entries);
      alias_reverse_delete(np);
      alias_free(&np);
      break;
    }
  } while (MoreArgs(line));

done:
  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}
