/**
 * @file
 * Config/command parsing
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013,2016 Michael R. Elkins <me@mutt.org>
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
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <wchar.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "init.h"
#include "alias.h"
#include "context.h"
#include "envelope.h"
#include "filter.h"
#include "group.h"
#include "hcache/hcache.h"
#include "header.h"
#include "history.h"
#include "keymap.h"
#include "mailbox.h"
#include "mbtable.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "mx.h"
#include "myvar.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "pattern.h"
#include "sidebar.h"
#include "version.h"
#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif
#ifdef USE_IMAP
#include "imap/imap.h" /* for imap_subscribe() */
#endif

#define CHECK_PAGER                                                                  \
  if ((CurrentMenu == MENU_PAGER) && (idx >= 0) && (MuttVars[idx].flags & R_RESORT)) \
  {                                                                                  \
    snprintf(err->data, err->dsize, "%s", _("Not available in this menu."));         \
    return -1;                                                                       \
  }

/**
 * struct MyVar - A user-set variable
 */
struct MyVar
{
  char *name;
  char *value;
  struct MyVar *next;
};

static struct MyVar *MyVars;

void myvar_set(const char *var, const char *val)
{
  struct MyVar **cur = NULL;

  for (cur = &MyVars; *cur; cur = &((*cur)->next))
    if (mutt_str_strcmp((*cur)->name, var) == 0)
      break;

  if (!*cur)
    *cur = mutt_mem_calloc(1, sizeof(struct MyVar));

  if (!(*cur)->name)
    (*cur)->name = mutt_str_strdup(var);

  mutt_str_replace(&(*cur)->value, val);
}

static void myvar_del(const char *var)
{
  struct MyVar **cur = NULL;
  struct MyVar *tmp = NULL;

  for (cur = &MyVars; *cur; cur = &((*cur)->next))
    if (mutt_str_strcmp((*cur)->name, var) == 0)
      break;

  if (*cur)
  {
    tmp = (*cur)->next;
    FREE(&(*cur)->name);
    FREE(&(*cur)->value);
    FREE(cur);
    *cur = tmp;
  }
}

#ifdef USE_NOTMUCH
/* List of tags found in last call to mutt_nm_query_complete(). */
static char **nm_tags;
#endif

#ifndef DOMAIN
/**
 * getmailname - Try to retrieve the FQDN from mailname files
 * @retval ptr Heap allocated string with the FQDN
 * @retval NULL if no valid mailname file could be read
 */
static char *getmailname(void)
{
  char *mailname = NULL;
  static const char *mn_files[] = { "/etc/mailname", "/etc/mail/mailname" };

  for (size_t i = 0; i < mutt_array_size(mn_files); i++)
  {
    FILE *f = mutt_file_fopen(mn_files[i], "r");
    if (!f)
      continue;

    size_t len = 0;
    mailname = mutt_file_read_line(NULL, &len, f, NULL, 0);
    mutt_file_fclose(&f);
    if (mailname && *mailname)
      break;

    FREE(&mailname);
  }

  return mailname;
}
#endif

static int toggle_quadoption(int opt)
{
  /* toggle the low bit
   * MUTT_NO    <--> MUTT_YES
   * MUTT_ASKNO <--> MUTT_ASKYES */
  return opt ^= 1;
}

static int parse_regex(int idx, struct Buffer *tmp, struct Buffer *err)
{
  struct Regex **ptr = (struct Regex **) MuttVars[idx].var;

  if (*ptr)
  {
    /* Same pattern as we already have */
    if (mutt_str_strcmp((*ptr)->pattern, tmp->data) == 0)
      return 0;
  }

  if (mutt_buffer_is_empty(tmp))
  {
    mutt_regex_free(ptr);
    return 0;
  }

  struct Regex *rnew = mutt_regex_create(tmp->data, MuttVars[idx].type, err);
  if (!rnew)
    return 1;

  mutt_regex_free(ptr);
  *ptr = rnew;
  return 0;
}

int query_quadoption(int opt, const char *prompt)
{
  switch (opt)
  {
    case MUTT_YES:
    case MUTT_NO:
      return opt;

    default:
      opt = mutt_yesorno(prompt, (opt == MUTT_ASKYES));
      mutt_window_clearline(MuttMessageWindow, 0);
      return opt;
  }

  /* not reached */
}

/**
 * mutt_option_index - Find the index (in rc_vars) of a variable name
 * @param s Variable name to search for
 * @retval -1 on error
 * @retval >0 on success
 */
int mutt_option_index(const char *s)
{
  for (int i = 0; MuttVars[i].name; i++)
    if (mutt_str_strcmp(s, MuttVars[i].name) == 0)
      return (MuttVars[i].type == DT_SYNONYM ?
                  mutt_option_index((char *) MuttVars[i].initial) :
                  i);
  return -1;
}

#ifdef USE_LUA
int mutt_option_to_string(const struct Option *opt, char *val, size_t len)
{
  mutt_debug(2, " * mutt_option_to_string(%s)\n", NONULL((char *) opt->var));
  int idx = mutt_option_index((const char *) opt->name);
  if (idx != -1)
    return var_to_string(idx, val, len);
  return 0;
}

bool mutt_option_get(const char *s, struct Option *opt)
{
  mutt_debug(2, " * mutt_option_get(%s)\n", s);
  int idx = mutt_option_index(s);
  if (idx != -1)
  {
    if (opt)
      *opt = MuttVars[idx];
    return true;
  }

  if (mutt_str_strncmp("my_", s, 3) == 0)
  {
    const char *mv = myvar_get(s);
    if (!mv)
      return false;

    if (opt)
    {
      memset(opt, 0, sizeof(*opt));
      opt->name = s;
      opt->type = DT_STRING;
      opt->initial = (intptr_t) mv;
    }
    return true;
  }
  return false;
}
#endif

static void free_mbtable(struct MbTable **t)
{
  if (!t || !*t)
    return;

  FREE(&(*t)->chars);
  FREE(&(*t)->segmented_str);
  FREE(&(*t)->orig_str);
  FREE(t);
}

static struct MbTable *parse_mbtable(const char *s)
{
  size_t slen, k;
  mbstate_t mbstate;
  char *d = NULL;

  struct MbTable *t = mutt_mem_calloc(1, sizeof(struct MbTable));
  slen = mutt_str_strlen(s);
  if (slen == 0)
    return t;

  t->orig_str = mutt_str_strdup(s);
  /* This could be more space efficient.  However, being used on tiny
   * strings (ToChars and StatusChars), the overhead is not great. */
  t->chars = mutt_mem_calloc(slen, sizeof(char *));
  d = t->segmented_str = mutt_mem_calloc(slen * 2, sizeof(char));

  memset(&mbstate, 0, sizeof(mbstate));
  while (slen && (k = mbrtowc(NULL, s, slen, &mbstate)))
  {
    if (k == (size_t)(-1) || k == (size_t)(-2))
    {
      mutt_debug(1, "mbrtowc returned %d converting %s in %s\n",
                 (k == (size_t)(-1)) ? -1 : -2, s, t->orig_str);
      if (k == (size_t)(-1))
        memset(&mbstate, 0, sizeof(mbstate));
      k = (k == (size_t)(-1)) ? 1 : slen;
    }

    slen -= k;
    t->chars[t->len++] = d;
    while (k--)
      *d++ = *s++;
    *d++ = '\0';
  }

  return t;
}

static int parse_sort(short *val, const char *s, const struct Mapping *map, struct Buffer *err)
{
  int i, flags = 0;

  if (mutt_str_strncmp("reverse-", s, 8) == 0)
  {
    s += 8;
    flags = SORT_REVERSE;
  }

  if (mutt_str_strncmp("last-", s, 5) == 0)
  {
    s += 5;
    flags |= SORT_LAST;
  }

  i = mutt_map_get_value(s, map);
  if (i == -1)
  {
    snprintf(err->data, err->dsize, _("%s: unknown sorting method"), s);
    return -1;
  }

  *val = i | flags;

  return 0;
}

#ifdef USE_LUA
int mutt_option_set(const struct Option *val, struct Buffer *err)
{
  mutt_debug(2, " * mutt_option_set()\n");
  int idx = mutt_option_index(val->name);
  if (idx != -1)
  {
    switch (DTYPE(MuttVars[idx].type))
    {
      case DT_REGEX:
      {
        char err_str[LONG_STRING] = "";
        struct Buffer err2;
        err2.data = err_str;
        err2.dsize = sizeof(err_str);

        struct Buffer tmp;
        tmp.data = (char *) val->var;
        tmp.dsize = strlen((char *) val->var);

        if (parse_regex(idx, &tmp, &err2))
        {
          /* $reply_regex and $alternates require special treatment */
          if (Context && Context->msgcount &&
              (mutt_str_strcmp(MuttVars[idx].name, "reply_regex") == 0))
          {
            regmatch_t pmatch[1];

            for (int i = 0; i < Context->msgcount; i++)
            {
              struct Envelope *e = Context->hdrs[i]->env;
              if (e && e->subject)
              {
                e->real_subj = e->subject;
                if (ReplyRegex && ReplyRegex->regex &&
                    (regexec(ReplyRegex->regex, e->subject, 1, pmatch, 0) == 0))
                {
                  e->subject += pmatch[0].rm_eo;
                }
              }
            }
          }
        }
        else
        {
          snprintf(err->data, err->dsize, _("%s: Unknown type."), MuttVars[idx].name);
          return -1;
        }
        break;
      }
      case DT_SORT:
      {
        const struct Mapping *map = NULL;

        switch (MuttVars[idx].type & DT_SUBTYPE_MASK)
        {
          case DT_SORT_ALIAS:
            map = SortAliasMethods;
            break;
          case DT_SORT_BROWSER:
            map = SortBrowserMethods;
            break;
          case DT_SORT_KEYS:
            if (WithCrypto & APPLICATION_PGP)
              map = SortKeyMethods;
            break;
          case DT_SORT_AUX:
            map = SortAuxMethods;
            break;
          case DT_SORT_SIDEBAR:
            map = SortSidebarMethods;
            break;
          default:
            map = SortMethods;
            break;
        }

        if (!map)
        {
          snprintf(err->data, err->dsize, _("%s: Unknown type."), MuttVars[idx].name);
          return -1;
        }

        if (parse_sort((short *) MuttVars[idx].var, (const char *) val->var, map, err) == -1)
        {
          return -1;
        }
      }
      break;
      case DT_MBTABLE:
      {
        struct MbTable **tbl = (struct MbTable **) MuttVars[idx].var;
        free_mbtable(tbl);
        *tbl = parse_mbtable((const char *) val->var);
      }
      break;
      case DT_ADDRESS:
        mutt_addr_free((struct Address **) MuttVars[idx].var);
        *((struct Address **) MuttVars[idx].var) =
            mutt_addr_parse_list(NULL, (const char *) val->var);
        break;
      case DT_PATH:
      {
        char scratch[LONG_STRING];
        mutt_str_strfcpy(scratch, NONULL((const char *) val->var), sizeof(scratch));
        mutt_expand_path(scratch, sizeof(scratch));
        /* MuttVars[idx].var is already 'char**' (or some 'void**') or...
         * so cast to 'void*' is okay */
        FREE((void *) MuttVars[idx].var);
        *((char **) MuttVars[idx].var) = mutt_str_strdup(scratch);
        break;
      }
      case DT_STRING:
      {
        /* MuttVars[idx].var is already 'char**' (or some 'void**') or...
         * so cast to 'void*' is okay */
        FREE((void *) MuttVars[idx].var);
        *((char **) MuttVars[idx].var) = mutt_str_strdup((char *) val->var);
      }
      break;
      case DT_BOOL:
        if (val->var)
          *(bool *) MuttVars[idx].var = true;
        else
          *(bool *) MuttVars[idx].var = false;
        break;
      case DT_QUAD:
        *(short *) MuttVars[idx].var = (long) val->var;
        break;
      case DT_NUMBER:
        *(short *) MuttVars[idx].var = (long) val->var;
        break;
      default:
        return -1;
    }
  }
  /* set the string as a myvar if it's one */
  if (mutt_str_strncmp("my_", val->name, 3) == 0)
  {
    myvar_set(val->name, (const char *) val->var);
  }
  return 0;
}
#endif

int mutt_extract_token(struct Buffer *dest, struct Buffer *tok, int flags)
{
  if (!dest || !tok)
    return -1;

  char ch;
  char qc = 0; /* quote char */
  char *pc = NULL;

  /* reset the destination pointer to the beginning of the buffer */
  dest->dptr = dest->data;

  SKIPWS(tok->dptr);
  while ((ch = *tok->dptr))
  {
    if (!qc)
    {
      if ((ISSPACE(ch) && !(flags & MUTT_TOKEN_SPACE)) ||
          (ch == '#' && !(flags & MUTT_TOKEN_COMMENT)) ||
          (ch == '=' && (flags & MUTT_TOKEN_EQUAL)) ||
          (ch == ';' && !(flags & MUTT_TOKEN_SEMICOLON)) ||
          ((flags & MUTT_TOKEN_PATTERN) && strchr("~%=!|", ch)))
        break;
    }

    tok->dptr++;

    if (ch == qc)
      qc = 0; /* end of quote */
    else if (!qc && (ch == '\'' || ch == '"') && !(flags & MUTT_TOKEN_QUOTE))
      qc = ch;
    else if (ch == '\\' && qc != '\'')
    {
      if (!*tok->dptr)
        return -1; /* premature end of token */
      switch (ch = *tok->dptr++)
      {
        case 'c':
        case 'C':
          if (!*tok->dptr)
            return -1; /* premature end of token */
          mutt_buffer_addch(dest, (toupper((unsigned char) *tok->dptr) - '@') & 0x7f);
          tok->dptr++;
          break;
        case 'e':
          mutt_buffer_addch(dest, '\033');
          break;
        case 'f':
          mutt_buffer_addch(dest, '\f');
          break;
        case 'n':
          mutt_buffer_addch(dest, '\n');
          break;
        case 'r':
          mutt_buffer_addch(dest, '\r');
          break;
        case 't':
          mutt_buffer_addch(dest, '\t');
          break;
        default:
          if (isdigit((unsigned char) ch) && isdigit((unsigned char) *tok->dptr) &&
              isdigit((unsigned char) *(tok->dptr + 1)))
          {
            mutt_buffer_addch(dest, (ch << 6) + (*tok->dptr << 3) + *(tok->dptr + 1) - 3504);
            tok->dptr += 2;
          }
          else
            mutt_buffer_addch(dest, ch);
      }
    }
    else if (ch == '^' && (flags & MUTT_TOKEN_CONDENSE))
    {
      if (!*tok->dptr)
        return -1; /* premature end of token */
      ch = *tok->dptr++;
      if (ch == '^')
        mutt_buffer_addch(dest, ch);
      else if (ch == '[')
        mutt_buffer_addch(dest, '\033');
      else if (isalpha((unsigned char) ch))
        mutt_buffer_addch(dest, toupper((unsigned char) ch) - '@');
      else
      {
        mutt_buffer_addch(dest, '^');
        mutt_buffer_addch(dest, ch);
      }
    }
    else if (ch == '`' && (!qc || qc == '"'))
    {
      FILE *fp = NULL;
      pid_t pid;
      char *cmd = NULL, *ptr = NULL;
      size_t expnlen;
      struct Buffer expn;
      int line = 0;

      pc = tok->dptr;
      do
      {
        pc = strpbrk(pc, "\\`");
        if (pc)
        {
          /* skip any quoted chars */
          if (*pc == '\\')
            pc += 2;
        }
      } while (pc && *pc != '`');
      if (!pc)
      {
        mutt_debug(1, "mismatched backticks\n");
        return -1;
      }
      cmd = mutt_str_substr_dup(tok->dptr, pc);
      pid = mutt_create_filter(cmd, NULL, &fp, NULL);
      if (pid < 0)
      {
        mutt_debug(1, "unable to fork command: %s\n", cmd);
        FREE(&cmd);
        return -1;
      }
      FREE(&cmd);

      tok->dptr = pc + 1;

      /* read line */
      mutt_buffer_init(&expn);
      expn.data = mutt_file_read_line(NULL, &expn.dsize, fp, &line, 0);
      mutt_file_fclose(&fp);
      mutt_wait_filter(pid);

      /* if we got output, make a new string consisting of the shell output
         plus whatever else was left on the original line */
      /* BUT: If this is inside a quoted string, directly add output to
       * the token */
      if (expn.data && qc)
      {
        mutt_buffer_addstr(dest, expn.data);
        FREE(&expn.data);
      }
      else if (expn.data)
      {
        expnlen = mutt_str_strlen(expn.data);
        tok->dsize = expnlen + mutt_str_strlen(tok->dptr) + 1;
        ptr = mutt_mem_malloc(tok->dsize);
        memcpy(ptr, expn.data, expnlen);
        strcpy(ptr + expnlen, tok->dptr);
        if (tok->destroy)
          FREE(&tok->data);
        tok->data = ptr;
        tok->dptr = ptr;
        tok->destroy = 1; /* mark that the caller should destroy this data */
        ptr = NULL;
        FREE(&expn.data);
      }
    }
    else if (ch == '$' && (!qc || qc == '"') &&
             (*tok->dptr == '{' || isalpha((unsigned char) *tok->dptr)))
    {
      const char *env = NULL;
      char *var = NULL;

      if (*tok->dptr == '{')
      {
        tok->dptr++;
        pc = strchr(tok->dptr, '}');
        if (pc)
        {
          var = mutt_str_substr_dup(tok->dptr, pc);
          tok->dptr = pc + 1;
        }
      }
      else
      {
        for (pc = tok->dptr; isalnum((unsigned char) *pc) || *pc == '_'; pc++)
          ;
        var = mutt_str_substr_dup(tok->dptr, pc);
        tok->dptr = pc;
      }
      if (var)
      {
        int idx;
        if ((env = mutt_str_getenv(var)) || (env = myvar_get(var)))
          mutt_buffer_addstr(dest, env);
        else if ((idx = mutt_option_index(var)) != -1)
        {
          /* expand settable neomutt variables */
          char val[LONG_STRING];

          if (var_to_string(idx, val, sizeof(val)))
            mutt_buffer_addstr(dest, val);
        }
        FREE(&var);
      }
    }
    else
      mutt_buffer_addch(dest, ch);
  }
  mutt_buffer_addch(dest, 0); /* terminate the string */
  SKIPWS(tok->dptr);
  return 0;
}

static void free_opt(struct Option *p)
{
  switch (DTYPE(p->type))
  {
    case DT_ADDRESS:
      mutt_addr_free((struct Address **) p->var);
      break;
    case DT_REGEX:
      mutt_regex_free((struct Regex **) p->var);
      break;
    case DT_PATH:
    case DT_STRING:
      FREE((char **) p->var);
      break;
  }
}

/**
 * mutt_free_opts - clean up before quitting
 */
void mutt_free_opts(void)
{
  for (int i = 0; MuttVars[i].name; i++)
    free_opt(MuttVars + i);

  mutt_regexlist_free(&Alternates);
  mutt_regexlist_free(&UnAlternates);
  mutt_regexlist_free(&MailLists);
  mutt_regexlist_free(&UnMailLists);
  mutt_regexlist_free(&SubscribedLists);
  mutt_regexlist_free(&UnSubscribedLists);
  mutt_regexlist_free(&NoSpamList);
}

static void add_to_stailq(struct ListHead *head, const char *str)
{
  /* don't add a NULL or empty string to the list */
  if (!str || *str == '\0')
    return;

  /* check to make sure the item is not already on this list */
  struct ListNode *np;
  STAILQ_FOREACH(np, head, entries)
  {
    if (mutt_str_strcasecmp(str, np->data) == 0)
    {
      return;
    }
  }
  mutt_list_insert_tail(head, mutt_str_strdup(str));
}

/**
 * finish_source - 'finish' command: stop processing current config file
 * @param tmp  Temporary space shared by all command handlers
 * @param s    Current line of the config file
 * @param data data field from init.h:struct Command
 * @param err  Buffer for any error message
 * @retval  1 Stop processing the current file
 * @retval -1 Failed
 *
 * If the 'finish' command is found, we should stop reading the current file.
 */
static int finish_source(struct Buffer *buf, struct Buffer *s,
                         unsigned long data, struct Buffer *err)
{
  if (MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too many arguments"), "finish");
    return -1;
  }

  return 1;
}

/**
 * parse_ifdef - 'ifdef' command: conditional config
 * @param tmp  Temporary space shared by all command handlers
 * @param s    Current line of the config file
 * @param data data field from init.h:struct Command
 * @param err  Buffer for any error message
 * @retval  0 Success
 * @retval -1 Failed
 *
 * The 'ifdef' command allows conditional elements in the config file.
 * If a given variable, function, command or compile-time symbol exists, then
 * read the rest of the line of config commands.
 * e.g.
 *      ifdef sidebar source ~/.neomutt/sidebar.rc
 *
 * If (data == 1) then it means use the 'ifndef' (if-not-defined) command.
 * e.g.
 *      ifndef imap finish
 */
static int parse_ifdef(struct Buffer *buf, struct Buffer *s, unsigned long data,
                       struct Buffer *err)
{
  bool res = 0;
  struct Buffer token;

  memset(&token, 0, sizeof(token));
  mutt_extract_token(buf, s, 0);

  /* is the item defined as a variable? */
  res = (mutt_option_index(buf->data) != -1);

  /* is the item a compiled-in feature? */
  if (!res)
  {
    res = feature_enabled(buf->data);
  }

  /* or a function? */
  if (!res)
  {
    for (int i = 0; !res && (i < MENU_MAX); i++)
    {
      const struct Binding *b = km_get_table(Menus[i].value);
      if (!b)
        continue;

      for (int j = 0; b[j].name; j++)
      {
        if (mutt_str_strcmp(buf->data, b[j].name) == 0)
        {
          res = true;
          break;
        }
      }
    }
  }

  /* or a command? */
  if (!res)
  {
    for (int i = 0; Commands[i].name; i++)
    {
      if (mutt_str_strcmp(buf->data, Commands[i].name) == 0)
      {
        res = true;
        break;
      }
    }
  }

  /* or a my_ var? */
  if (!res)
  {
    res = !!myvar_get(buf->data);
  }

  if (!MoreArgs(s))
  {
    snprintf(err->data, err->dsize, _("%s: too few arguments"), (data ? "ifndef" : "ifdef"));
    return -1;
  }
  mutt_extract_token(buf, s, MUTT_TOKEN_SPACE);

  /* ifdef KNOWN_SYMBOL or ifndef UNKNOWN_SYMBOL */
  if ((res && (data == 0)) || (!res && (data == 1)))
  {
    int rc = mutt_parse_rc_line(buf->data, &token, err);
    if (rc == -1)
    {
      mutt_error("Error: %s", err->data);
      FREE(&token.data);
      return -1;
    }
    FREE(&token.data);
    return rc;
  }
  return 0;
}

static void remove_from_stailq(struct ListHead *head, const char *str)
{
  if (mutt_str_strcmp("*", str) == 0)
    mutt_list_free(head); /* ``unCMD *'' means delete all current entries */
  else
  {
    struct ListNode *np, *tmp;
    STAILQ_FOREACH_SAFE(np, head, entries, tmp)
    {
      if (mutt_str_strcasecmp(str, np->data) == 0)
      {
        STAILQ_REMOVE(head, np, ListNode, entries);
        FREE(&np->data);
        FREE(&np);
        break;
      }
    }
  }
}

static int parse_unignore(struct Buffer *buf, struct Buffer *s,
                          unsigned long data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, 0);

    /* don't add "*" to the unignore list */
    if (strcmp(buf->data, "*") != 0)
      add_to_stailq(&UnIgnore, buf->data);

    remove_from_stailq(&Ignore, buf->data);
  } while (MoreArgs(s));

  return 0;
}

static int parse_ignore(struct Buffer *buf, struct Buffer *s,
                        unsigned long data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, 0);
    remove_from_stailq(&UnIgnore, buf->data);
    add_to_stailq(&Ignore, buf->data);
  } while (MoreArgs(s));

  return 0;
}

static int parse_stailq(struct Buffer *buf, struct Buffer *s,
                        unsigned long data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, 0);
    add_to_stailq((struct ListHead *) data, buf->data);
  } while (MoreArgs(s));

  return 0;
}

static int parse_unstailq(struct Buffer *buf, struct Buffer *s,
                          unsigned long data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, 0);
    /*
     * Check for deletion of entire list
     */
    if (mutt_str_strcmp(buf->data, "*") == 0)
    {
      mutt_list_free((struct ListHead *) data);
      break;
    }
    remove_from_stailq((struct ListHead *) data, buf->data);
  } while (MoreArgs(s));

  return 0;
}

static void alternates_clean(void)
{
  if (!Context)
    return;

  for (int i = 0; i < Context->msgcount; i++)
    Context->hdrs[i]->recip_valid = false;
}

static int parse_alternates(struct Buffer *buf, struct Buffer *s,
                            unsigned long data, struct Buffer *err)
{
  struct GroupContext *gc = NULL;

  alternates_clean();

  do
  {
    mutt_extract_token(buf, s, 0);

    if (parse_group_context(&gc, buf, s, data, err) == -1)
      goto bail;

    mutt_regexlist_remove(&UnAlternates, buf->data);

    if (mutt_regexlist_add(&Alternates, buf->data, REG_ICASE, err) != 0)
      goto bail;

    if (mutt_group_context_add_regex(gc, buf->data, REG_ICASE, err) != 0)
      goto bail;
  } while (MoreArgs(s));

  mutt_group_context_destroy(&gc);
  return 0;

bail:
  mutt_group_context_destroy(&gc);
  return -1;
}

static int parse_unalternates(struct Buffer *buf, struct Buffer *s,
                              unsigned long data, struct Buffer *err)
{
  alternates_clean();
  do
  {
    mutt_extract_token(buf, s, 0);
    mutt_regexlist_remove(&Alternates, buf->data);

    if ((mutt_str_strcmp(buf->data, "*") != 0) &&
        mutt_regexlist_add(&UnAlternates, buf->data, REG_ICASE, err) != 0)
    {
      return -1;
    }

  } while (MoreArgs(s));

  return 0;
}

static int parse_replace_list(struct Buffer *buf, struct Buffer *s,
                              unsigned long data, struct Buffer *err)
{
  struct ReplaceList **list = (struct ReplaceList **) data;
  struct Buffer templ;

  memset(&templ, 0, sizeof(templ));

  /* First token is a regex. */
  if (!MoreArgs(s))
  {
    mutt_str_strfcpy(err->data, _("not enough arguments"), err->dsize);
    return -1;
  }
  mutt_extract_token(buf, s, 0);

  /* Second token is a replacement template */
  if (!MoreArgs(s))
  {
    mutt_str_strfcpy(err->data, _("not enough arguments"), err->dsize);
    return -1;
  }
  mutt_extract_token(&templ, s, 0);

  if (mutt_replacelist_add(list, buf->data, templ.data, err) != 0)
  {
    FREE(&templ.data);
    return -1;
  }
  FREE(&templ.data);

  return 0;
}

static int parse_unreplace_list(struct Buffer *buf, struct Buffer *s,
                                unsigned long data, struct Buffer *err)
{
  struct ReplaceList **list = (struct ReplaceList **) data;

  /* First token is a regex. */
  if (!MoreArgs(s))
  {
    mutt_str_strfcpy(err->data, _("not enough arguments"), err->dsize);
    return -1;
  }

  mutt_extract_token(buf, s, 0);

  /* "*" is a special case. */
  if (mutt_str_strcmp(buf->data, "*") == 0)
  {
    mutt_replacelist_free(list);
    return 0;
  }

  mutt_replacelist_remove(list, buf->data);
  return 0;
}

static void clear_subject_mods(void)
{
  if (!Context)
    return;

  for (int i = 0; i < Context->msgcount; i++)
    FREE(&Context->hdrs[i]->env->disp_subj);
}

static int parse_subjectrx_list(struct Buffer *buf, struct Buffer *s,
                                unsigned long data, struct Buffer *err)
{
  int rc;

  rc = parse_replace_list(buf, s, data, err);
  if (rc == 0)
    clear_subject_mods();
  return rc;
}

static int parse_unsubjectrx_list(struct Buffer *buf, struct Buffer *s,
                                  unsigned long data, struct Buffer *err)
{
  int rc;

  rc = parse_unreplace_list(buf, s, data, err);
  if (rc == 0)
    clear_subject_mods();
  return rc;
}

static int parse_spam_list(struct Buffer *buf, struct Buffer *s,
                           unsigned long data, struct Buffer *err)
{
  struct Buffer templ;

  mutt_buffer_init(&templ);

  /* Insist on at least one parameter */
  if (!MoreArgs(s))
  {
    if (data == MUTT_SPAM)
      mutt_str_strfcpy(err->data, _("spam: no matching pattern"), err->dsize);
    else
      mutt_str_strfcpy(err->data, _("nospam: no matching pattern"), err->dsize);
    return -1;
  }

  /* Extract the first token, a regex */
  mutt_extract_token(buf, s, 0);

  /* data should be either MUTT_SPAM or MUTT_NOSPAM. MUTT_SPAM is for spam commands. */
  if (data == MUTT_SPAM)
  {
    /* If there's a second parameter, it's a template for the spam tag. */
    if (MoreArgs(s))
    {
      mutt_extract_token(&templ, s, 0);

      /* Add to the spam list. */
      if (mutt_replacelist_add(&SpamList, buf->data, templ.data, err) != 0)
      {
        FREE(&templ.data);
        return -1;
      }
      FREE(&templ.data);
    }

    /* If not, try to remove from the nospam list. */
    else
    {
      mutt_regexlist_remove(&NoSpamList, buf->data);
    }

    return 0;
  }

  /* MUTT_NOSPAM is for nospam commands. */
  else if (data == MUTT_NOSPAM)
  {
    /* nospam only ever has one parameter. */

    /* "*" is a special case. */
    if (mutt_str_strcmp(buf->data, "*") == 0)
    {
      mutt_replacelist_free(&SpamList);
      mutt_regexlist_free(&NoSpamList);
      return 0;
    }

    /* If it's on the spam list, just remove it. */
    if (mutt_replacelist_remove(&SpamList, buf->data) != 0)
      return 0;

    /* Otherwise, add it to the nospam list. */
    if (mutt_regexlist_add(&NoSpamList, buf->data, REG_ICASE, err) != 0)
      return -1;

    return 0;
  }

  /* This should not happen. */
  mutt_str_strfcpy(err->data, "This is no good at all.", err->dsize);
  return -1;
}

#ifdef USE_SIDEBAR
static int parse_path_list(struct Buffer *buf, struct Buffer *s,
                           unsigned long data, struct Buffer *err)
{
  char path[_POSIX_PATH_MAX];

  do
  {
    mutt_extract_token(buf, s, 0);
    mutt_str_strfcpy(path, buf->data, sizeof(path));
    mutt_expand_path(path, sizeof(path));
    add_to_stailq((struct ListHead *) data, path);
  } while (MoreArgs(s));

  return 0;
}

static int parse_path_unlist(struct Buffer *buf, struct Buffer *s,
                             unsigned long data, struct Buffer *err)
{
  char path[_POSIX_PATH_MAX];

  do
  {
    mutt_extract_token(buf, s, 0);
    /*
     * Check for deletion of entire list
     */
    if (mutt_str_strcmp(buf->data, "*") == 0)
    {
      mutt_list_free((struct ListHead *) data);
      break;
    }
    mutt_str_strfcpy(path, buf->data, sizeof(path));
    mutt_expand_path(path, sizeof(path));
    remove_from_stailq((struct ListHead *) data, path);
  } while (MoreArgs(s));

  return 0;
}
#endif

static int parse_lists(struct Buffer *buf, struct Buffer *s, unsigned long data,
                       struct Buffer *err)
{
  struct GroupContext *gc = NULL;

  do
  {
    mutt_extract_token(buf, s, 0);

    if (parse_group_context(&gc, buf, s, data, err) == -1)
      goto bail;

    mutt_regexlist_remove(&UnMailLists, buf->data);

    if (mutt_regexlist_add(&MailLists, buf->data, REG_ICASE, err) != 0)
      goto bail;

    if (mutt_group_context_add_regex(gc, buf->data, REG_ICASE, err) != 0)
      goto bail;
  } while (MoreArgs(s));

  mutt_group_context_destroy(&gc);
  return 0;

bail:
  mutt_group_context_destroy(&gc);
  return -1;
}

/**
 * enum GroupState - Type of email address group
 */
enum GroupState
{
  GS_NONE,
  GS_RX,
  GS_ADDR
};

static int parse_group(struct Buffer *buf, struct Buffer *s, unsigned long data,
                       struct Buffer *err)
{
  struct GroupContext *gc = NULL;
  enum GroupState state = GS_NONE;
  struct Address *addr = NULL;
  char *estr = NULL;

  do
  {
    mutt_extract_token(buf, s, 0);
    if (parse_group_context(&gc, buf, s, data, err) == -1)
      goto bail;

    if (data == MUTT_UNGROUP && (mutt_str_strcasecmp(buf->data, "*") == 0))
    {
      if (mutt_group_context_clear(&gc) < 0)
        goto bail;
      goto out;
    }

    if (mutt_str_strcasecmp(buf->data, "-rx") == 0)
      state = GS_RX;
    else if (mutt_str_strcasecmp(buf->data, "-addr") == 0)
      state = GS_ADDR;
    else
    {
      switch (state)
      {
        case GS_NONE:
          snprintf(err->data, err->dsize, _("%sgroup: missing -rx or -addr."),
                   data == MUTT_UNGROUP ? "un" : "");
          goto bail;

        case GS_RX:
          if (data == MUTT_GROUP &&
              mutt_group_context_add_regex(gc, buf->data, REG_ICASE, err) != 0)
          {
            goto bail;
          }
          else if (data == MUTT_UNGROUP &&
                   mutt_group_context_remove_regex(gc, buf->data) < 0)
          {
            goto bail;
          }
          break;

        case GS_ADDR:
          addr = mutt_addr_parse_list2(NULL, buf->data);
          if (!addr)
            goto bail;
          if (mutt_addrlist_to_intl(addr, &estr))
          {
            snprintf(err->data, err->dsize, _("%sgroup: warning: bad IDN '%s'.\n"),
                     data == 1 ? "un" : "", estr);
            mutt_addr_free(&addr);
            FREE(&estr);
            goto bail;
          }
          if (data == MUTT_GROUP)
            mutt_group_context_add_addrlist(gc, addr);
          else if (data == MUTT_UNGROUP)
            mutt_group_context_remove_addrlist(gc, addr);
          mutt_addr_free(&addr);
          break;
      }
    }
  } while (MoreArgs(s));

out:
  mutt_group_context_destroy(&gc);
  return 0;

bail:
  mutt_group_context_destroy(&gc);
  return -1;
}

/**
 * attachments_clean - always wise to do what someone else did before
 */
static void attachments_clean(void)
{
  if (!Context)
    return;

  for (int i = 0; i < Context->msgcount; i++)
    Context->hdrs[i]->attach_valid = false;
}

static int parse_attach_list(struct Buffer *buf, struct Buffer *s,
                             struct ListHead *head, struct Buffer *err)
{
  struct AttachMatch *a = NULL;
  char *p = NULL;
  char *tmpminor = NULL;
  size_t len;
  int ret;

  do
  {
    mutt_extract_token(buf, s, 0);

    if (!buf->data || *buf->data == '\0')
      continue;

    a = mutt_mem_malloc(sizeof(struct AttachMatch));

    /* some cheap hacks that I expect to remove */
    if (mutt_str_strcasecmp(buf->data, "any") == 0)
      a->major = mutt_str_strdup("*/.*");
    else if (mutt_str_strcasecmp(buf->data, "none") == 0)
      a->major = mutt_str_strdup("cheap_hack/this_should_never_match");
    else
      a->major = mutt_str_strdup(buf->data);

    p = strchr(a->major, '/');
    if (p)
    {
      *p = '\0';
      p++;
      a->minor = p;
    }
    else
    {
      a->minor = "unknown";
    }

    len = strlen(a->minor);
    tmpminor = mutt_mem_malloc(len + 3);
    strcpy(&tmpminor[1], a->minor);
    tmpminor[0] = '^';
    tmpminor[len + 1] = '$';
    tmpminor[len + 2] = '\0';

    a->major_int = mutt_check_mime_type(a->major);
    ret = REGCOMP(&a->minor_regex, tmpminor, REG_ICASE);

    FREE(&tmpminor);

    if (ret != 0)
    {
      regerror(ret, &a->minor_regex, err->data, err->dsize);
      FREE(&a->major);
      FREE(&a);
      return -1;
    }

    mutt_debug(5, "added %s/%s [%d]\n", a->major, a->minor, a->major_int);

    mutt_list_insert_tail(head, (char *) a);
  } while (MoreArgs(s));

  attachments_clean();
  return 0;
}

static int parse_unattach_list(struct Buffer *buf, struct Buffer *s,
                               struct ListHead *head, struct Buffer *err)
{
  struct AttachMatch *a = NULL;
  char *tmp = NULL;
  char *minor = NULL;

  do
  {
    mutt_extract_token(buf, s, 0);
    FREE(&tmp);

    if (mutt_str_strcasecmp(buf->data, "any") == 0)
      tmp = mutt_str_strdup("*/.*");
    else if (mutt_str_strcasecmp(buf->data, "none") == 0)
      tmp = mutt_str_strdup("cheap_hack/this_should_never_match");
    else
      tmp = mutt_str_strdup(buf->data);

    minor = strchr(tmp, '/');
    if (minor)
    {
      *minor = '\0';
      minor++;
    }
    else
    {
      minor = "unknown";
    }
    const int major = mutt_check_mime_type(tmp);

    struct ListNode *np, *tmp2;
    STAILQ_FOREACH_SAFE(np, head, entries, tmp2)
    {
      a = (struct AttachMatch *) np->data;
      mutt_debug(5, "check %s/%s [%d] : %s/%s [%d]\n", a->major, a->minor,
                 a->major_int, tmp, minor, major);
      if (a->major_int == major && (mutt_str_strcasecmp(minor, a->minor) == 0))
      {
        mutt_debug(5, "removed %s/%s [%d]\n", a->major, a->minor, a->major_int);
        regfree(&a->minor_regex);
        FREE(&a->major);
        STAILQ_REMOVE(head, np, ListNode, entries);
        FREE(&np->data);
        FREE(&np);
      }
    }

  } while (MoreArgs(s));

  FREE(&tmp);
  attachments_clean();
  return 0;
}

static int print_attach_list(struct ListHead *h, char op, char *name)
{
  struct ListNode *np;
  STAILQ_FOREACH(np, h, entries)
  {
    printf("attachments %c%s %s/%s\n", op, name,
           ((struct AttachMatch *) np->data)->major,
           ((struct AttachMatch *) np->data)->minor);
  }

  return 0;
}

static int parse_attachments(struct Buffer *buf, struct Buffer *s,
                             unsigned long data, struct Buffer *err)
{
  char op, *category = NULL;
  struct ListHead *head = NULL;

  mutt_extract_token(buf, s, 0);
  if (!buf->data || *buf->data == '\0')
  {
    mutt_str_strfcpy(err->data, _("attachments: no disposition"), err->dsize);
    return -1;
  }

  category = buf->data;
  op = *category++;

  if (op == '?')
  {
    mutt_endwin();
    fflush(stdout);
    printf("\n%s\n\n", _("Current attachments settings:"));
    print_attach_list(&AttachAllow, '+', "A");
    print_attach_list(&AttachExclude, '-', "A");
    print_attach_list(&InlineAllow, '+', "I");
    print_attach_list(&InlineExclude, '-', "I");
    mutt_any_key_to_continue(NULL);
    return 0;
  }

  if (op != '+' && op != '-')
  {
    op = '+';
    category--;
  }
  if (mutt_str_strncasecmp(category, "attachment", strlen(category)) == 0)
  {
    if (op == '+')
      head = &AttachAllow;
    else
      head = &AttachExclude;
  }
  else if (mutt_str_strncasecmp(category, "inline", strlen(category)) == 0)
  {
    if (op == '+')
      head = &InlineAllow;
    else
      head = &InlineExclude;
  }
  else
  {
    mutt_str_strfcpy(err->data, _("attachments: invalid disposition"), err->dsize);
    return -1;
  }

  return parse_attach_list(buf, s, head, err);
}

static int parse_unattachments(struct Buffer *buf, struct Buffer *s,
                               unsigned long data, struct Buffer *err)
{
  char op, *p = NULL;
  struct ListHead *head = NULL;

  mutt_extract_token(buf, s, 0);
  if (!buf->data || *buf->data == '\0')
  {
    mutt_str_strfcpy(err->data, _("unattachments: no disposition"), err->dsize);
    return -1;
  }

  p = buf->data;
  op = *p++;
  if (op != '+' && op != '-')
  {
    op = '+';
    p--;
  }
  if (mutt_str_strncasecmp(p, "attachment", strlen(p)) == 0)
  {
    if (op == '+')
      head = &AttachAllow;
    else
      head = &AttachExclude;
  }
  else if (mutt_str_strncasecmp(p, "inline", strlen(p)) == 0)
  {
    if (op == '+')
      head = &InlineAllow;
    else
      head = &InlineExclude;
  }
  else
  {
    mutt_str_strfcpy(err->data, _("unattachments: invalid disposition"), err->dsize);
    return -1;
  }

  return parse_unattach_list(buf, s, head, err);
}

static int parse_unlists(struct Buffer *buf, struct Buffer *s,
                         unsigned long data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, 0);
    mutt_regexlist_remove(&SubscribedLists, buf->data);
    mutt_regexlist_remove(&MailLists, buf->data);

    if ((mutt_str_strcmp(buf->data, "*") != 0) &&
        mutt_regexlist_add(&UnMailLists, buf->data, REG_ICASE, err) != 0)
    {
      return -1;
    }
  } while (MoreArgs(s));

  return 0;
}

static int parse_subscribe(struct Buffer *buf, struct Buffer *s,
                           unsigned long data, struct Buffer *err)
{
  struct GroupContext *gc = NULL;

  do
  {
    mutt_extract_token(buf, s, 0);

    if (parse_group_context(&gc, buf, s, data, err) == -1)
      goto bail;

    mutt_regexlist_remove(&UnMailLists, buf->data);
    mutt_regexlist_remove(&UnSubscribedLists, buf->data);

    if (mutt_regexlist_add(&MailLists, buf->data, REG_ICASE, err) != 0)
      goto bail;
    if (mutt_regexlist_add(&SubscribedLists, buf->data, REG_ICASE, err) != 0)
      goto bail;
    if (mutt_group_context_add_regex(gc, buf->data, REG_ICASE, err) != 0)
      goto bail;
  } while (MoreArgs(s));

  mutt_group_context_destroy(&gc);
  return 0;

bail:
  mutt_group_context_destroy(&gc);
  return -1;
}

static int parse_unsubscribe(struct Buffer *buf, struct Buffer *s,
                             unsigned long data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, 0);
    mutt_regexlist_remove(&SubscribedLists, buf->data);

    if ((mutt_str_strcmp(buf->data, "*") != 0) &&
        mutt_regexlist_add(&UnSubscribedLists, buf->data, REG_ICASE, err) != 0)
    {
      return -1;
    }
  } while (MoreArgs(s));

  return 0;
}

static int parse_unalias(struct Buffer *buf, struct Buffer *s,
                         unsigned long data, struct Buffer *err)
{
  struct Alias *tmp = NULL, *last = NULL;

  do
  {
    mutt_extract_token(buf, s, 0);

    if (mutt_str_strcmp("*", buf->data) == 0)
    {
      if (CurrentMenu == MENU_ALIAS)
      {
        for (tmp = Aliases; tmp; tmp = tmp->next)
          tmp->del = true;
        mutt_menu_set_current_redraw_full();
      }
      else
        mutt_alias_free(&Aliases);
      break;
    }
    else
    {
      for (tmp = Aliases; tmp; tmp = tmp->next)
      {
        if (mutt_str_strcasecmp(buf->data, tmp->name) == 0)
        {
          if (CurrentMenu == MENU_ALIAS)
          {
            tmp->del = true;
            mutt_menu_set_current_redraw_full();
            break;
          }

          if (last)
            last->next = tmp->next;
          else
            Aliases = tmp->next;
          tmp->next = NULL;
          mutt_alias_free(&tmp);
          break;
        }
        last = tmp;
      }
    }
  } while (MoreArgs(s));
  return 0;
}

static int parse_alias(struct Buffer *buf, struct Buffer *s, unsigned long data,
                       struct Buffer *err)
{
  struct Alias *tmp = Aliases;
  struct Alias *last = NULL;
  char *estr = NULL;
  struct GroupContext *gc = NULL;

  if (!MoreArgs(s))
  {
    mutt_str_strfcpy(err->data, _("alias: no address"), err->dsize);
    return -1;
  }

  mutt_extract_token(buf, s, 0);

  if (parse_group_context(&gc, buf, s, data, err) == -1)
    return -1;

  /* check to see if an alias with this name already exists */
  for (; tmp; tmp = tmp->next)
  {
    if (mutt_str_strcasecmp(tmp->name, buf->data) == 0)
      break;
    last = tmp;
  }

  if (!tmp)
  {
    /* create a new alias */
    tmp = mutt_mem_calloc(1, sizeof(struct Alias));
    tmp->name = mutt_str_strdup(buf->data);
    /* give the main addressbook code a chance */
    if (CurrentMenu == MENU_ALIAS)
      OPT_MENU_CALLER = true;
  }
  else
  {
    mutt_alias_delete_reverse(tmp);
    /* override the previous value */
    mutt_addr_free(&tmp->addr);
    if (CurrentMenu == MENU_ALIAS)
      mutt_menu_set_current_redraw_full();
  }

  mutt_extract_token(buf, s, MUTT_TOKEN_QUOTE | MUTT_TOKEN_SPACE | MUTT_TOKEN_SEMICOLON);
  mutt_debug(3, "Second token is '%s'.\n", buf->data);

  tmp->addr = mutt_addr_parse_list2(tmp->addr, buf->data);

  if (last)
    last->next = tmp;
  else
    Aliases = tmp;
  if (mutt_addrlist_to_intl(tmp->addr, &estr))
  {
    snprintf(err->data, err->dsize, _("Warning: Bad IDN '%s' in alias '%s'.\n"),
             estr, tmp->name);
    FREE(&estr);
    goto bail;
  }

  mutt_group_context_add_addrlist(gc, tmp->addr);
  mutt_alias_add_reverse(tmp);

  if (DebugLevel > 2)
  {
    /* A group is terminated with an empty address, so check a->mailbox */
    for (struct Address *a = tmp->addr; a && a->mailbox; a = a->next)
    {
      if (!a->group)
        mutt_debug(3, "  %s\n", a->mailbox);
      else
        mutt_debug(3, "  Group %s\n", a->mailbox);
    }
  }
  mutt_group_context_destroy(&gc);
  return 0;

bail:
  mutt_group_context_destroy(&gc);
  return -1;
}

static int parse_unmy_hdr(struct Buffer *buf, struct Buffer *s,
                          unsigned long data, struct Buffer *err)
{
  struct ListNode *np, *tmp;
  size_t l;

  do
  {
    mutt_extract_token(buf, s, 0);
    if (mutt_str_strcmp("*", buf->data) == 0)
    {
      mutt_list_free(&UserHeader);
      continue;
    }

    l = mutt_str_strlen(buf->data);
    if (buf->data[l - 1] == ':')
      l--;

    STAILQ_FOREACH_SAFE(np, &UserHeader, entries, tmp)
    {
      if ((mutt_str_strncasecmp(buf->data, np->data, l) == 0) && np->data[l] == ':')
      {
        STAILQ_REMOVE(&UserHeader, np, ListNode, entries);
        FREE(&np->data);
        FREE(&np);
      }
    }
  } while (MoreArgs(s));
  return 0;
}

static int parse_my_hdr(struct Buffer *buf, struct Buffer *s,
                        unsigned long data, struct Buffer *err)
{
  struct ListNode *n = NULL;
  size_t keylen;

  mutt_extract_token(buf, s, MUTT_TOKEN_SPACE | MUTT_TOKEN_QUOTE);
  char *p = strpbrk(buf->data, ": \t");
  if (!p || (*p != ':'))
  {
    mutt_str_strfcpy(err->data, _("invalid header field"), err->dsize);
    return -1;
  }
  keylen = p - buf->data + 1;

  STAILQ_FOREACH(n, &UserHeader, entries)
  {
    /* see if there is already a field by this name */
    if (mutt_str_strncasecmp(buf->data, n->data, keylen) == 0)
    {
      break;
    }
  }

  if (!n)
  {
    /* not found, allocate memory for a new node and add it to the list */
    n = mutt_list_insert_tail(&UserHeader, NULL);
  }
  else
  {
    /* found, free the existing data */
    FREE(&n->data);
  }

  n->data = buf->data;
  mutt_buffer_init(buf);

  return 0;
}

static void set_default(struct Option *p)
{
  switch (DTYPE(p->type))
  {
    case DT_STRING:
      if (!p->initial && *((char **) p->var))
        p->initial = (unsigned long) mutt_str_strdup(*((char **) p->var));
      break;
    case DT_PATH:
      if (!p->initial && *((char **) p->var))
      {
        char *cp = mutt_str_strdup(*((char **) p->var));
        /* mutt_pretty_mailbox (cp); */
        p->initial = (unsigned long) cp;
      }
      break;
    case DT_ADDRESS:
      if (!p->initial && *((struct Address **) p->var))
      {
        char tmp[HUGE_STRING];
        *tmp = '\0';
        mutt_addr_write(tmp, sizeof(tmp), *((struct Address **) p->var), false);
        p->initial = (unsigned long) mutt_str_strdup(tmp);
      }
      break;
    case DT_REGEX:
    {
      struct Regex **ptr = (struct Regex **) p->var;
      if (!p->initial && *ptr && (*ptr)->pattern)
        p->initial = (unsigned long) mutt_str_strdup((*ptr)->pattern);
      break;
    }
  }
}

static void restore_default(struct Option *p)
{
  switch (DTYPE(p->type))
  {
    case DT_STRING:
      mutt_str_replace((char **) p->var, (char *) p->initial);
      break;
    case DT_MBTABLE:
      free_mbtable((struct MbTable **) p->var);
      *((struct MbTable **) p->var) = parse_mbtable((char *) p->initial);
      break;
    case DT_PATH:
    {
      char *init = (char *) p->initial;
      if (mutt_str_strcmp(p->name, "debug_file") == 0)
      {
        mutt_log_set_file(init, true);
      }
      else
      {
        FREE((char **) p->var);
        if (init)
        {
          char path[_POSIX_PATH_MAX];
          mutt_str_strfcpy(path, init, sizeof(path));
          mutt_expand_path(path, sizeof(path));
          *((char **) p->var) = mutt_str_strdup(path);
        }
      }
      break;
    }
    case DT_ADDRESS:
      mutt_addr_free((struct Address **) p->var);
      if (p->initial)
        *((struct Address **) p->var) = mutt_addr_parse_list(NULL, (char *) p->initial);
      break;
    case DT_BOOL:
      if (p->initial)
        *(bool *) p->var = true;
      else
        *(bool *) p->var = false;
      break;
    case DT_QUAD:
      *(unsigned char *) p->var = p->initial;
      break;
    case DT_NUMBER:
    case DT_SORT:
    case DT_MAGIC:
      if (mutt_str_strcmp(p->name, "debug_level") == 0)
        mutt_log_set_level(p->initial, true);
      else
        *((short *) p->var) = p->initial;
      break;
    case DT_REGEX:
    {
      struct Regex **ptr = (struct Regex **) p->var;

      if (*ptr)
        mutt_regex_free(ptr);

      *ptr = mutt_regex_create((const char *) p->initial, p->type, NULL);
      break;
    }
  }

  if (p->flags & R_INDEX)
    mutt_menu_set_redraw_full(MENU_MAIN);
  if (p->flags & R_PAGER)
    mutt_menu_set_redraw_full(MENU_PAGER);
  if (p->flags & R_PAGER_FLOW)
  {
    mutt_menu_set_redraw_full(MENU_PAGER);
    mutt_menu_set_redraw(MENU_PAGER, REDRAW_FLOW);
  }
  if (p->flags & R_RESORT_SUB)
    OPT_SORT_SUBTHREADS = true;
  if (p->flags & R_RESORT)
    OPT_NEED_RESORT = true;
  if (p->flags & R_RESORT_INIT)
    OPT_RESORT_INIT = true;
  if (p->flags & R_TREE)
    OPT_REDRAW_TREE = true;
  if (p->flags & R_REFLOW)
    mutt_window_reflow();
#ifdef USE_SIDEBAR
  if (p->flags & R_SIDEBAR)
    mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
#endif
  if (p->flags & R_MENU)
    mutt_menu_set_current_redraw_full();
}

static void esc_char(char c, char *p, char *dst, size_t len)
{
  *p++ = '\\';
  if (p - dst < len)
    *p++ = c;
}

static size_t escape_string(char *dst, size_t len, const char *src)
{
  char *p = dst;

  if (!len)
    return 0;
  len--; /* save room for \0 */
  while (p - dst < len && src && *src)
  {
    switch (*src)
    {
      case '\n':
        esc_char('n', p, dst, len);
        p += 2;
        break;
      case '\r':
        esc_char('r', p, dst, len);
        p += 2;
        break;
      case '\t':
        esc_char('t', p, dst, len);
        p += 2;
        break;
      default:
        if ((*src == '\\' || *src == '"') && p - dst < len - 1)
          *p++ = '\\';
        *p++ = *src;
    }
    src++;
  }
  *p = '\0';
  return p - dst;
}

static void pretty_var(char *dst, size_t len, const char *option, const char *val)
{
  char *p = NULL;

  if (!len)
    return;

  mutt_str_strfcpy(dst, option, len);
  len--; /* save room for \0 */
  p = dst + mutt_str_strlen(dst);

  if (p - dst < len)
    *p++ = '=';
  if (p - dst < len)
    *p++ = '"';
  p += escape_string(p, len - (p - dst) + 1, val); /* \0 terminate it */
  if (p - dst < len)
    *p++ = '"';
  *p = '\0';
}

static int check_charset(struct Option *opt, const char *val)
{
  char *q = NULL, *s = mutt_str_strdup(val);
  int rc = 0;
  bool strict = (strcmp(opt->name, "send_charset") == 0);

  if (!s)
    return rc;

  for (char *p = strtok_r(s, ":", &q); p; p = strtok_r(NULL, ":", &q))
  {
    if (!*p)
      continue;
    if (!mutt_ch_check_charset(p, strict))
    {
      rc = -1;
      break;
    }
  }

  FREE(&s);
  return rc;
}

static bool valid_show_multipart_alternative(const char *val)
{
  return ((mutt_str_strcmp(val, "inline") == 0) ||
          (mutt_str_strcmp(val, "info") == 0) || !val || (*val == 0));
}

static int parse_setenv(struct Buffer *buf, struct Buffer *s,
                        unsigned long data, struct Buffer *err)
{
  char **envp = mutt_envlist_getlist();

  bool query = false;
  bool unset = data & MUTT_SET_UNSET;

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "setenv");
    return -1;
  }

  if (*s->dptr == '?')
  {
    query = true;
    s->dptr++;
  }

  /* get variable name */
  mutt_extract_token(buf, s, MUTT_TOKEN_EQUAL);
  int len = strlen(buf->data);

  if (query)
  {
    bool found = false;
    while (envp && *envp)
    {
      /* This will display all matches for "^QUERY" */
      if (mutt_str_strncmp(buf->data, *envp, len) == 0)
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
      return 0;
    }

    snprintf(err->data, err->dsize, _("%s is unset"), buf->data);
    return -1;
  }

  if (unset)
  {
    if (mutt_envlist_unset(buf->data))
      return 0;
    return -1;
  }

  /* set variable */

  if (*s->dptr == '=')
  {
    s->dptr++;
    SKIPWS(s->dptr);
  }

  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "setenv");
    return -1;
  }

  char *name = mutt_str_strdup(buf->data);
  mutt_extract_token(buf, s, 0);
  mutt_envlist_set(name, buf->data, true);
  FREE(&name);

  return 0;
}

static int parse_set(struct Buffer *buf, struct Buffer *s, unsigned long data,
                     struct Buffer *err)
{
  int r = 0;

  while (MoreArgs(s))
  {
    int query = 0;
    int unset = data & MUTT_SET_UNSET;
    int inv = data & MUTT_SET_INV;
    int reset = data & MUTT_SET_RESET;
    int idx = -1;
    const char *p = NULL;
    const char *myvar = NULL;

    if (*s->dptr == '?')
    {
      query = 1;
      s->dptr++;
    }
    else if (mutt_str_strncmp("no", s->dptr, 2) == 0)
    {
      s->dptr += 2;
      unset = !unset;
    }
    else if (mutt_str_strncmp("inv", s->dptr, 3) == 0)
    {
      s->dptr += 3;
      inv = !inv;
    }
    else if (*s->dptr == '&')
    {
      reset = 1;
      s->dptr++;
    }

    /* get the variable name */
    mutt_extract_token(buf, s, MUTT_TOKEN_EQUAL);

    if (mutt_str_strncmp("my_", buf->data, 3) == 0)
      myvar = buf->data;
    else if ((idx = mutt_option_index(buf->data)) == -1 &&
             !(reset && (mutt_str_strcmp("all", buf->data) == 0)))
    {
      snprintf(err->data, err->dsize, _("%s: unknown variable"), buf->data);
      return -1;
    }
    SKIPWS(s->dptr);

    if (reset)
    {
      if (query || unset || inv)
      {
        snprintf(err->data, err->dsize, "%s", _("prefix is illegal with reset"));
        return -1;
      }

      if (*s->dptr == '=')
      {
        snprintf(err->data, err->dsize, "%s", _("value is illegal with reset"));
        return -1;
      }

      if (mutt_str_strcmp("all", buf->data) == 0)
      {
        if (CurrentMenu == MENU_PAGER)
        {
          snprintf(err->data, err->dsize, "%s", _("Not available in this menu."));
          return -1;
        }
        for (idx = 0; MuttVars[idx].name; idx++)
          restore_default(&MuttVars[idx]);
        mutt_menu_set_current_redraw_full();
        OPT_SORT_SUBTHREADS = true;
        OPT_NEED_RESORT = true;
        OPT_RESORT_INIT = true;
        OPT_REDRAW_TREE = true;
        return 0;
      }
      else
      {
        CHECK_PAGER;
        if (myvar)
          myvar_del(myvar);
        else
          restore_default(&MuttVars[idx]);
      }
    }
    else if (!myvar && (idx >= 0) && (DTYPE(MuttVars[idx].type) == DT_BOOL))
    {
      if (*s->dptr == '=')
      {
        if (unset || inv || query)
        {
          snprintf(err->data, err->dsize, "%s", _("Usage: set variable=yes|no"));
          return -1;
        }

        s->dptr++;
        mutt_extract_token(buf, s, 0);
        if (mutt_str_strcasecmp("yes", buf->data) == 0)
          unset = inv = 0;
        else if (mutt_str_strcasecmp("no", buf->data) == 0)
          unset = 1;
        else
        {
          snprintf(err->data, err->dsize, "%s", _("Usage: set variable=yes|no"));
          return -1;
        }
      }

      if (query)
      {
        snprintf(err->data, err->dsize,
                 *(bool *) MuttVars[idx].var ? _("%s is set") : _("%s is unset"),
                 buf->data);
        return 0;
      }

      CHECK_PAGER;
      if (unset)
        *(bool *) MuttVars[idx].var = false;
      else if (inv)
        *(bool *) MuttVars[idx].var = !(*(bool *) MuttVars[idx].var);
      else
        *(bool *) MuttVars[idx].var = true;
    }
    else if (myvar || ((idx >= 0) && ((DTYPE(MuttVars[idx].type) == DT_STRING) ||
                                      (DTYPE(MuttVars[idx].type) == DT_PATH) ||
                                      (DTYPE(MuttVars[idx].type) == DT_ADDRESS) ||
                                      (DTYPE(MuttVars[idx].type) == DT_MBTABLE))))
    {
      if (unset)
      {
        CHECK_PAGER;
        if (myvar)
          myvar_del(myvar);
        else if (DTYPE(MuttVars[idx].type) == DT_ADDRESS)
          mutt_addr_free((struct Address **) MuttVars[idx].var);
        else if (DTYPE(MuttVars[idx].type) == DT_MBTABLE)
          free_mbtable((struct MbTable **) MuttVars[idx].var);
        else
          /* MuttVars[idx].var is already 'char**' (or some 'void**') or...
           * so cast to 'void*' is okay */
          FREE((void *) MuttVars[idx].var);
      }
      else if (query || *s->dptr != '=')
      {
        char tmp2[LONG_STRING];
        const char *val = NULL;

        if (myvar)
        {
          val = myvar_get(myvar);
          if (val)
          {
            pretty_var(err->data, err->dsize, myvar, val);
            break;
          }
          else
          {
            snprintf(err->data, err->dsize, _("%s: unknown variable"), myvar);
            return -1;
          }
        }
        else if (DTYPE(MuttVars[idx].type) == DT_ADDRESS)
        {
          tmp2[0] = '\0';
          mutt_addr_write(tmp2, sizeof(tmp2),
                          *((struct Address **) MuttVars[idx].var), false);
          val = tmp2;
        }
        else if (DTYPE(MuttVars[idx].type) == DT_PATH)
        {
          tmp2[0] = '\0';
          mutt_str_strfcpy(tmp2, NONULL(*((char **) MuttVars[idx].var)), sizeof(tmp2));
          mutt_pretty_mailbox(tmp2, sizeof(tmp2));
          val = tmp2;
        }
        else if (DTYPE(MuttVars[idx].type) == DT_MBTABLE)
        {
          struct MbTable *mbt = (*((struct MbTable **) MuttVars[idx].var));
          val = mbt ? NONULL(mbt->orig_str) : "";
        }
        else
          val = *((char **) MuttVars[idx].var);

        /* user requested the value of this variable */
        pretty_var(err->data, err->dsize, MuttVars[idx].name, NONULL(val));
        break;
      }
      else
      {
        CHECK_PAGER;
        s->dptr++;

        if (myvar)
        {
          /* myvar is a pointer to buf and will be lost on extract_token */
          myvar = mutt_str_strdup(myvar);
          myvar_del(myvar);
        }

        mutt_extract_token(buf, s, 0);

        if (myvar)
        {
          myvar_set(myvar, buf->data);
          FREE(&myvar);
          myvar = "don't resort";
        }
        else if ((idx >= 0) && (DTYPE(MuttVars[idx].type) == DT_PATH))
        {
          char scratch[_POSIX_PATH_MAX];
          mutt_str_strfcpy(scratch, buf->data, sizeof(scratch));
          mutt_expand_path(scratch, sizeof(scratch));
          if (mutt_str_strcmp(MuttVars[idx].name, "debug_file") == 0)
          {
            mutt_log_set_file(scratch, true);
          }
          else
          {
            /* MuttVars[idx].var is already 'char**' (or some 'void**') or...
             * so cast to 'void*' is okay */
            FREE((void *) MuttVars[idx].var);
            *((char **) MuttVars[idx].var) = mutt_str_strdup(scratch);
          }
        }
        else if ((idx >= 0) && (DTYPE(MuttVars[idx].type) == DT_STRING))
        {
          if ((strstr(MuttVars[idx].name, "charset") &&
               check_charset(&MuttVars[idx], buf->data) < 0) |
              /* $charset can't be empty, others can */
              ((strcmp(MuttVars[idx].name, "charset") == 0) && !*buf->data))
          {
            snprintf(err->data, err->dsize, _("Invalid value for option %s: \"%s\""),
                     MuttVars[idx].name, buf->data);
            return -1;
          }

          FREE((void *) MuttVars[idx].var);
          *((char **) MuttVars[idx].var) = mutt_str_strdup(buf->data);
          if (mutt_str_strcmp(MuttVars[idx].name, "charset") == 0)
            mutt_ch_set_charset(Charset);

          if ((mutt_str_strcmp(MuttVars[idx].name,
                               "show_multipart_alternative") == 0) &&
              !valid_show_multipart_alternative(buf->data))
          {
            snprintf(err->data, err->dsize, _("Invalid value for name %s: \"%s\""),
                     MuttVars[idx].name, buf->data);
            return -1;
          }
        }
        else if (DTYPE(MuttVars[idx].type) == DT_MBTABLE)
        {
          free_mbtable((struct MbTable **) MuttVars[idx].var);
          *((struct MbTable **) MuttVars[idx].var) = parse_mbtable(buf->data);
        }
        else
        {
          mutt_addr_free((struct Address **) MuttVars[idx].var);
          *((struct Address **) MuttVars[idx].var) =
              mutt_addr_parse_list(NULL, buf->data);
        }
      }
    }
    else if ((idx >= 0) && (DTYPE(MuttVars[idx].type) == DT_REGEX))
    {
      if (query || *s->dptr != '=')
      {
        /* user requested the value of this variable */
        struct Regex *ptr = *(struct Regex **) MuttVars[idx].var;
        const char *value = ptr ? ptr->pattern : NULL;
        pretty_var(err->data, err->dsize, MuttVars[idx].name, NONULL(value));
        break;
      }

      if (OPT_ATTACH_MSG &&
          (mutt_str_strcmp(MuttVars[idx].name, "reply_regex") == 0))
      {
        snprintf(err->data, err->dsize, "Operation not permitted when in attach-message mode.");
        r = -1;
        break;
      }

      CHECK_PAGER;
      s->dptr++;

      /* copy the value of the string */
      mutt_extract_token(buf, s, 0);

      if (parse_regex(idx, buf, err))
      {
        /* $reply_regex and $alternates require special treatment */
        if (Context && Context->msgcount &&
            (mutt_str_strcmp(MuttVars[idx].name, "reply_regex") == 0))
        {
          regmatch_t pmatch[1];

          for (int i = 0; i < Context->msgcount; i++)
          {
            struct Envelope *e = Context->hdrs[i]->env;
            if (e && e->subject)
            {
              e->real_subj = (ReplyRegex && ReplyRegex->regex &&
                              (regexec(ReplyRegex->regex, e->subject, 1, pmatch, 0))) ?
                                 e->subject :
                                 e->subject + pmatch[0].rm_eo;
            }
          }
        }
      }
    }
    else if ((idx >= 0) && (DTYPE(MuttVars[idx].type) == DT_MAGIC))
    {
      if (query || *s->dptr != '=')
      {
        switch (MboxType)
        {
          case MUTT_MBOX:
            p = "mbox";
            break;
          case MUTT_MMDF:
            p = "MMDF";
            break;
          case MUTT_MH:
            p = "MH";
            break;
          case MUTT_MAILDIR:
            p = "Maildir";
            break;
          default:
            p = "unknown";
            break;
        }
        snprintf(err->data, err->dsize, "%s=%s", MuttVars[idx].name, p);
        break;
      }

      CHECK_PAGER;
      s->dptr++;

      /* copy the value of the string */
      mutt_extract_token(buf, s, 0);
      if (mx_set_magic(buf->data))
      {
        snprintf(err->data, err->dsize, _("%s: invalid mailbox type"), buf->data);
        r = -1;
        break;
      }
    }
    else if ((idx >= 0) && (DTYPE(MuttVars[idx].type) == DT_NUMBER))
    {
      short *ptr = (short *) MuttVars[idx].var;
      short val;
      int rc;

      if (query || *s->dptr != '=')
      {
        val = *ptr;
        /* compatibility alias */
        if (mutt_str_strcmp(MuttVars[idx].name, "wrapmargin") == 0)
          val = *ptr < 0 ? -*ptr : 0;

        /* user requested the value of this variable */
        snprintf(err->data, err->dsize, "%s=%d", MuttVars[idx].name, val);
        break;
      }

      CHECK_PAGER;
      s->dptr++;

      mutt_extract_token(buf, s, 0);
      rc = mutt_str_atos(buf->data, (short *) &val);

      if (rc < 0 || !*buf->data)
      {
        snprintf(err->data, err->dsize, _("%s: invalid value (%s)"), buf->data,
                 rc == -1 ? _("format error") : _("number overflow"));
        r = -1;
        break;
      }

      if (mutt_str_strcmp(MuttVars[idx].name, "debug_level") == 0)
        mutt_log_set_level(val, true);
      else
        *ptr = val;

      /* these ones need a sanity check */
      if (mutt_str_strcmp(MuttVars[idx].name, "history") == 0)
      {
        if (*ptr < 0)
          *ptr = 0;
        mutt_hist_init();
      }
      else if (mutt_str_strcmp(MuttVars[idx].name, "pager_index_lines") == 0)
      {
        if (*ptr < 0)
          *ptr = 0;
      }
      else if (mutt_str_strcmp(MuttVars[idx].name, "wrapmargin") == 0)
      {
        if (*ptr < 0)
          *ptr = 0;
        else
          *ptr = -*ptr;
      }
#ifdef USE_IMAP
      else if (mutt_str_strcmp(MuttVars[idx].name, "imap_pipeline_depth") == 0)
      {
        if (*ptr < 0)
          *ptr = 0;
      }
#endif
    }
    else if ((idx >= 0) && (DTYPE(MuttVars[idx].type) == DT_QUAD))
    {
      if (query)
      {
        static const char *const vals[] = { "no", "yes", "ask-no", "ask-yes" };

        snprintf(err->data, err->dsize, "%s=%s", MuttVars[idx].name,
                 vals[*(unsigned char *) MuttVars[idx].var]);
        break;
      }

      CHECK_PAGER;
      if (*s->dptr == '=')
      {
        s->dptr++;
        mutt_extract_token(buf, s, 0);
        if (mutt_str_strcasecmp("yes", buf->data) == 0)
          *(unsigned char *) MuttVars[idx].var = MUTT_YES;
        else if (mutt_str_strcasecmp("no", buf->data) == 0)
          *(unsigned char *) MuttVars[idx].var = MUTT_NO;
        else if (mutt_str_strcasecmp("ask-yes", buf->data) == 0)
          *(unsigned char *) MuttVars[idx].var = MUTT_ASKYES;
        else if (mutt_str_strcasecmp("ask-no", buf->data) == 0)
          *(unsigned char *) MuttVars[idx].var = MUTT_ASKNO;
        else
        {
          snprintf(err->data, err->dsize, _("%s: invalid value"), buf->data);
          r = -1;
          break;
        }
      }
      else
      {
        if (inv)
          *(unsigned char *) MuttVars[idx].var =
              toggle_quadoption(*(unsigned char *) MuttVars[idx].var);
        else if (unset)
          *(unsigned char *) MuttVars[idx].var = MUTT_NO;
        else
          *(unsigned char *) MuttVars[idx].var = MUTT_YES;
      }
    }
    else if ((idx >= 0) && (DTYPE(MuttVars[idx].type) == DT_SORT))
    {
      const struct Mapping *map = NULL;

      switch (MuttVars[idx].type & DT_SUBTYPE_MASK)
      {
        case DT_SORT_ALIAS:
          map = SortAliasMethods;
          break;
        case DT_SORT_AUX:
          map = SortAuxMethods;
          break;
        case DT_SORT_BROWSER:
          map = SortBrowserMethods;
          break;
        case DT_SORT_KEYS:
          if (WithCrypto & APPLICATION_PGP)
            map = SortKeyMethods;
          break;
        case DT_SORT_SIDEBAR:
          map = SortSidebarMethods;
          break;
        default:
          map = SortMethods;
          break;
      }

      if (!map)
      {
        snprintf(err->data, err->dsize, _("%s: Unknown type."), MuttVars[idx].name);
        r = -1;
        break;
      }

      if (query || *s->dptr != '=')
      {
        p = mutt_map_get_name(*((short *) MuttVars[idx].var) & SORT_MASK, map);

        snprintf(err->data, err->dsize, "%s=%s%s%s", MuttVars[idx].name,
                 (*((short *) MuttVars[idx].var) & SORT_REVERSE) ? "reverse-" : "",
                 (*((short *) MuttVars[idx].var) & SORT_LAST) ? "last-" : "", p);
        return 0;
      }
      CHECK_PAGER;
      s->dptr++;
      mutt_extract_token(buf, s, 0);

      if (parse_sort((short *) MuttVars[idx].var, buf->data, map, err) == -1)
      {
        r = -1;
        break;
      }
    }
#ifdef USE_HCACHE
    else if ((idx >= 0) && (DTYPE(MuttVars[idx].type) == DT_HCACHE))
    {
      if (query || (*s->dptr != '='))
      {
        pretty_var(err->data, err->dsize, MuttVars[idx].name,
                   NONULL((*(char **) MuttVars[idx].var)));
        break;
      }

      CHECK_PAGER;
      s->dptr++;

      /* copy the value of the string */
      mutt_extract_token(buf, s, 0);
      if (mutt_hcache_is_valid_backend(buf->data))
      {
        FREE((void *) MuttVars[idx].var);
        *(char **) (MuttVars[idx].var) = mutt_str_strdup(buf->data);
      }
      else
      {
        snprintf(err->data, err->dsize, _("%s: invalid backend"), buf->data);
        r = -1;
        break;
      }
    }
#endif
    else
    {
      snprintf(err->data, err->dsize, _("%s: unknown type"), MuttVars[idx].name);
      r = -1;
      break;
    }

    if (!myvar)
    {
      if (MuttVars[idx].flags & R_INDEX)
        mutt_menu_set_redraw_full(MENU_MAIN);
      if (MuttVars[idx].flags & R_PAGER)
        mutt_menu_set_redraw_full(MENU_PAGER);
      if (MuttVars[idx].flags & R_PAGER_FLOW)
      {
        mutt_menu_set_redraw_full(MENU_PAGER);
        mutt_menu_set_redraw(MENU_PAGER, REDRAW_FLOW);
      }
      if (MuttVars[idx].flags & R_RESORT_SUB)
        OPT_SORT_SUBTHREADS = true;
      if (MuttVars[idx].flags & R_RESORT)
        OPT_NEED_RESORT = true;
      if (MuttVars[idx].flags & R_RESORT_INIT)
        OPT_RESORT_INIT = true;
      if (MuttVars[idx].flags & R_TREE)
        OPT_REDRAW_TREE = true;
      if (MuttVars[idx].flags & R_REFLOW)
        mutt_window_reflow();
#ifdef USE_SIDEBAR
      if (MuttVars[idx].flags & R_SIDEBAR)
        mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
#endif
      if (MuttVars[idx].flags & R_MENU)
        mutt_menu_set_current_redraw_full();
    }
  }
  return r;
}

/* LIFO designed to contain the list of config files that have been sourced and
 * avoid cyclic sourcing */
static struct ListHead MuttrcStack = STAILQ_HEAD_INITIALIZER(MuttrcStack);

#define MAXERRS 128

/**
 * source_rc - Read an initialization file
 * @param rcfile_path Path to initialization file
 * @param err         Buffer for error messages
 * @retval <0 if neomutt should pause to let the user know
 */
static int source_rc(const char *rcfile_path, struct Buffer *err)
{
  FILE *f = NULL;
  int line = 0, rc = 0, line_rc, warnings = 0;
  struct Buffer token;
  char *linebuf = NULL;
  char *currentline = NULL;
  char rcfile[PATH_MAX];
  size_t buflen;
  size_t rcfilelen;
  bool ispipe;

  pid_t pid;

  mutt_str_strfcpy(rcfile, rcfile_path, PATH_MAX);

  rcfilelen = mutt_str_strlen(rcfile);
  if (rcfilelen == 0)
    return -1;

  ispipe = rcfile[rcfilelen - 1] == '|';

  if (!ispipe)
  {
    struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
    if (!mutt_file_to_absolute_path(rcfile, np ? NONULL(np->data) : ""))
    {
      mutt_error("Error: impossible to build path of '%s'.", rcfile_path);
      return -1;
    }

    STAILQ_FOREACH(np, &MuttrcStack, entries)
    {
      if (mutt_str_strcmp(np->data, rcfile) == 0)
      {
        break;
      }
    }
    if (!np)
    {
      mutt_list_insert_head(&MuttrcStack, mutt_str_strdup(rcfile));
    }
    else
    {
      mutt_error("Error: Cyclic sourcing of configuration file '%s'.", rcfile);
      return -1;
    }
  }

  mutt_debug(2, "Reading configuration file '%s'.\n", rcfile);

  f = mutt_open_read(rcfile, &pid);
  if (!f)
  {
    snprintf(err->data, err->dsize, "%s: %s", rcfile, strerror(errno));
    return -1;
  }

  mutt_buffer_init(&token);
  while ((linebuf = mutt_file_read_line(linebuf, &buflen, f, &line, MUTT_CONT)) != NULL)
  {
    const int conv = ConfigCharset && (*ConfigCharset) && Charset;
    if (conv)
    {
      currentline = mutt_str_strdup(linebuf);
      if (!currentline)
        continue;
      mutt_ch_convert_string(&currentline, ConfigCharset, Charset, 0);
    }
    else
      currentline = linebuf;

    line_rc = mutt_parse_rc_line(currentline, &token, err);
    if (line_rc == -1)
    {
      mutt_error(_("Error in %s, line %d: %s"), rcfile, line, err->data);
      if (--rc < -MAXERRS)
      {
        if (conv)
          FREE(&currentline);
        break;
      }
    }
    else if (line_rc == -2)
    {
      /* Warning */
      mutt_error(_("Warning in %s, line %d: %s"), rcfile, line, err->data);
      warnings++;
    }
    else if (line_rc == 1)
    {
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
  FREE(&token.data);
  FREE(&linebuf);
  mutt_file_fclose(&f);
  if (pid != -1)
    mutt_wait_filter(pid);
  if (rc)
  {
    /* the neomuttrc source keyword */
    snprintf(err->data, err->dsize,
             rc >= -MAXERRS ?
                 _("source: errors in %s") :
                 _("source: reading aborted due to too many errors in %s"),
             rcfile);
    rc = -1;
  }
  else
  {
    /* Don't alias errors with warnings */
    if (warnings > 0)
    {
      snprintf(err->data, err->dsize, _("source: %d warnings in %s"), warnings, rcfile);
      rc = -2;
    }
  }

  if (!ispipe && !STAILQ_EMPTY(&MuttrcStack))
  {
    STAILQ_REMOVE_HEAD(&MuttrcStack, entries);
  }

  return rc;
}

#undef MAXERRS

static int parse_source(struct Buffer *buf, struct Buffer *s,
                        unsigned long data, struct Buffer *err)
{
  char path[_POSIX_PATH_MAX];

  do
  {
    if (mutt_extract_token(buf, s, 0) != 0)
    {
      snprintf(err->data, err->dsize, _("source: error at %s"), s->dptr);
      return -1;
    }
    mutt_str_strfcpy(path, buf->data, sizeof(path));
    mutt_expand_path(path, sizeof(path));

    if (source_rc(path, err) < 0)
    {
      snprintf(err->data, err->dsize, _("source: file %s could not be sourced."), path);
      return -1;
    }

  } while (MoreArgs(s));

  return 0;
}

/**
 * mutt_parse_rc_line - Parse a line of user config
 * @param line  config line to read
 * @param token scratch buffer to be used by parser
 * @param err   where to write error messages
 *
 * Caller should free token->data when finished.  the reason for this variable
 * is to avoid having to allocate and deallocate a lot of memory if we are
 * parsing many lines.  the caller can pass in the memory to use, which avoids
 * having to create new space for every call to this function.
 */
int mutt_parse_rc_line(/* const */ char *line, struct Buffer *token, struct Buffer *err)
{
  int i, r = 0;
  struct Buffer expn;

  if (!line || !*line)
    return 0;

  mutt_buffer_init(&expn);
  expn.data = expn.dptr = line;
  expn.dsize = mutt_str_strlen(line);

  *err->data = 0;

  SKIPWS(expn.dptr);
  while (*expn.dptr)
  {
    if (*expn.dptr == '#')
      break; /* rest of line is a comment */
    if (*expn.dptr == ';')
    {
      expn.dptr++;
      continue;
    }
    mutt_extract_token(token, &expn, 0);
    for (i = 0; Commands[i].name; i++)
    {
      if (mutt_str_strcmp(token->data, Commands[i].name) == 0)
      {
        r = Commands[i].func(token, &expn, Commands[i].data, err);
        if (r != 0)
        {              /* -1 Error, +1 Finish */
          goto finish; /* Propagate return code */
        }
        break; /* Continue with next command */
      }
    }
    if (!Commands[i].name)
    {
      snprintf(err->data, err->dsize, _("%s: unknown command"), NONULL(token->data));
      r = -1;
      break; /* Ignore the rest of the line */
    }
  }
finish:
  if (expn.destroy)
    FREE(&expn.data);
  return r;
}

#define NUMVARS mutt_array_size(MuttVars)
#define NUMCOMMANDS mutt_array_size(Commands)

/* initial string that starts completion. No telling how much crap
 * the user has typed so far. Allocate LONG_STRING just to be sure! */
static char UserTyped[LONG_STRING] = { 0 };

static int NumMatched = 0;             /* Number of matches for completion */
static char Completed[STRING] = { 0 }; /* completed string (command or variable) */
static const char **Matches;
/* this is a lie until mutt_init runs: */
static int MatchesListsize = MAX(NUMVARS, NUMCOMMANDS) + 10;

static void matches_ensure_morespace(int current)
{
  if (current > MatchesListsize - 2)
  {
    int base_space = MAX(NUMVARS, NUMCOMMANDS) + 1;
    int extra_space = MatchesListsize - base_space;
    extra_space *= 2;
    const int space = base_space + extra_space;
    mutt_mem_realloc(&Matches, space * sizeof(char *));
    memset(&Matches[current + 1], 0, space - current);
    MatchesListsize = space;
  }
}

/**
 * candidate - helper function for completion
 * @param dest Completion result gets here
 * @param src  Candidate for completion
 * @param try  User entered data for completion
 * @param len  Length of dest buffer
 *
 * Changes the dest buffer if necessary/possible to aid completion.
*/
static void candidate(char *dest, char *try, const char *src, size_t len)
{
  if (!dest || !try || !src)
    return;

  if (strstr(src, try) == src)
  {
    matches_ensure_morespace(NumMatched);
    Matches[NumMatched++] = src;
    if (dest[0] == 0)
      mutt_str_strfcpy(dest, src, len);
    else
    {
      int l;
      for (l = 0; src[l] && src[l] == dest[l]; l++)
        ;
      dest[l] = '\0';
    }
  }
}

#ifdef USE_LUA
const struct Command *mutt_command_get(const char *s)
{
  for (int i = 0; Commands[i].name; i++)
    if (mutt_str_strcmp(s, Commands[i].name) == 0)
      return &Commands[i];
  return NULL;
}
#endif

void mutt_commands_apply(void *data, void (*application)(void *, const struct Command *))
{
  for (int i = 0; Commands[i].name; i++)
    application(data, &Commands[i]);
}

int mutt_command_complete(char *buffer, size_t len, int pos, int numtabs)
{
  char *pt = buffer;
  int num;
  int spaces; /* keep track of the number of leading spaces on the line */
  struct MyVar *myv = NULL;

  SKIPWS(buffer);
  spaces = buffer - pt;

  pt = buffer + pos - spaces;
  while ((pt > buffer) && !isspace((unsigned char) *pt))
    pt--;

  if (pt == buffer) /* complete cmd */
  {
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      NumMatched = 0;
      mutt_str_strfcpy(UserTyped, pt, sizeof(UserTyped));
      memset(Matches, 0, MatchesListsize);
      memset(Completed, 0, sizeof(Completed));
      for (num = 0; Commands[num].name; num++)
        candidate(Completed, UserTyped, Commands[num].name, sizeof(Completed));
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buffer'. Fake successful return this time */
      if (UserTyped[0] == 0)
        return 1;
    }

    if (Completed[0] == 0 && UserTyped[0])
      return 0;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if (numtabs == 1 && NumMatched == 2)
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if (numtabs > 1 && NumMatched > 2)
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    /* return the completed command */
    strncpy(buffer, Completed, len - spaces);
  }
  else if ((mutt_str_strncmp(buffer, "set", 3) == 0) ||
           (mutt_str_strncmp(buffer, "unset", 5) == 0) ||
           (mutt_str_strncmp(buffer, "reset", 5) == 0) ||
           (mutt_str_strncmp(buffer, "toggle", 6) == 0))
  { /* complete variables */
    static const char *const prefixes[] = { "no", "inv", "?", "&", 0 };

    pt++;
    /* loop through all the possible prefixes (no, inv, ...) */
    if (mutt_str_strncmp(buffer, "set", 3) == 0)
    {
      for (num = 0; prefixes[num]; num++)
      {
        if (mutt_str_strncmp(pt, prefixes[num], mutt_str_strlen(prefixes[num])) == 0)
        {
          pt += mutt_str_strlen(prefixes[num]);
          break;
        }
      }
    }

    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      NumMatched = 0;
      mutt_str_strfcpy(UserTyped, pt, sizeof(UserTyped));
      memset(Matches, 0, MatchesListsize);
      memset(Completed, 0, sizeof(Completed));
      for (num = 0; MuttVars[num].name; num++)
        candidate(Completed, UserTyped, MuttVars[num].name, sizeof(Completed));
      for (myv = MyVars; myv; myv = myv->next)
        candidate(Completed, UserTyped, myv->name, sizeof(Completed));
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buffer'. Fake successful return this time */
      if (UserTyped[0] == 0)
        return 1;
    }

    if (Completed[0] == 0 && UserTyped[0])
      return 0;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if (numtabs == 1 && NumMatched == 2)
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if (numtabs > 1 && NumMatched > 2)
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    strncpy(pt, Completed, buffer + len - pt - spaces);
  }
  else if (mutt_str_strncmp(buffer, "exec", 4) == 0)
  {
    const struct Binding *menu = km_get_table(CurrentMenu);

    if (!menu && CurrentMenu != MENU_PAGER)
      menu = OpGeneric;

    pt++;
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      NumMatched = 0;
      mutt_str_strfcpy(UserTyped, pt, sizeof(UserTyped));
      memset(Matches, 0, MatchesListsize);
      memset(Completed, 0, sizeof(Completed));
      for (num = 0; menu[num].name; num++)
        candidate(Completed, UserTyped, menu[num].name, sizeof(Completed));
      /* try the generic menu */
      if (Completed[0] == 0 && CurrentMenu != MENU_PAGER)
      {
        menu = OpGeneric;
        for (num = 0; menu[num].name; num++)
          candidate(Completed, UserTyped, menu[num].name, sizeof(Completed));
      }
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buffer'. Fake successful return this time */
      if (UserTyped[0] == 0)
        return 1;
    }

    if (Completed[0] == 0 && UserTyped[0])
      return 0;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if (numtabs == 1 && NumMatched == 2)
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if (numtabs > 1 && NumMatched > 2)
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    strncpy(pt, Completed, buffer + len - pt - spaces);
  }
  else
    return 0;

  return 1;
}

int mutt_var_value_complete(char *buffer, size_t len, int pos)
{
  char *pt = buffer;

  if (buffer[0] == 0)
    return 0;

  SKIPWS(buffer);
  const int spaces = buffer - pt;

  pt = buffer + pos - spaces;
  while ((pt > buffer) && !isspace((unsigned char) *pt))
    pt--;
  pt++;           /* move past the space */
  if (*pt == '=') /* abort if no var before the '=' */
    return 0;

  if (mutt_str_strncmp(buffer, "set", 3) == 0)
  {
    int idx;
    char val[LONG_STRING];
    const char *myvarval = NULL;
    char var[STRING];
    mutt_str_strfcpy(var, pt, sizeof(var));
    /* ignore the trailing '=' when comparing */
    int vlen = mutt_str_strlen(var);
    if (vlen == 0)
      return 0;

    var[vlen - 1] = '\0';
    idx = mutt_option_index(var);
    if (idx == -1)
    {
      myvarval = myvar_get(var);
      if (myvarval)
      {
        pretty_var(pt, len - (pt - buffer), var, myvarval);
        return 1;
      }
      return 0; /* no such variable. */
    }
    else if (var_to_string(idx, val, sizeof(val)))
    {
      snprintf(pt, len - (pt - buffer), "%s=\"%s\"", var, val);
      return 1;
    }
  }
  return 0;
}

#ifdef USE_NOTMUCH

/**
 * complete_all_nm_tags - Pass a list of notmuch tags to the completion code
 */
static int complete_all_nm_tags(const char *pt)
{
  int tag_count_1 = 0;
  int tag_count_2 = 0;

  NumMatched = 0;
  mutt_str_strfcpy(UserTyped, pt, sizeof(UserTyped));
  memset(Matches, 0, MatchesListsize);
  memset(Completed, 0, sizeof(Completed));

  nm_longrun_init(Context, false);

  /* Work out how many tags there are. */
  if (nm_get_all_tags(Context, NULL, &tag_count_1) || tag_count_1 == 0)
    goto done;

  /* Free the old list, if any. */
  if (nm_tags)
  {
    for (int i = 0; nm_tags[i] != NULL; i++)
      FREE(&nm_tags[i]);
    FREE(&nm_tags);
  }
  /* Allocate a new list, with sentinel. */
  nm_tags = mutt_mem_malloc((tag_count_1 + 1) * sizeof(char *));
  nm_tags[tag_count_1] = NULL;

  /* Get all the tags. */
  if (nm_get_all_tags(Context, nm_tags, &tag_count_2) || tag_count_1 != tag_count_2)
  {
    FREE(&nm_tags);
    nm_tags = NULL;
    nm_longrun_done(Context);
    return -1;
  }

  /* Put them into the completion machinery. */
  for (int num = 0; num < tag_count_1; num++)
  {
    candidate(Completed, UserTyped, nm_tags[num], sizeof(Completed));
  }

  matches_ensure_morespace(NumMatched);
  Matches[NumMatched++] = UserTyped;

done:
  nm_longrun_done(Context);
  return 0;
}

/**
 * mutt_nm_query_complete - Complete to the nearest notmuch tag
 *
 * Complete the nearest "tag:"-prefixed string previous to pos.
 */
bool mutt_nm_query_complete(char *buffer, size_t len, int pos, int numtabs)
{
  char *pt = buffer;
  int spaces;

  SKIPWS(buffer);
  spaces = buffer - pt;

  pt = (char *) mutt_str_rstrnstr((char *) buffer, pos, "tag:");
  if (pt)
  {
    pt += 4;
    if (numtabs == 1)
    {
      /* First TAB. Collect all the matches */
      complete_all_nm_tags(pt);

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buffer'. Fake successful return this time.
       */
      if (UserTyped[0] == 0)
        return true;
    }

    if (Completed[0] == 0 && UserTyped[0])
      return false;

    /* NumMatched will _always_ be at least 1 since the initial
     * user-typed string is always stored */
    if (numtabs == 1 && NumMatched == 2)
      snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
    else if (numtabs > 1 && NumMatched > 2)
    {
      /* cycle through all the matches */
      snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
    }

    /* return the completed query */
    strncpy(pt, Completed, buffer + len - pt - spaces);
  }
  else
    return false;

  return true;
}

/**
 * mutt_nm_tag_complete - Complete to the nearest notmuch tag
 *
 * Complete the nearest "+" or "-" -prefixed string previous to pos.
 */
bool mutt_nm_tag_complete(char *buffer, size_t len, int numtabs)
{
  if (!buffer)
    return false;

  char *pt = buffer;

  /* Only examine the last token */
  char *last_space = strrchr(buffer, ' ');
  if (last_space)
    pt = (last_space + 1);

  /* Skip the +/- */
  if ((pt[0] == '+') || (pt[0] == '-'))
    pt++;

  if (numtabs == 1)
  {
    /* First TAB. Collect all the matches */
    complete_all_nm_tags(pt);

    /* All matches are stored. Longest non-ambiguous string is ""
     * i.e. don't change 'buffer'. Fake successful return this time.
     */
    if (UserTyped[0] == 0)
      return true;
  }

  if (Completed[0] == 0 && UserTyped[0])
    return false;

  /* NumMatched will _always_ be at least 1 since the initial
   * user-typed string is always stored */
  if (numtabs == 1 && NumMatched == 2)
    snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
  else if (numtabs > 1 && NumMatched > 2)
  {
    /* cycle through all the matches */
    snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
  }

  /* return the completed query */
  strncpy(pt, Completed, buffer + len - pt);

  return true;
}
#endif

int var_to_string(int idx, char *val, size_t len)
{
  char tmp[LONG_STRING];
  static const char *const vals[] = { "no", "yes", "ask-no", "ask-yes" };

  tmp[0] = '\0';

  if ((DTYPE(MuttVars[idx].type) == DT_STRING) || (DTYPE(MuttVars[idx].type) == DT_PATH))
  {
    mutt_str_strfcpy(tmp, NONULL(*((char **) MuttVars[idx].var)), sizeof(tmp));
    if (DTYPE(MuttVars[idx].type) == DT_PATH)
      mutt_pretty_mailbox(tmp, sizeof(tmp));
  }
  else if (DTYPE(MuttVars[idx].type) == DT_REGEX)
  {
    struct Regex *r = *(struct Regex **) MuttVars[idx].var;
    if (r)
      mutt_str_strfcpy(tmp, NONULL(r->pattern), sizeof(tmp));
  }
  else if (DTYPE(MuttVars[idx].type) == DT_MBTABLE)
  {
    struct MbTable *mbt = (*((struct MbTable **) MuttVars[idx].var));
    mutt_str_strfcpy(tmp, mbt ? NONULL(mbt->orig_str) : "", sizeof(tmp));
  }
  else if (DTYPE(MuttVars[idx].type) == DT_ADDRESS)
  {
    mutt_addr_write(tmp, sizeof(tmp), *((struct Address **) MuttVars[idx].var), false);
  }
  else if (DTYPE(MuttVars[idx].type) == DT_QUAD)
    mutt_str_strfcpy(tmp, vals[*(unsigned char *) MuttVars[idx].var], sizeof(tmp));
  else if (DTYPE(MuttVars[idx].type) == DT_NUMBER)
  {
    short sval = *((short *) MuttVars[idx].var);

    /* avert your eyes, gentle reader */
    if (mutt_str_strcmp(MuttVars[idx].name, "wrapmargin") == 0)
      sval = sval > 0 ? 0 : -sval;

    snprintf(tmp, sizeof(tmp), "%d", sval);
  }
  else if (DTYPE(MuttVars[idx].type) == DT_SORT)
  {
    const struct Mapping *map = NULL;
    const char *p = NULL;

    switch (MuttVars[idx].type & DT_SUBTYPE_MASK)
    {
      case DT_SORT_ALIAS:
        map = SortAliasMethods;
        break;
      case DT_SORT_BROWSER:
        map = SortBrowserMethods;
        break;
      case DT_SORT_KEYS:
        if (WithCrypto & APPLICATION_PGP)
          map = SortKeyMethods;
        else
          map = SortMethods;
        break;
      default:
        map = SortMethods;
        break;
    }
    p = mutt_map_get_name(*((short *) MuttVars[idx].var) & SORT_MASK, map);
    snprintf(tmp, sizeof(tmp), "%s%s%s",
             (*((short *) MuttVars[idx].var) & SORT_REVERSE) ? "reverse-" : "",
             (*((short *) MuttVars[idx].var) & SORT_LAST) ? "last-" : "", p);
  }
  else if (DTYPE(MuttVars[idx].type) == DT_MAGIC)
  {
    char *p = NULL;

    switch (MboxType)
    {
      case MUTT_MBOX:
        p = "mbox";
        break;
      case MUTT_MMDF:
        p = "MMDF";
        break;
      case MUTT_MH:
        p = "MH";
        break;
      case MUTT_MAILDIR:
        p = "Maildir";
        break;
      default:
        p = "unknown";
    }
    mutt_str_strfcpy(tmp, p, sizeof(tmp));
  }
  else if (DTYPE(MuttVars[idx].type) == DT_BOOL)
    mutt_str_strfcpy(tmp, *(bool *) MuttVars[idx].var ? "yes" : "no", sizeof(tmp));
  else
    return 0;

  escape_string(val, len - 1, tmp);

  return 1;
}

/**
 * mutt_query_variables - Implement the -Q command line flag
 */
int mutt_query_variables(struct ListHead *queries)
{
  char command[STRING];

  struct Buffer err, token;

  mutt_buffer_init(&err);
  mutt_buffer_init(&token);

  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);

  struct ListNode *np;
  STAILQ_FOREACH(np, queries, entries)
  {
    snprintf(command, sizeof(command), "set ?%s\n", np->data);
    if (mutt_parse_rc_line(command, &token, &err) == -1)
    {
      mutt_error("%s", err.data);
      FREE(&token.data);
      FREE(&err.data);

      return 1; // TEST15: neomutt -Q missing
    }
    mutt_message("%s", err.data);
  }

  FREE(&token.data);
  FREE(&err.data);

  return 0; // TEST16: neomutt -Q charset
}

/**
 * mutt_dump_variables - Print a list of all variables with their values
 */
int mutt_dump_variables(int hide_sensitive)
{
  char command[STRING];

  struct Buffer err, token;

  mutt_buffer_init(&err);
  mutt_buffer_init(&token);

  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);

  for (int i = 0; MuttVars[i].name; i++)
  {
    if (MuttVars[i].type == DT_SYNONYM)
      continue;

    if (hide_sensitive && IS_SENSITIVE(MuttVars[i]))
    {
      mutt_message("%s='***'\n", MuttVars[i].name);
      continue;
    }
    snprintf(command, sizeof(command), "set ?%s\n", MuttVars[i].name);
    if (mutt_parse_rc_line(command, &token, &err) == -1)
    {
      mutt_message("%s", err.data);
      FREE(&token.data);
      FREE(&err.data);

      return 1; // TEST17: can't test
    }
    mutt_message("%s", err.data);
  }

  FREE(&token.data);
  FREE(&err.data);

  return 0;
}

static int execute_commands(struct ListHead *p)
{
  struct Buffer err, token;

  mutt_buffer_init(&err);
  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);
  mutt_buffer_init(&token);
  struct ListNode *np;
  STAILQ_FOREACH(np, p, entries)
  {
    if (mutt_parse_rc_line(np->data, &token, &err) == -1)
    {
      mutt_error(_("Error in command line: %s"), err.data);
      FREE(&token.data);
      FREE(&err.data);

      return -1;
    }
  }
  FREE(&token.data);
  FREE(&err.data);

  return 0;
}

static char *find_cfg(const char *home, const char *xdg_cfg_home)
{
  const char *names[] = {
    "neomuttrc",
    "muttrc",
    NULL,
  };

  const char *locations[][2] = {
    { xdg_cfg_home, "neomutt/" },
    { xdg_cfg_home, "mutt/" },
    { home, ".neomutt/" },
    { home, ".mutt/" },
    { home, "." },
    { NULL, NULL },
  };

  for (int i = 0; locations[i][0] || locations[i][1]; i++)
  {
    if (!locations[i][0])
      continue;

    for (int j = 0; names[j]; j++)
    {
      char buffer[STRING];

      snprintf(buffer, sizeof(buffer), "%s/%s%s", locations[i][0],
               locations[i][1], names[j]);
      if (access(buffer, F_OK) == 0)
        return mutt_str_strdup(buffer);
    }
  }

  return NULL;
}

/**
 * get_hostname - Find the Fully-Qualified Domain Name
 * @retval true  Success
 * @retval false Error, failed to find any name
 *
 * Use several methods to try to find the Fully-Qualified domain name of this host.
 * If the user has already configured a hostname, this function will use it.
 */
static bool get_hostname(void)
{
  char *str = NULL;
  struct utsname utsname;

  if (Hostname)
  {
    str = Hostname;
  }
  else
  {
    /* The call to uname() shouldn't fail, but if it does, the system is horribly
     * broken, and the system's networking configuration is in an unreliable
     * state.  We should bail.  */
    if ((uname(&utsname)) == -1)
    {
      mutt_perror(_("unable to determine nodename via uname()"));
      return false; // TEST09: can't test
    }

    str = utsname.nodename;
  }

  /* some systems report the FQDN instead of just the hostname */
  char *dot = strchr(str, '.');
  if (dot)
    ShortHostname = mutt_str_substr_dup(str, dot);
  else
    ShortHostname = mutt_str_strdup(str);

  if (!Hostname)
  {
    /* now get FQDN.  Use configured domain first, DNS next, then uname */
#ifdef DOMAIN
    /* we have a compile-time domain name, use that for Hostname */
    Hostname = mutt_mem_malloc(mutt_str_strlen(DOMAIN) + mutt_str_strlen(ShortHostname) + 2);
    sprintf((char *) Hostname, "%s.%s", NONULL(ShortHostname), DOMAIN);
#else
    Hostname = getmailname();
    if (!Hostname)
    {
      char buffer[LONG_STRING];
      if (getdnsdomainname(buffer, sizeof(buffer)) == 0)
      {
        Hostname = mutt_mem_malloc(mutt_str_strlen(buffer) + mutt_str_strlen(ShortHostname) + 2);
        sprintf((char *) Hostname, "%s.%s", NONULL(ShortHostname), buffer);
      }
      else
      {
        /* DNS failed, use the nodename.  Whether or not the nodename had a '.'
         * in it, we can use the nodename as the FQDN.  On hosts where DNS is
         * not being used, e.g. small network that relies on hosts files, a
         * short host name is all that is required for SMTP to work correctly.
         * It could be wrong, but we've done the best we can, at this point the
         * onus is on the user to provide the correct hostname if the nodename
         * won't work in their network.  */
        Hostname = mutt_str_strdup(utsname.nodename);
      }
    }
#endif
  }
  if (Hostname)
    set_default_value("hostname", (intptr_t) mutt_str_strdup(Hostname));

  return true;
}

int mutt_init(int skip_sys_rc, struct ListHead *commands)
{
  char buffer[LONG_STRING];
  int need_pause = 0;
  struct Buffer err;

  mutt_buffer_init(&err);
  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);
  err.dptr = err.data;

  Groups = mutt_hash_create(1031, 0);
  /* reverse alias keys need to be strdup'ed because of idna conversions */
  ReverseAliases = mutt_hash_create(
      1031, MUTT_HASH_STRCASECMP | MUTT_HASH_STRDUP_KEYS | MUTT_HASH_ALLOW_DUPS);
  TagTransforms = mutt_hash_create(64, MUTT_HASH_STRCASECMP);
  TagFormats = mutt_hash_create(64, 0);

  mutt_menu_init();

  snprintf(AttachmentMarker, sizeof(AttachmentMarker), "\033]9;%" PRIu64 "\a",
           mutt_rand64());

  const char *p = mutt_str_getenv("MAIL");
  if (p)
    SpoolFile = mutt_str_strdup(p);
  else if ((p = mutt_str_getenv("MAILDIR")))
    SpoolFile = mutt_str_strdup(p);
  else
  {
#ifdef HOMESPOOL
    mutt_file_concat_path(buffer, NONULL(HomeDir), MAILPATH, sizeof(buffer));
#else
    mutt_file_concat_path(buffer, MAILPATH, NONULL(Username), sizeof(buffer));
#endif
    SpoolFile = mutt_str_strdup(buffer);
  }

  p = mutt_str_getenv("REPLYTO");
  if (p)
  {
    struct Buffer buf, token;

    snprintf(buffer, sizeof(buffer), "Reply-To: %s", p);

    mutt_buffer_init(&buf);
    buf.data = buf.dptr = buffer;
    buf.dsize = mutt_str_strlen(buffer);

    mutt_buffer_init(&token);
    parse_my_hdr(&token, &buf, 0, &err);
    FREE(&token.data);
  }

  p = mutt_str_getenv("EMAIL");
  if (p)
    From = mutt_addr_parse_list(NULL, p);

  Charset = mutt_ch_get_langinfo_charset();
  mutt_ch_set_charset(Charset);

  Matches = mutt_mem_calloc(MatchesListsize, sizeof(char *));

  /* Set standard defaults */
  for (int i = 0; MuttVars[i].name; i++)
  {
    set_default(&MuttVars[i]);
    restore_default(&MuttVars[i]);
  }

  CurrentMenu = MENU_MAIN;

#ifndef LOCALES_HACK
  /* Do we have a locale definition? */
  if ((p = mutt_str_getenv("LC_ALL")) || (p = mutt_str_getenv("LANG")) ||
      (p = mutt_str_getenv("LC_CTYPE")))
  {
    OPT_LOCALES = true;
  }
#endif

#ifdef HAVE_GETSID
  /* Unset suspend by default if we're the session leader */
  if (getsid(0) == getpid())
    Suspend = false;
#endif

  mutt_hist_init();

  /* RFC2368, "4. Unsafe headers"
   * The creator of a mailto URL cannot expect the resolver of a URL to
   * understand more than the "subject" and "body" headers. Clients that
   * resolve mailto URLs into mail messages should be able to correctly
   * create RFC822-compliant mail messages using the "subject" and "body"
   * headers.
   */
  add_to_stailq(&MailToAllow, "body");
  add_to_stailq(&MailToAllow, "subject");
  /* Cc, In-Reply-To, and References help with not breaking threading on
   * mailing lists, see https://github.com/neomutt/neomutt/issues/115 */
  add_to_stailq(&MailToAllow, "cc");
  add_to_stailq(&MailToAllow, "in-reply-to");
  add_to_stailq(&MailToAllow, "references");

  if (STAILQ_EMPTY(&Muttrc))
  {
    const char *xdg_cfg_home = mutt_str_getenv("XDG_CONFIG_HOME");

    if (!xdg_cfg_home && HomeDir)
    {
      snprintf(buffer, sizeof(buffer), "%s/.config", HomeDir);
      xdg_cfg_home = buffer;
    }

    char *config = find_cfg(HomeDir, xdg_cfg_home);
    if (config)
    {
      mutt_list_insert_tail(&Muttrc, config);
    }
  }
  else
  {
    struct ListNode *np;
    STAILQ_FOREACH(np, &Muttrc, entries)
    {
      mutt_str_strfcpy(buffer, np->data, sizeof(buffer));
      FREE(&np->data);
      mutt_expand_path(buffer, sizeof(buffer));
      np->data = mutt_str_strdup(buffer);
      if (access(np->data, F_OK))
      {
        mutt_perror(np->data);
        return 1; // TEST10: neomutt -F missing
      }
    }
  }

  if (!STAILQ_EMPTY(&Muttrc))
  {
    FREE(&AliasFile);
    AliasFile = mutt_str_strdup(STAILQ_FIRST(&Muttrc)->data);
  }

  /* Process the global rc file if it exists and the user hasn't explicitly
     requested not to via "-n".  */
  if (!skip_sys_rc)
  {
    do
    {
      if (mutt_set_xdg_path(XDG_CONFIG_DIRS, buffer, sizeof(buffer)))
        break;

      snprintf(buffer, sizeof(buffer), "%s/neomuttrc", SYSCONFDIR);
      if (access(buffer, F_OK) == 0)
        break;

      snprintf(buffer, sizeof(buffer), "%s/Muttrc", SYSCONFDIR);
      if (access(buffer, F_OK) == 0)
        break;

      snprintf(buffer, sizeof(buffer), "%s/neomuttrc", PKGDATADIR);
      if (access(buffer, F_OK) == 0)
        break;

      snprintf(buffer, sizeof(buffer), "%s/Muttrc", PKGDATADIR);
    } while (0);
    if (access(buffer, F_OK) == 0)
    {
      if (source_rc(buffer, &err) != 0)
      {
        mutt_error("%s", err.data);
        need_pause = 1; // TEST11: neomutt (error in /etc/neomuttrc)
      }
    }
  }

  /* Read the user's initialization file.  */
  struct ListNode *np;
  STAILQ_FOREACH(np, &Muttrc, entries)
  {
    if (np->data)
    {
      if (source_rc(np->data, &err) != 0)
      {
        mutt_error("%s", err.data);
        need_pause = 1; // TEST12: neomutt (error in ~/.neomuttrc)
      }
    }
  }

  if (execute_commands(commands) != 0)
    need_pause = 1; // TEST13: neomutt -e broken

  if (!get_hostname())
    return 1;

  /* "$mailcap_path" precedence: environment, config file, code */
  const char *env_mc = mutt_str_getenv("MAILCAPS");
  if (env_mc)
    mutt_str_replace(&MailcapPath, env_mc);

  /* "$tmpdir" precedence: environment, config file, code */
  const char *env_tmp = mutt_str_getenv("TMPDIR");
  if (env_tmp)
    mutt_str_replace(&Tmpdir, env_tmp);

  /* "$visual", "$editor" precedence: environment, config file, code */
  const char *env_ed = mutt_str_getenv("VISUAL");
  if (!env_ed)
    env_ed = mutt_str_getenv("EDITOR");
  if (env_ed)
  {
    mutt_str_replace(&Editor, env_ed);
    mutt_str_replace(&Visual, env_ed);
  }

  if (need_pause && !OPT_NO_CURSES)
  {
    log_queue_flush(log_disp_terminal);
    if (mutt_any_key_to_continue(NULL) == 'q')
      return 1; // TEST14: neomutt -e broken (press 'q')
  }

  mutt_file_mkdir(Tmpdir, S_IRWXU);

  mutt_hist_read_file();

#ifdef USE_NOTMUCH
  if (VirtualSpoolfile)
  {
    /* Find the first virtual folder and open it */
    for (struct Buffy *b = Incoming; b; b = b->next)
    {
      if (b->magic == MUTT_NOTMUCH)
      {
        mutt_str_replace(&SpoolFile, b->path);
        mutt_sb_toggle_virtual();
        break;
      }
    }
  }
#endif

  FREE(&err.data);
  return 0;
}

int mutt_get_hook_type(const char *name)
{
  for (const struct Command *c = Commands; c->name; c++)
    if (c->func == mutt_parse_hook && (mutt_str_strcasecmp(c->name, name) == 0))
      return c->data;
  return 0;
}

static int parse_group_context(struct GroupContext **ctx, struct Buffer *buf,
                               struct Buffer *s, unsigned long data, struct Buffer *err)
{
  while (mutt_str_strcasecmp(buf->data, "-group") == 0)
  {
    if (!MoreArgs(s))
    {
      mutt_str_strfcpy(err->data, _("-group: no group name"), err->dsize);
      goto bail;
    }

    mutt_extract_token(buf, s, 0);

    mutt_group_context_add(ctx, mutt_pattern_group(buf->data));

    if (!MoreArgs(s))
    {
      mutt_str_strfcpy(err->data, _("out of arguments"), err->dsize);
      goto bail;
    }

    mutt_extract_token(buf, s, 0);
  }

  return 0;

bail:
  mutt_group_context_destroy(ctx);
  return -1;
}

static int parse_tag_transforms(struct Buffer *buf, struct Buffer *s,
                                unsigned long data, struct Buffer *err)
{
  if (!buf || !s)
    return -1;

  char *tmp = NULL;

  while (MoreArgs(s))
  {
    char *tag = NULL, *transform = NULL;

    mutt_extract_token(buf, s, 0);
    if (buf->data && *buf->data)
      tag = mutt_str_strdup(buf->data);
    else
      continue;

    mutt_extract_token(buf, s, 0);
    transform = mutt_str_strdup(buf->data);

    /* avoid duplicates */
    tmp = mutt_hash_find(TagTransforms, tag);
    if (tmp)
    {
      mutt_debug(3, "tag transform '%s' already registered as '%s'\n", tag, tmp);
      FREE(&tag);
      FREE(&transform);
      continue;
    }

    mutt_hash_insert(TagTransforms, tag, transform);
  }
  return 0;
}

static int parse_tag_formats(struct Buffer *buf, struct Buffer *s,
                             unsigned long data, struct Buffer *err)
{
  if (!buf || !s)
    return -1;

  char *tmp = NULL;

  while (MoreArgs(s))
  {
    char *tag = NULL, *format = NULL;

    mutt_extract_token(buf, s, 0);
    if (buf->data && *buf->data)
      tag = mutt_str_strdup(buf->data);
    else
      continue;

    mutt_extract_token(buf, s, 0);
    format = mutt_str_strdup(buf->data);

    /* avoid duplicates */
    tmp = mutt_hash_find(TagFormats, format);
    if (tmp)
    {
      mutt_debug(3, "tag format '%s' already registered as '%s'\n", format, tmp);
      FREE(&tag);
      FREE(&format);
      continue;
    }

    mutt_hash_insert(TagFormats, format, tag);
  }
  return 0;
}

#ifdef USE_IMAP
/**
 * parse_subscribe_to - 'subscribe-to' command: Add an IMAP subscription.
 * @param b    Buffer space shared by all command handlers
 * @param s    Current line of the config file
 * @param data Data field from init.h:struct Command
 * @param err  Buffer for any error message
 * @retval  0 Success
 * @retval -1 Failed
 *
 * The 'subscribe-to' command allows to subscribe to an IMAP-Mailbox.
 * Patterns are not supported.
 * Use it as follows: subscribe-to =folder
 */
static int parse_subscribe_to(struct Buffer *b, struct Buffer *s,
                              unsigned long data, struct Buffer *err)
{
  if (!b || !s || !err)
    return -1;

  mutt_buffer_reset(err);

  if (MoreArgs(s))
  {
    mutt_extract_token(b, s, 0);

    if (MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too many arguments"), "subscribe-to");
      return -1;
    }

    if (b->data && *b->data)
    {
      /* Expand and subscribe */
      if (imap_subscribe(mutt_expand_path(b->data, b->dsize), 1) != 0)
      {
        mutt_buffer_printf(err, _("Could not subscribe to %s"), b->data);
        return -1;
      }
      else
      {
        mutt_message(_("Subscribed to %s"), b->data);
        return 0;
      }
    }
    else
    {
      mutt_debug(5, "Corrupted buffer");
      return -1;
    }
  }

  mutt_buffer_addstr(err, _("No folder specified"));
  return -1;
}

/**
 * parse_unsubscribe_from - 'unsubscribe-from' command: Cancel an IMAP subscription.
 * @param b    Buffer space shared by all command handlers
 * @param s    Current line of the config file
 * @param data Data field from init.h:struct Command
 * @param err  Buffer for any error message
 * @retval  0 Success
 * @retval -1 Failed
 *
 * The 'unsubscribe-from' command allows to unsubscribe from an IMAP-Mailbox.
 * Patterns are not supported.
 * Use it as follows: unsubscribe-from =folder
 */
static int parse_unsubscribe_from(struct Buffer *b, struct Buffer *s,
                                  unsigned long data, struct Buffer *err)
{
  if (!b || !s || !err)
    return -1;

  if (MoreArgs(s))
  {
    mutt_extract_token(b, s, 0);

    if (MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too many arguments"), "unsubscribe-from");
      return -1;
    }

    if (b->data && *b->data)
    {
      /* Expand and subscribe */
      if (imap_subscribe(mutt_expand_path(b->data, b->dsize), 0) != 0)
      {
        mutt_buffer_printf(err, _("Could not unsubscribe from %s"), b->data);
        return -1;
      }
      else
      {
        mutt_message(_("Unsubscribed from %s"), b->data);
        return 0;
      }
    }
    else
    {
      mutt_debug(5, "Corrupted buffer");
      return -1;
    }
  }

  mutt_buffer_addstr(err, _("No folder specified"));
  return -1;
}
#endif

const char *myvar_get(const char *var)
{
  struct MyVar *cur = NULL;

  for (cur = MyVars; cur; cur = cur->next)
    if (mutt_str_strcmp(cur->name, var) == 0)
      return NONULL(cur->value);

  return NULL;
}

int mutt_label_complete(char *buffer, size_t len, int numtabs)
{
  char *pt = buffer;
  int spaces; /* keep track of the number of leading spaces on the line */

  if (!Context || !Context->label_hash)
    return 0;

  SKIPWS(buffer);
  spaces = buffer - pt;

  /* first TAB. Collect all the matches */
  if (numtabs == 1)
  {
    struct HashElem *entry = NULL;
    struct HashWalkState state;

    NumMatched = 0;
    mutt_str_strfcpy(UserTyped, buffer, sizeof(UserTyped));
    memset(Matches, 0, MatchesListsize);
    memset(Completed, 0, sizeof(Completed));
    memset(&state, 0, sizeof(state));
    while ((entry = mutt_hash_walk(Context->label_hash, &state)))
      candidate(Completed, UserTyped, entry->key.strkey, sizeof(Completed));
    matches_ensure_morespace(NumMatched);
    qsort(Matches, NumMatched, sizeof(char *), (sort_t *) mutt_str_strcasecmp);
    Matches[NumMatched++] = UserTyped;

    /* All matches are stored. Longest non-ambiguous string is ""
     * i.e. don't change 'buffer'. Fake successful return this time */
    if (UserTyped[0] == 0)
      return 1;
  }

  if (Completed[0] == 0 && UserTyped[0])
    return 0;

  /* NumMatched will _always_ be at least 1 since the initial
   * user-typed string is always stored */
  if (numtabs == 1 && NumMatched == 2)
    snprintf(Completed, sizeof(Completed), "%s", Matches[0]);
  else if (numtabs > 1 && NumMatched > 2)
  {
    /* cycle through all the matches */
    snprintf(Completed, sizeof(Completed), "%s", Matches[(numtabs - 2) % NumMatched]);
  }

  /* return the completed label */
  strncpy(buffer, Completed, len - spaces);

  return 1;
}

bool set_default_value(const char *name, intptr_t value)
{
  if (!name)
    return false;

  int idx = mutt_option_index(name);
  if (idx < 0)
    return false;

  MuttVars[idx].initial = value;
  return true;
}

void reset_value(const char *name)
{
  if (!name)
    return;

  int idx = mutt_option_index(name);
  if (idx < 0)
    return;

  restore_default(&MuttVars[idx]);
}
