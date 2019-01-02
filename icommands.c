/**
 * @file
 * Information commands
 *
 * @authors
 * Copyright (C) 2016 Christopher John Czettel <chris@meicloud.at>
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

#include "config.h"
#include "mutt/mutt.h"
#include "icommands.h"
#include "globals.h"
#include "muttlib.h"
#include "pager.h"
#include "protos.h"
#include "summary.h"
#include "version.h"

/* prototypes for interactive commands */
static int icmd_test(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
static int icmd_quit(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
static int icmd_bind(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
static int icmd_color(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
static int icmd_help(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
static int icmd_messages(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
static int icmd_scripts(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
static int icmd_version(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
static int icmd_vars(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
static int icmd_set(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
/* WARNING: set is already defined and would be overriden, therfore changed name to vars */

/* lookup table for all available interactive commands
 * be aware, that these command take precendence over conventional mutt rc-lines
 */
const struct ICommand ICommandList[] = {
  { "test", icmd_test, 0 },
  { "quit", icmd_quit, 0 },
  { "q", icmd_quit, 0 },
  { "q!", icmd_quit, 0 },
  { "qa", icmd_quit, 0 },
  { "bind", icmd_bind, 0 },
  { "macro", icmd_bind, 0 },
  { "color", icmd_color, 0 },
  { "help", icmd_help, 0 },
  { "messages", icmd_messages, 0 },
  { "scripts", icmd_scripts, 0 },
  { "version", icmd_version, 0 },
  { "vars", icmd_vars, 0 },
  { "set", icmd_set, 0 },
  { NULL, NULL, 0 } /* important for end of loop conditions */
};

/* WARNING: this is a simplified clone from init.c:mutt_parse_command   */
/* TODO: replace it with function more appropriate/optimized for ICommandList */
/*
 * line     command to execute
 * err      where to write error messages
 * @retval  0 Success
 * @retval -1 Error (no message): command not found
 * @retval -1 Error with message: command failed
 * @retval -2 Warning with message: command failed
 */
int neomutt_parse_icommand(/* const */ char *line, struct Buffer *err)
{
  if (!line || !*line || !err)
    return -1;

  int i, rc = 0;

  struct Buffer expn, token;

  mutt_buffer_init(&expn);
  mutt_buffer_init(&token);

  expn.data = expn.dptr = line;
  expn.dsize = mutt_str_strlen(line);

  mutt_buffer_reset(err);

  SKIPWS(expn.dptr);
  while (*expn.dptr)
  {
    /* TODO: contemplate implementing a icommand specific tokenizer */
    mutt_extract_token(&token, &expn, 0);
    for (i = 0; ICommandList[i].name; i++)
    {
      if (mutt_str_strcmp(token.data, ICommandList[i].name) != 0)
        continue;

      rc = ICommandList[i].func(&token, &expn, ICommandList[i].data, err);
      if (rc != 0)
        goto finish;

      break; /* Continue with next command */
    }
  }

finish:
  if (expn.destroy)
    FREE(&expn.data);
  return rc;
}


/*
 *  wrapper functions to prepare and call other functionality within mutt
 *  see icmd_quit and icmd_help for easy examples
 */
static int icmd_quit(struct Buffer *buf, struct Buffer *s, unsigned long data,
                     struct Buffer *err)
{
  /* TODO: exit more gracefully */
  mutt_exit(0);
  return 0;
}


static int icmd_help(struct Buffer *buf, struct Buffer *s, unsigned long data,
                     struct Buffer *err)
{
  /* TODO: implement ':help' command as suggested by flatcap in #162 */
  mutt_buffer_addstr(err, _("Not implemented yet."));
  return -1;
}


static int icmd_test(struct Buffer *buf, struct Buffer *s, unsigned long data,
                     struct Buffer *err)
{
  mutt_summary();
  return 0;
}


static int icmd_bind(struct Buffer *buf, struct Buffer *s, unsigned long data,
                     struct Buffer *err)
{
  /* TODO: implement ':bind' and ':macro' command as suggested by flatcap in #162 */
  mutt_buffer_addstr(err, _("Not implemented yet."));
  return -1;
}

static int icmd_color(struct Buffer *buf, struct Buffer *s, unsigned long data,
                      struct Buffer *err)

{
  /* TODO: implement ':color' command as suggested by flatcap in #162 */
  mutt_buffer_addstr(err, _("Not implemented yet."));
  return -1;
}


static int icmd_messages(struct Buffer *buf, struct Buffer *s,
                         unsigned long data, struct Buffer *err)
{
  /* TODO: implement ':messages' command as suggested by flatcap in #162 */
  mutt_buffer_addstr(err, _("Not implemented yet."));
  return -1;
}


static int icmd_scripts(struct Buffer *buf, struct Buffer *s,
                        unsigned long data, struct Buffer *err)
{
  /* TODO: implement ':scripts' command as suggested by flatcap in #162 */
  mutt_buffer_addstr(err, _("Not implemented yet."));
  return -1;
}


static int icmd_vars(struct Buffer *buf, struct Buffer *s, unsigned long data,
                     struct Buffer *err)
{
  /* TODO: implement ':vars' command as suggested by flatcap in #162 */
  mutt_buffer_addstr(err, _("Not implemented yet."));
  return -1;
}

static int icmd_version(struct Buffer *buf, struct Buffer *s,
                        unsigned long data, struct Buffer *err)
{
  char tempfile[PATH_MAX];
  FILE *fpout = NULL;

  mutt_mktemp(tempfile, sizeof(tempfile));
  fpout = mutt_file_fopen(tempfile, "w");
  if (!fpout)
  {
    mutt_buffer_addstr(err, _("Could not create temporary file"));
    return -1;
  }

  print_version_to_file(fpout);
  fflush(fpout);

  struct Pager info = { 0 };
  if (mutt_pager("version", tempfile, 0, &info) == -1)
  {
    mutt_buffer_addstr(err, _("Could not create temporary file"));
    return -1;
  }

  return 0;
}

static int icmd_set(struct Buffer *buf, struct Buffer *s, unsigned long data,
                    struct Buffer *err)
{
  char tempfile[PATH_MAX];
  FILE *fpout = NULL;

  mutt_mktemp(tempfile, sizeof(tempfile));
  fpout = mutt_file_fopen(tempfile, "w");
  if (!fpout)
  {
    mutt_buffer_addstr(err, _("Could not create temporary file"));
    return -1;
  }

  if (mutt_str_strcmp(s->data, "set all") == 0)
  {
    dump_config(Config, CS_DUMP_STYLE_NEO | CS_DUMP_STYLE_FILE, 0, fpout);
  }
  else if (mutt_str_strcmp(s->data, "set") == 0)
  {
    dump_config(Config, CS_DUMP_STYLE_NEO | CS_DUMP_STYLE_FILE, CS_DUMP_ONLY_CHANGED, fpout);
  }
  else
  {
    return -1;
  }

  fflush(fpout);

  struct Pager info = { 0 };
  if (mutt_pager("set", tempfile, 0, &info) == -1)
  {
    mutt_buffer_addstr(err, _("Could not create temporary file"));
    return -1;
  }

  return 0;
}
