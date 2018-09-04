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
#include <pwd.h>
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
#include "email/email.h"
#include "mutt.h"
#include "init.h"
#include "alias.h"
#include "context.h"
#include "filter.h"
#include "group.h"
#include "hcache/hcache.h"
#include "keymap.h"
#include "menu.h"
#include "mutt_curses.h"
#include "mutt_window.h"
#include "mx.h"
#include "myvar.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "protos.h"
#include "sidebar.h"
#include "version.h"
#ifdef USE_NOTMUCH
#include "notmuch/mutt_notmuch.h"
#endif
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/* LIFO designed to contain the list of config files that have been sourced and
 * avoid cyclic sourcing */
static struct ListHead MuttrcStack = STAILQ_HEAD_INITIALIZER(MuttrcStack);

#define MAXERRS 128

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

#ifdef USE_NOTMUCH
/* List of tags found in last call to mutt_nm_query_complete(). */
static char **nm_tags;
#endif

/**
 * enum GroupState - Type of email address group
 */
enum GroupState
{
  GS_NONE,
  GS_RX,
  GS_ADDR
};

/**
 * add_to_stailq - Add a string to a list
 * @param head String list
 * @param str  String to add
 *
 * @note Duplicate or empty strings will not be added
 */
static void add_to_stailq(struct ListHead *head, const char *str)
{
  /* don't add a NULL or empty string to the list */
  if (!str || *str == '\0')
    return;

  /* check to make sure the item is not already on this list */
  struct ListNode *np = NULL;
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
 * alternates_clean - Clear the recipient valid flag of all emails
 */
static void alternates_clean(void)
{
  if (!Context)
    return;

  for (int i = 0; i < Context->mailbox->msg_count; i++)
    Context->hdrs[i]->recip_valid = false;
}

/**
 * attachments_clean - always wise to do what someone else did before
 */
static void attachments_clean(void)
{
  if (!Context)
    return;

  for (int i = 0; i < Context->mailbox->msg_count; i++)
    Context->hdrs[i]->attach_valid = false;
}

/**
 * matches_ensure_morespace - Allocate more space for auto-completion
 * @param current Current allocation
 */
static void matches_ensure_morespace(int current)
{
  if (current <= (MatchesListsize - 2))
    return;

  int base_space = MAX(NUMVARS, NUMCOMMANDS) + 1;
  int extra_space = MatchesListsize - base_space;
  extra_space *= 2;
  const int space = base_space + extra_space;
  mutt_mem_realloc(&Matches, space * sizeof(char *));
  memset(&Matches[current + 1], 0, space - current);
  MatchesListsize = space;
}

/**
 * candidate - helper function for completion
 * @param try  User entered data for completion
 * @param src  Candidate for completion
 * @param dest Completion result gets here
 * @param dlen Length of dest buffer
 *
 * Changes the dest buffer if necessary/possible to aid completion.
*/
static void candidate(char *try, const char *src, char *dest, size_t dlen)
{
  if (!dest || !try || !src)
    return;

  if (strstr(src, try) != src)
    return;

  matches_ensure_morespace(NumMatched);
  Matches[NumMatched++] = src;
  if (dest[0] == 0)
    mutt_str_strfcpy(dest, src, dlen);
  else
  {
    int l;
    for (l = 0; src[l] && src[l] == dest[l]; l++)
      ;
    dest[l] = '\0';
  }
}

/**
 * clear_subject_mods - Clear out all modified email subjects
 */
static void clear_subject_mods(void)
{
  if (!Context)
    return;

  for (int i = 0; i < Context->mailbox->msg_count; i++)
    FREE(&Context->hdrs[i]->env->disp_subj);
}

#ifdef USE_NOTMUCH
/**
 * complete_all_nm_tags - Pass a list of Notmuch tags to the completion code
 * @param pt List of all Notmuch tags
 * @retval  0 Success
 * @retval -1 Error
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
    for (int i = 0; nm_tags[i]; i++)
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
    candidate(UserTyped, nm_tags[num], Completed, sizeof(Completed));
  }

  matches_ensure_morespace(NumMatched);
  Matches[NumMatched++] = UserTyped;

done:
  nm_longrun_done(Context);
  return 0;
}
#endif

/**
 * execute_commands - Execute a set of NeoMutt commands
 * @param p List of command strings
 * @retval  0 Success, all the commands succeeded
 * @retval -1 Error
 */
static int execute_commands(struct ListHead *p)
{
  struct Buffer err, token;

  mutt_buffer_init(&err);
  err.dsize = STRING;
  err.data = mutt_mem_malloc(err.dsize);
  mutt_buffer_init(&token);
  struct ListNode *np = NULL;
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

/**
 * find_cfg - Find a config file
 * @param home         User's home directory
 * @param xdg_cfg_home XDG home directory
 * @retval ptr  Success, first matching directory
 * @retval NULL Error, no matching directories
 */
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
    Hostname =
        mutt_mem_malloc(mutt_str_strlen(DOMAIN) + mutt_str_strlen(ShortHostname) + 2);
    sprintf((char *) Hostname, "%s.%s", NONULL(ShortHostname), DOMAIN);
#else
    Hostname = getmailname();
    if (!Hostname)
    {
      char buffer[LONG_STRING];
      if (getdnsdomainname(buffer, sizeof(buffer)) == 0)
      {
        Hostname = mutt_mem_malloc(mutt_str_strlen(buffer) +
                                   mutt_str_strlen(ShortHostname) + 2);
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
    cs_str_initial_set(Config, "hostname", mutt_str_strdup(Hostname), NULL);

  return true;
}

/**
 * parse_attach_list - Parse the "attachments" command
 * @param buf  Buffer for temporary storage
 * @param s    Buffer containing the attachments command
 * @param head List of AttachMatch to add to
 * @param err  Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 */
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

/**
 * parse_group_context - Parse a group context
 * @param ctx  GroupContext to add to
 * @param buf  Temporary Buffer space
 * @param s    Buffer containing string to be parsed
 * @param data Flags associated with the command
 * @param err  Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 */
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

/**
 * parse_replace_list - Parse a string replacement rule - Implements ::command_t
 */
static int parse_replace_list(struct Buffer *buf, struct Buffer *s,
                              unsigned long data, struct Buffer *err)
{
  struct ReplaceList *list = (struct ReplaceList *) data;
  struct Buffer templ = { 0 };

  /* First token is a regex. */
  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "subjectrx");
    return -1;
  }
  mutt_extract_token(buf, s, 0);

  /* Second token is a replacement template */
  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "subjectrx");
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

/**
 * parse_unattach_list - Parse the "unattachments" command
 * @param buf  Buffer for temporary storage
 * @param s    Buffer containing the unattachments command
 * @param head List of AttachMatch to remove from
 * @param err  Buffer for error messages
 * @retval 0 Always
 */
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

/**
 * parse_unreplace_list - Remove a string replacement rule - Implements ::command_t
 */
static int parse_unreplace_list(struct Buffer *buf, struct Buffer *s,
                                unsigned long data, struct Buffer *err)
{
  struct ReplaceList *list = (struct ReplaceList *) data;

  /* First token is a regex. */
  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "unsubjectrx");
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

/**
 * print_attach_list - Print a list of attachments
 * @param h    List of attachments
 * @param op   Operation, e.g. '+', '-'
 * @param name Attached/Inline, 'A', 'I'
 * @retval 0 Always
 */
static int print_attach_list(struct ListHead *h, const char op, const char *name)
{
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, h, entries)
  {
    printf("attachments %c%s %s/%s\n", op, name,
           ((struct AttachMatch *) np->data)->major,
           ((struct AttachMatch *) np->data)->minor);
  }

  return 0;
}

/**
 * remove_from_stailq - Remove an item, matching a string, from a List
 * @param head Head of the List
 * @param str  String to match
 *
 * @note The string comparison is case-insensitive
 */
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

  mutt_str_strfcpy(rcfile, rcfile_path, sizeof(rcfile));

  rcfilelen = mutt_str_strlen(rcfile);
  if (rcfilelen == 0)
    return -1;

  ispipe = rcfile[rcfilelen - 1] == '|';

  if (!ispipe)
  {
    struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
    if (!mutt_path_to_absolute(rcfile, np ? NONULL(np->data) : ""))
    {
      mutt_error(_("Error: impossible to build path of '%s'"), rcfile_path);
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
      mutt_error(_("Error: Cyclic sourcing of configuration file '%s'"), rcfile);
      return -1;
    }
  }

  mutt_debug(2, "Reading configuration file '%s'.\n", rcfile);

  f = mutt_open_read(rcfile, &pid);
  if (!f)
  {
    mutt_buffer_printf(err, "%s: %s", rcfile, strerror(errno));
    return -1;
  }

  mutt_buffer_init(&token);
  while ((linebuf = mutt_file_read_line(linebuf, &buflen, f, &line, MUTT_CONT)))
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
    mutt_buffer_reset(err);
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
    mutt_buffer_reset(err);
    mutt_buffer_printf(err, (rc >= -MAXERRS) ? _("source: errors in %s") : _("source: reading aborted due to too many errors in %s"),
                       rcfile);
    rc = -1;
  }
  else
  {
    /* Don't alias errors with warnings */
    if (warnings > 0)
    {
      mutt_buffer_printf(err, ngettext("source: %d warning in %s", "source: %d warnings in %s", warnings),
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

  return rc;
}

/**
 * parse_alias - Parse the 'alias' command - Implements ::command_t
 */
static int parse_alias(struct Buffer *buf, struct Buffer *s, unsigned long data,
                       struct Buffer *err)
{
  struct Alias *tmp = NULL;
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
  TAILQ_FOREACH(tmp, &Aliases, entries)
  {
    if (mutt_str_strcasecmp(tmp->name, buf->data) == 0)
      break;
  }

  if (!tmp)
  {
    /* create a new alias */
    tmp = mutt_mem_calloc(1, sizeof(struct Alias));
    tmp->name = mutt_str_strdup(buf->data);
    TAILQ_INSERT_TAIL(&Aliases, tmp, entries);
    /* give the main addressbook code a chance */
    if (CurrentMenu == MENU_ALIAS)
      OptMenuCaller = true;
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

  if (mutt_addrlist_to_intl(tmp->addr, &estr))
  {
    mutt_buffer_printf(err, _("Warning: Bad IDN '%s' in alias '%s'"), estr, tmp->name);
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

/**
 * parse_alternates - Parse the 'alternates' command - Implements ::command_t
 */
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

/**
 * parse_attachments - Parse the 'attachments' command - Implements ::command_t
 */
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

/**
 * parse_echo - Parse the 'echo' command - Implements ::command_t
 */
static int parse_echo(struct Buffer *buf, struct Buffer *s, unsigned long data,
                      struct Buffer *err)
{
  if (!MoreArgs(s))
  {
    mutt_buffer_printf(err, _("%s: too few arguments"), "echo");
    return -1;
  }
  mutt_extract_token(buf, s, 0);
  OptForceRefresh = true;
  mutt_message("%s", buf->data);
  OptForceRefresh = false;
  mutt_sleep(0);

  return 0;
}

/**
 * parse_finish - Parse the 'finish' command - Implements ::command_t
 * @retval  1 Stop processing the current file
 * @retval -1 Failed
 *
 * If the 'finish' command is found, we should stop reading the current file.
 */
static int parse_finish(struct Buffer *buf, struct Buffer *s,
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
 * parse_group - Parse the 'group' and 'ungroup' commands - Implements ::command_t
 */
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
          mutt_buffer_printf(err, _("%sgroup: missing -rx or -addr"),
                             (data == MUTT_UNGROUP) ? "un" : "");
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
            mutt_buffer_printf(err, _("%sgroup: warning: bad IDN '%s'"),
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
 * parse_ifdef - Parse the 'ifdef' and 'ifndef' commands - Implements ::command_t
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
  bool res = false;
  struct Buffer token = { 0 };

  mutt_extract_token(buf, s, 0);

  /* is the item defined as a variable? */
  struct HashElem *he = cs_get_elem(Config, buf->data);
  res = (he != NULL);

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
    mutt_buffer_printf(err, _("%s: too few arguments"), (data ? "ifndef" : "ifdef"));
    return -1;
  }
  mutt_extract_token(buf, s, MUTT_TOKEN_SPACE);

  /* ifdef KNOWN_SYMBOL or ifndef UNKNOWN_SYMBOL */
  if ((res && (data == 0)) || (!res && (data == 1)))
  {
    int rc = mutt_parse_rc_line(buf->data, &token, err);
    if (rc == -1)
    {
      mutt_error(_("Error: %s"), err->data);
      FREE(&token.data);
      return -1;
    }
    FREE(&token.data);
    return rc;
  }
  return 0;
}

/**
 * parse_ignore - Parse the 'ignore' command - Implements ::command_t
 */
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

/**
 * parse_lists - Parse the 'lists' command - Implements ::command_t
 */
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
 * parse_my_hdr - Parse the 'my_hdr' command - Implements ::command_t
 */
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

#ifdef USE_SIDEBAR
/**
 * parse_path_list - Parse the 'sidebar_whitelist' command - Implements ::command_t
 */
static int parse_path_list(struct Buffer *buf, struct Buffer *s,
                           unsigned long data, struct Buffer *err)
{
  char path[PATH_MAX];

  do
  {
    mutt_extract_token(buf, s, 0);
    mutt_str_strfcpy(path, buf->data, sizeof(path));
    mutt_expand_path(path, sizeof(path));
    add_to_stailq((struct ListHead *) data, path);
  } while (MoreArgs(s));

  return 0;
}
#endif

#ifdef USE_SIDEBAR
/**
 * parse_path_unlist - Parse the 'unsidebar_whitelist' command - Implements ::command_t
 */
static int parse_path_unlist(struct Buffer *buf, struct Buffer *s,
                             unsigned long data, struct Buffer *err)
{
  char path[PATH_MAX];

  do
  {
    mutt_extract_token(buf, s, 0);
    /* Check for deletion of entire list */
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

/**
 * parse_set - Parse the 'set' family of commands - Implements ::command_t
 *
 * This is used by 'reset', 'set', 'toggle' and 'unset'.
 */
static int parse_set(struct Buffer *buf, struct Buffer *s, unsigned long data,
                     struct Buffer *err)
{
  /* The order must match `enum MuttSetCommand` */
  static const char *set_commands[] = { "set", "toggle", "unset", "reset" };

  int rc = 0;

  while (MoreArgs(s))
  {
    bool prefix = false;
    bool query = false;
    bool inv = (data == MUTT_SET_INV);
    bool reset = (data == MUTT_SET_RESET);
    bool unset = (data == MUTT_SET_UNSET);

    if (*s->dptr == '?')
    {
      prefix = true;
      query = true;
      s->dptr++;
    }
    else if (mutt_str_strncmp("no", s->dptr, 2) == 0)
    {
      prefix = true;
      unset = !unset;
      s->dptr += 2;
    }
    else if (mutt_str_strncmp("inv", s->dptr, 3) == 0)
    {
      prefix = true;
      inv = !inv;
      s->dptr += 3;
    }
    else if (*s->dptr == '&')
    {
      prefix = true;
      reset = true;
      s->dptr++;
    }

    if (prefix && (data != MUTT_SET_SET))
    {
      mutt_buffer_printf(err, "ERR22 cannot use 'inv', 'no', '&' or '?' with the '%s' command",
                         set_commands[data]);
      return -1;
    }

    /* get the variable name */
    mutt_extract_token(buf, s, MUTT_TOKEN_EQUAL | MUTT_TOKEN_QUESTION);

    bool bq = false;
    bool equals = false;

    struct HashElem *he = NULL;
    bool my = (mutt_str_strncmp("my_", buf->data, 3) == 0);
    if (!my)
    {
      he = cs_get_elem(Config, buf->data);
      if (!he)
      {
        if (reset && (mutt_str_strcmp(buf->data, "all") == 0))
        {
          struct HashElem **list = get_elem_list(Config);
          if (!list)
            return -1;

          for (size_t i = 0; list[i]; i++)
            cs_he_reset(Config, list[i], NULL);

          FREE(&list);
          break;
        }
        else
        {
          mutt_buffer_printf(err, "ERR01 unknown variable: %s", buf->data);
          return -1;
        }
      }

      bq = ((DTYPE(he->type) == DT_BOOL) || (DTYPE(he->type) == DT_QUAD));
    }

    if (*s->dptr == '?')
    {
      if (prefix)
      {
        mutt_buffer_printf(
            err, "ERR02 cannot use a prefix when querying a variable");
        return -1;
      }

      if (reset || unset || inv)
      {
        mutt_buffer_printf(err, "ERR03 cannot query a variable with the '%s' command",
                           set_commands[data]);
        return -1;
      }

      query = true;
      s->dptr++;
    }
    else if (*s->dptr == '=')
    {
      if (prefix)
      {
        mutt_buffer_printf(err,
                           "ERR04 cannot use prefix when setting a variable");
        return -1;
      }

      if (reset || unset || inv)
      {
        mutt_buffer_printf(err, "ERR05 cannot set a variable with the '%s' command",
                           set_commands[data]);
        return -1;
      }

      equals = true;
      s->dptr++;
    }

    if (!bq && (inv || (unset && prefix)))
    {
      if (data == MUTT_SET_SET)
      {
        mutt_buffer_printf(err, "ERR06 prefixes 'no' and 'inv' may only be "
                                "used with bool/quad variables");
      }
      else
      {
        mutt_buffer_printf(err, "ERR07 command '%s' can only be used with bool/quad variables",
                           set_commands[data]);
      }
      return -1;
    }

    if (reset)
    {
      // mutt_buffer_printf(err, "ACT24 reset variable %s", buf->data);
      if (he)
      {
        rc = cs_he_reset(Config, he, err);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
          return -1;
      }
      else
      {
        myvar_del(buf->data);
      }
      continue;
    }

    if ((data == MUTT_SET_SET) && !inv && !unset)
    {
      if (query)
      {
        // mutt_buffer_printf(err, "ACT08 query variable %s", buf->data);
        if (he)
        {
          mutt_buffer_addstr(err, buf->data);
          mutt_buffer_addch(err, '=');
          mutt_buffer_reset(buf);
          rc = cs_he_string_get(Config, he, buf);
          if (CSR_RESULT(rc) != CSR_SUCCESS)
          {
            mutt_buffer_addstr(err, buf->data);
            return -1;
          }
          pretty_var(buf->data, err);
        }
        else
        {
          const char *val = myvar_get(buf->data);
          if (val)
          {
            mutt_buffer_addstr(err, buf->data);
            mutt_buffer_addch(err, '=');
            pretty_var(val, err);
          }
          else
          {
            mutt_buffer_printf(err, _("%s: unknown variable"), buf->data);
            return -1;
          }
        }
        break;
      }
      else if (equals)
      {
        // mutt_buffer_printf(err, "ACT11 set variable %s to ", buf->data);
        const char *name = NULL;
        if (my)
        {
          name = mutt_str_strdup(buf->data);
        }
        mutt_extract_token(buf, s, MUTT_TOKEN_BACKTICK_VARS);
        if (my)
        {
          myvar_set(name, buf->data);
          FREE(&name);
        }
        else
        {
          if (DTYPE(he->type) == DT_PATH)
          {
            mutt_expand_path(buf->data, buf->dsize);
            char scratch[PATH_MAX];
            mutt_str_strfcpy(scratch, buf->data, sizeof(scratch));
            size_t scratchlen = mutt_str_strlen(scratch);
            if (!(he->type & DT_MAILBOX) && (scratchlen != 0))
            {
              if ((scratch[scratchlen - 1] != '|') && /* not a command */
                  (url_check_scheme(scratch) == U_UNKNOWN)) /* probably a local file */
              {
                struct ListNode *np = STAILQ_FIRST(&MuttrcStack);
                if (mutt_path_to_absolute(scratch, np ? NONULL(np->data) : "./"))
                {
                  mutt_buffer_reset(buf);
                  mutt_buffer_addstr(buf, scratch);
                }
                else
                {
                  mutt_error(_("Error: impossible to build path of '%s'"), scratch);
                }
              }
            }
          }
          else if (DTYPE(he->type) == DT_COMMAND)
          {
            char scratch[PATH_MAX];
            mutt_str_strfcpy(scratch, buf->data, sizeof(scratch));

            if (mutt_str_strcmp(buf->data, "builtin") != 0)
            {
              mutt_expand_path(scratch, sizeof(scratch));
            }
            mutt_buffer_reset(buf);
            mutt_buffer_addstr(buf, scratch);
          }

          rc = cs_he_string_set(Config, he, buf->data, err);
          if (CSR_RESULT(rc) != CSR_SUCCESS)
            return -1;
        }
        continue;
      }
      else
      {
        if (bq)
        {
          // mutt_buffer_printf(err, "ACT23 set variable %s to 'yes'", buf->data);
          rc = cs_he_native_set(Config, he, true, err);
          if (CSR_RESULT(rc) != CSR_SUCCESS)
            return -1;
          continue;
        }
        else
        {
          // mutt_buffer_printf(err, "ACT10 query variable %s", buf->data);
          if (he)
          {
            mutt_buffer_addstr(err, buf->data);
            mutt_buffer_addch(err, '=');
            mutt_buffer_reset(buf);
            rc = cs_he_string_get(Config, he, buf);
            if (CSR_RESULT(rc) != CSR_SUCCESS)
            {
              mutt_buffer_addstr(err, buf->data);
              return -1;
            }
            pretty_var(buf->data, err);
          }
          else
          {
            const char *val = myvar_get(buf->data);
            if (val)
            {
              mutt_buffer_addstr(err, buf->data);
              mutt_buffer_addch(err, '=');
              pretty_var(val, err);
            }
            else
            {
              mutt_buffer_printf(err, _("%s: unknown variable"), buf->data);
              return -1;
            }
          }
          break;
        }
      }
    }

    if (my)
    {
      myvar_del(buf->data);
    }
    else if (bq)
    {
      if (inv)
      {
        // mutt_buffer_printf(err, "ACT25 TOGGLE bool/quad variable %s", buf->data);
        if (DTYPE(he->type) == DT_BOOL)
          bool_he_toggle(Config, he, err);
        else
          quad_he_toggle(Config, he, err);
      }
      else
      {
        // mutt_buffer_printf(err, "ACT26 UNSET bool/quad variable %s", buf->data);
        rc = cs_he_native_set(Config, he, false, err);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
          return -1;
      }
      continue;
    }
    else
    {
      rc = cs_str_string_set(Config, buf->data, NULL, err);
      if (CSR_RESULT(rc) != CSR_SUCCESS)
        return -1;
    }
  }

  return 0;
}

/**
 * parse_setenv - Parse the 'setenv' and 'unsetenv' commands - Implements ::command_t
 */
static int parse_setenv(struct Buffer *buf, struct Buffer *s,
                        unsigned long data, struct Buffer *err)
{
  char **envp = mutt_envlist_getlist();

  bool query = false;
  bool unset = (data == MUTT_SET_UNSET);

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

    mutt_buffer_printf(err, _("%s is unset"), buf->data);
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

/**
 * parse_source - Parse the 'source' command - Implements ::command_t
 */
static int parse_source(struct Buffer *buf, struct Buffer *s,
                        unsigned long data, struct Buffer *err)
{
  char path[PATH_MAX];

  do
  {
    if (mutt_extract_token(buf, s, 0) != 0)
    {
      mutt_buffer_printf(err, _("source: error at %s"), s->dptr);
      return -1;
    }
    mutt_str_strfcpy(path, buf->data, sizeof(path));
    mutt_expand_path(path, sizeof(path));

    if (source_rc(path, err) < 0)
    {
      mutt_buffer_printf(err, _("source: file %s could not be sourced"), path);
      return -1;
    }

  } while (MoreArgs(s));

  return 0;
}

/**
 * parse_spam_list - Parse the 'spam' and 'nospam' commands - Implements ::command_t
 */
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

/**
 * parse_stailq - Parse a list command - Implements ::command_t
 *
 * This is used by 'alternative_order', 'auto_view' and several others.
 */
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

/**
 * parse_subjectrx_list - Parse the 'subjectrx' command - Implements ::command_t
 */
static int parse_subjectrx_list(struct Buffer *buf, struct Buffer *s,
                                unsigned long data, struct Buffer *err)
{
  int rc;

  rc = parse_replace_list(buf, s, data, err);
  if (rc == 0)
    clear_subject_mods();
  return rc;
}

/**
 * parse_subscribe - Parse the 'subscribe' command - Implements ::command_t
 */
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

#ifdef USE_IMAP
/**
 * parse_subscribe_to - Parse the 'subscribe-to' command - Implements ::command_t
 *
 * The 'subscribe-to' command allows to subscribe to an IMAP-Mailbox.
 * Patterns are not supported.
 * Use it as follows: subscribe-to =folder
 */
static int parse_subscribe_to(struct Buffer *buf, struct Buffer *s,
                              unsigned long data, struct Buffer *err)
{
  if (!buf || !s || !err)
    return -1;

  mutt_buffer_reset(err);

  if (MoreArgs(s))
  {
    mutt_extract_token(buf, s, 0);

    if (MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too many arguments"), "subscribe-to");
      return -1;
    }

    if (buf->data && *buf->data)
    {
      /* Expand and subscribe */
      if (imap_subscribe(mutt_expand_path(buf->data, buf->dsize), 1) != 0)
      {
        mutt_buffer_printf(err, _("Could not subscribe to %s"), buf->data);
        return -1;
      }
      else
      {
        mutt_message(_("Subscribed to %s"), buf->data);
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

/**
 * parse_tag_formats - Parse the 'tag-formats' command - Implements ::command_t
 */
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

/**
 * parse_tag_transforms - Parse the 'tag-transforms' command - Implements ::command_t
 */
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

/**
 * parse_unalias - Parse the 'unalias' command - Implements ::command_t
 */
static int parse_unalias(struct Buffer *buf, struct Buffer *s,
                         unsigned long data, struct Buffer *err)
{
  struct Alias *a = NULL;

  do
  {
    mutt_extract_token(buf, s, 0);

    if (mutt_str_strcmp("*", buf->data) == 0)
    {
      if (CurrentMenu == MENU_ALIAS)
      {
        TAILQ_FOREACH(a, &Aliases, entries)
        {
          a->del = true;
        }
        mutt_menu_set_current_redraw_full();
      }
      else
        mutt_aliaslist_free(&Aliases);
      break;
    }
    else
    {
      TAILQ_FOREACH(a, &Aliases, entries)
      {
        if (mutt_str_strcasecmp(buf->data, a->name) == 0)
        {
          if (CurrentMenu == MENU_ALIAS)
          {
            a->del = true;
            mutt_menu_set_current_redraw_full();
          }
          else
          {
            TAILQ_REMOVE(&Aliases, a, entries);
            mutt_alias_free(&a);
          }
          break;
        }
      }
    }
  } while (MoreArgs(s));
  return 0;
}

/**
 * parse_unalternates - Parse the 'unalternates' command - Implements ::command_t
 */
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

/**
 * parse_unattachments - Parse the 'unattachments' command - Implements ::command_t
 */
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

/**
 * parse_unignore - Parse the 'unignore' command - Implements ::command_t
 */
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

/**
 * parse_unlists - Parse the 'unlists' command - Implements ::command_t
 */
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

/**
 * parse_unmy_hdr - Parse the 'unmy_hdr' command - Implements ::command_t
 */
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

/**
 * parse_unstailq - Parse an unlist command - Implements ::command_t
 *
 * This is used by 'unalternative_order', 'unauto_view' and several others.
 */
static int parse_unstailq(struct Buffer *buf, struct Buffer *s,
                          unsigned long data, struct Buffer *err)
{
  do
  {
    mutt_extract_token(buf, s, 0);
    /* Check for deletion of entire list */
    if (mutt_str_strcmp(buf->data, "*") == 0)
    {
      mutt_list_free((struct ListHead *) data);
      break;
    }
    remove_from_stailq((struct ListHead *) data, buf->data);
  } while (MoreArgs(s));

  return 0;
}

/**
 * parse_unsubjectrx_list - Parse the 'unsubjectrx' command - Implements ::command_t
 */
static int parse_unsubjectrx_list(struct Buffer *buf, struct Buffer *s,
                                  unsigned long data, struct Buffer *err)
{
  int rc;

  rc = parse_unreplace_list(buf, s, data, err);
  if (rc == 0)
    clear_subject_mods();
  return rc;
}

/**
 * parse_unsubscribe - Parse the 'unsubscribe' command - Implements ::command_t
 */
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

#ifdef USE_IMAP
/**
 * parse_unsubscribe_from - Parse the 'unsubscribe-from' command - Implements ::command_t
 *
 * The 'unsubscribe-from' command allows to unsubscribe from an IMAP-Mailbox.
 * Patterns are not supported.
 * Use it as follows: unsubscribe-from =folder
 */
static int parse_unsubscribe_from(struct Buffer *buf, struct Buffer *s,
                                  unsigned long data, struct Buffer *err)
{
  if (!buf || !s || !err)
    return -1;

  if (MoreArgs(s))
  {
    mutt_extract_token(buf, s, 0);

    if (MoreArgs(s))
    {
      mutt_buffer_printf(err, _("%s: too many arguments"), "unsubscribe-from");
      return -1;
    }

    if (buf->data && *buf->data)
    {
      /* Expand and subscribe */
      if (imap_subscribe(mutt_expand_path(buf->data, buf->dsize), 0) != 0)
      {
        mutt_buffer_printf(err, _("Could not unsubscribe from %s"), buf->data);
        return -1;
      }
      else
      {
        mutt_message(_("Unsubscribed from %s"), buf->data);
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

#ifdef USE_LUA
/**
 * mutt_command_get - Get a Command by its name
 * @param s Command string to lookup
 * @retval ptr  Success, Command
 * @retval NULL Error, no such command
 */
const struct Command *mutt_command_get(const char *s)
{
  for (int i = 0; Commands[i].name; i++)
    if (mutt_str_strcmp(s, Commands[i].name) == 0)
      return &Commands[i];
  return NULL;
}
#endif

#ifdef USE_LUA
/**
 * mutt_commands_apply - Run a callback function on every Command
 * @param data        Data to pass to the callback function
 * @param application Callback function
 *
 * This is used by Lua to expose all of NeoMutt's Commands.
 */
void mutt_commands_apply(void *data, void (*application)(void *, const struct Command *))
{
  for (int i = 0; Commands[i].name; i++)
    application(data, &Commands[i]);
}
#endif

/**
 * mutt_dump_variables - Print a list of all variables with their values
 * @param hide_sensitive Don't display the values of private config items
 * @retval 0 Success
 * @retval 1 Error
 */
int mutt_dump_variables(bool hide_sensitive)
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
      mutt_message("%s='***'", MuttVars[i].name);
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

/**
 * mutt_extract_token - Extract one token from a string
 * @param dest  Buffer for the result
 * @param tok   Buffer containing tokens
 * @param flags Flags, e.g. #MUTT_TOKEN_SPACE
 * @retval  0 Success
 * @retval -1 Error
 */
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
          (ch == '?' && (flags & MUTT_TOKEN_QUESTION)) ||
          (ch == ';' && !(flags & MUTT_TOKEN_SEMICOLON)) ||
          ((flags & MUTT_TOKEN_PATTERN) && strchr("~%=!|", ch)))
      {
        break;
      }
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
      char *ptr = NULL;
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
      struct Buffer cmd;
      mutt_buffer_init(&cmd);
      *pc = '\0';
      if (flags & MUTT_TOKEN_BACKTICK_VARS)
      {
        /* recursively extract tokens to interpolate variables */
        mutt_extract_token(&cmd, tok,
                           MUTT_TOKEN_QUOTE | MUTT_TOKEN_SPACE | MUTT_TOKEN_COMMENT |
                               MUTT_TOKEN_SEMICOLON | MUTT_TOKEN_NOSHELL);
      }
      else
      {
        cmd.data = mutt_str_strdup(tok->dptr);
      }
      *pc = '`';
      pid = mutt_create_filter(cmd.data, NULL, &fp, NULL);
      if (pid < 0)
      {
        mutt_debug(1, "unable to fork command: %s\n", cmd);
        FREE(&cmd.data);
        return -1;
      }
      FREE(&cmd.data);

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
        pc = strchr(tok->dptr, '}');
        if (pc)
        {
          var = mutt_str_substr_dup(tok->dptr + 1, pc);
          tok->dptr = pc + 1;

          if ((flags & MUTT_TOKEN_NOSHELL))
          {
            mutt_buffer_addch(dest, ch);
            mutt_buffer_addch(dest, '{');
            mutt_buffer_addstr(dest, var);
            mutt_buffer_addch(dest, '}');
            FREE(&var);
          }
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
        struct Buffer result;
        mutt_buffer_init(&result);
        int rc = cs_str_string_get(Config, var, &result);

        if (CSR_RESULT(rc) == CSR_SUCCESS)
        {
          mutt_buffer_addstr(dest, result.data);
          FREE(&result.data);
        }
        else if ((env = myvar_get(var)))
        {
          mutt_buffer_addstr(dest, env);
        }
        else if (!(flags & MUTT_TOKEN_NOSHELL) && (env = mutt_str_getenv(var)))
        {
          mutt_buffer_addstr(dest, env);
        }
        else
        {
          mutt_buffer_addch(dest, ch);
          mutt_buffer_addstr(dest, var);
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

/**
 * mutt_free_attachmatch - Free an AttachMatch - Implements ::list_free_t
 *
 * @note We don't free minor because it is either a pointer into major,
 *       or a static string.
 */
void mutt_free_attachmatch(struct AttachMatch **am)
{
  if (!am || !*am)
    return;

  regfree(&(*am)->minor_regex);
  FREE(&(*am)->major);
  FREE(am);
}

/**
 * mutt_free_opts - clean up before quitting
 */
void mutt_free_opts(void)
{
  mutt_list_free(&MuttrcStack);

  FREE(&Matches);

  mutt_aliaslist_free(&Aliases);

  mutt_regexlist_free(&Alternates);
  mutt_regexlist_free(&MailLists);
  mutt_regexlist_free(&NoSpamList);
  mutt_regexlist_free(&SubscribedLists);
  mutt_regexlist_free(&UnAlternates);
  mutt_regexlist_free(&UnMailLists);
  mutt_regexlist_free(&UnSubscribedLists);

  mutt_hash_destroy(&Groups);
  mutt_hash_destroy(&ReverseAliases);
  mutt_hash_destroy(&TagFormats);
  mutt_hash_destroy(&TagTransforms);

  /* Lists of strings */
  mutt_list_free(&AlternativeOrderList);
  mutt_list_free(&AutoViewList);
  mutt_list_free(&HeaderOrderList);
  mutt_list_free(&Ignore);
  mutt_list_free(&MailToAllow);
  mutt_list_free(&MimeLookupList);
  mutt_list_free(&Muttrc);
  mutt_list_free(&MuttrcStack);
#ifdef USE_SIDEBAR
  mutt_list_free(&SidebarWhitelist);
#endif
  mutt_list_free(&UnIgnore);
  mutt_list_free(&UserHeader);

  /* Lists of AttachMatch */
  mutt_list_free_type(&AttachAllow, (list_free_t) mutt_free_attachmatch);
  mutt_list_free_type(&AttachExclude, (list_free_t) mutt_free_attachmatch);
  mutt_list_free_type(&InlineAllow, (list_free_t) mutt_free_attachmatch);
  mutt_list_free_type(&InlineExclude, (list_free_t) mutt_free_attachmatch);

  mutt_free_colors();

  FREE(&CurrentFolder);
  FREE(&HomeDir);
  FREE(&LastFolder);
  FREE(&ShortHostname);
  FREE(&Username);

  mutt_replacelist_free(&SpamList);
  mutt_replacelist_free(&SubjectRegexList);

  mutt_delete_hooks(0);

  mutt_hist_free();
  mutt_free_keys();

  mutt_regexlist_free(&NoSpamList);
}

/**
 * mutt_get_hook_type - Find a hook by name
 * @param name Name to find
 * @retval num Data associated with the hook
 * @retval 0   Error, no matching hook
 */
int mutt_get_hook_type(const char *name)
{
  for (const struct Command *c = Commands; c->name; c++)
    if (c->func == mutt_parse_hook && (mutt_str_strcasecmp(c->name, name) == 0))
      return c->data;
  return 0;
}

/**
 * mutt_init - Initialise NeoMutt
 * @param skip_sys_rc If true, don't read the system config file
 * @param commands    List of config commands to execute
 * @retval 0 Success
 * @retval 1 Error
 */
int mutt_init(bool skip_sys_rc, struct ListHead *commands)
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

  /* "$spoolfile" precedence: config file, environment */
  const char *p = mutt_str_getenv("MAIL");
  if (!p)
    p = mutt_str_getenv("MAILDIR");
  if (!p)
  {
#ifdef HOMESPOOL
    mutt_path_concat(buffer, NONULL(HomeDir), MAILPATH, sizeof(buffer));
#else
    mutt_path_concat(buffer, MAILPATH, NONULL(Username), sizeof(buffer));
#endif
    p = buffer;
  }
  cs_str_initial_set(Config, "spoolfile", p, NULL);
  cs_str_reset(Config, "spoolfile", NULL);

  p = mutt_str_getenv("REPLYTO");
  if (p)
  {
    struct Buffer buf, token;

    snprintf(buffer, sizeof(buffer), "Reply-To: %s", p);

    mutt_buffer_init(&buf);
    buf.data = buffer;
    buf.dptr = buffer;
    buf.dsize = mutt_str_strlen(buffer);

    mutt_buffer_init(&token);
    parse_my_hdr(&token, &buf, 0, &err); /* adds to UserHeader */
    FREE(&token.data);
  }

  p = mutt_str_getenv("EMAIL");
  if (p)
  {
    cs_str_initial_set(Config, "from", p, NULL);
    cs_str_reset(Config, "from", NULL);
  }

  /* "$mailcap_path" precedence: config file, environment, code */
  const char *env_mc = mutt_str_getenv("MAILCAPS");
  if (env_mc)
    cs_str_string_set(Config, "mailcap_path", env_mc, NULL);

  /* "$tmpdir" precedence: config file, environment, code */
  const char *env_tmp = mutt_str_getenv("TMPDIR");
  if (env_tmp)
    cs_str_string_set(Config, "tmpdir", env_tmp, NULL);

  /* "$visual", "$editor" precedence: config file, environment, code */
  const char *env_ed = mutt_str_getenv("VISUAL");
  if (!env_ed)
    env_ed = mutt_str_getenv("EDITOR");
  if (env_ed)
  {
    cs_str_string_set(Config, "editor", env_ed, NULL);
    cs_str_string_set(Config, "visual", env_ed, NULL);
  }

  Charset = mutt_ch_get_langinfo_charset();
  mutt_ch_set_charset(Charset);

  Matches = mutt_mem_calloc(MatchesListsize, sizeof(char *));

  CurrentMenu = MENU_MAIN;

#ifdef HAVE_GETSID
  /* Unset suspend by default if we're the session leader */
  if (getsid(0) == getpid())
    Suspend = false;
#endif

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
    struct ListNode *np = NULL;
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
    cs_str_string_set(Config, "alias_file", STAILQ_FIRST(&Muttrc)->data, NULL);
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
  struct ListNode *np = NULL;
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

  if (!Realname)
  {
    struct passwd *pw = getpwuid(getuid());
    if (pw)
    {
      char buf[STRING];
      Realname = mutt_str_strdup(mutt_gecos_name(buf, sizeof(buf), pw));
    }
  }
  cs_str_initial_set(Config, "realname", Realname, NULL);

  if (need_pause && !OptNoCurses)
  {
    log_queue_flush(log_disp_terminal);
    if (mutt_any_key_to_continue(NULL) == 'q')
      return 1; // TEST14: neomutt -e broken (press 'q')
  }

  mutt_file_mkdir(Tmpdir, S_IRWXU);

  mutt_hist_init();
  mutt_hist_read_file();

#ifdef USE_NOTMUCH
  if (VirtualSpoolfile)
  {
    /* Find the first virtual folder and open it */
    struct MailboxNode *mp = NULL;
    STAILQ_FOREACH(mp, &AllMailboxes, entries)
    {
      if (mp->b->magic == MUTT_NOTMUCH)
      {
        cs_str_string_set(Config, "spoolfile", mp->b->path, NULL);
        mutt_sb_toggle_virtual();
        break;
      }
    }
  }
#endif

  FREE(&err.data);
  return 0;
}

/**
 * mutt_parse_rc_line - Parse a line of user config
 * @param line  config line to read
 * @param token scratch buffer to be used by parser
 * @param err   where to write error messages
 * @retval  0 Success
 * @retval -1 Failure
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
  expn.data = line;
  expn.dptr = line;
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
      mutt_buffer_printf(err, _("%s: unknown command"), NONULL(token->data));
      r = -1;
      break; /* Ignore the rest of the line */
    }
  }
finish:
  if (expn.destroy)
    FREE(&expn.data);
  return r;
}

/**
 * mutt_query_variables - Implement the -Q command line flag
 * @param queries List of query strings
 * @retval 0 Success, all queries exist
 * @retval 1 Error
 */
int mutt_query_variables(struct ListHead *queries)
{
  struct Buffer *value = mutt_buffer_alloc(STRING);
  struct Buffer *tmp = mutt_buffer_alloc(STRING);
  int rc = 0;

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, queries, entries)
  {
    mutt_buffer_reset(value);

    struct HashElem *he = cs_get_elem(Config, np->data);
    if (!he)
    {
      rc = 1;
      continue;
    }

    int rv = cs_he_string_get(Config, he, value);
    if (CSR_RESULT(rv) != CSR_SUCCESS)
    {
      rc = 1;
      continue;
    }

    int type = DTYPE(he->type);
    if ((type == DT_PATH) && !(he->type & DT_MAILBOX))
      mutt_pretty_mailbox(value->data, value->dsize);

    if ((type != DT_BOOL) && (type != DT_NUMBER) && (type != DT_LONG) && (type != DT_QUAD))
    {
      mutt_buffer_reset(tmp);
      size_t len = pretty_var(value->data, tmp);
      mutt_str_strfcpy(value->data, tmp->data, len + 1);
    }

    dump_config_neo(Config, he, value, NULL, 0);
  }

  mutt_buffer_free(&value);
  mutt_buffer_free(&tmp);

  return rc; // TEST16: neomutt -Q charset
}

/**
 * query_quadoption - Ask the user a quad-question
 * @param opt    Option to use
 * @param prompt Message to show to the user
 * @retval num Result, e.g. #MUTT_YES
 */
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
 * mutt_command_complete - Complete a command name
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param pos     Cursor position in the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval 1 Success, a match
 * @retval 0 Error, no match
 */
int mutt_command_complete(char *buf, size_t buflen, int pos, int numtabs)
{
  char *pt = buf;
  int num;
  int spaces; /* keep track of the number of leading spaces on the line */
  struct MyVar *myv = NULL;

  SKIPWS(buf);
  spaces = buf - pt;

  pt = buf + pos - spaces;
  while ((pt > buf) && !isspace((unsigned char) *pt))
    pt--;

  if (pt == buf) /* complete cmd */
  {
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      NumMatched = 0;
      mutt_str_strfcpy(UserTyped, pt, sizeof(UserTyped));
      memset(Matches, 0, MatchesListsize);
      memset(Completed, 0, sizeof(Completed));
      for (num = 0; Commands[num].name; num++)
        candidate(UserTyped, Commands[num].name, Completed, sizeof(Completed));
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
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
    strncpy(buf, Completed, buflen - spaces);
  }
  else if ((mutt_str_strncmp(buf, "set", 3) == 0) ||
           (mutt_str_strncmp(buf, "unset", 5) == 0) ||
           (mutt_str_strncmp(buf, "reset", 5) == 0) ||
           (mutt_str_strncmp(buf, "toggle", 6) == 0))
  { /* complete variables */
    static const char *const prefixes[] = { "no", "inv", "?", "&", 0 };

    pt++;
    /* loop through all the possible prefixes (no, inv, ...) */
    if (mutt_str_strncmp(buf, "set", 3) == 0)
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
        candidate(UserTyped, MuttVars[num].name, Completed, sizeof(Completed));
      TAILQ_FOREACH(myv, &MyVars, entries)
      {
        candidate(UserTyped, myv->name, Completed, sizeof(Completed));
      }
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
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

    strncpy(pt, Completed, buf + buflen - pt - spaces);
  }
  else if (mutt_str_strncmp(buf, "exec", 4) == 0)
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
        candidate(UserTyped, menu[num].name, Completed, sizeof(Completed));
      /* try the generic menu */
      if (Completed[0] == 0 && CurrentMenu != MENU_PAGER)
      {
        menu = OpGeneric;
        for (num = 0; menu[num].name; num++)
          candidate(UserTyped, menu[num].name, Completed, sizeof(Completed));
      }
      matches_ensure_morespace(NumMatched);
      Matches[NumMatched++] = UserTyped;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time */
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

    strncpy(pt, Completed, buf + buflen - pt - spaces);
  }
  else
    return 0;

  return 1;
}

/**
 * mutt_label_complete - Complete a label name
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval 1 Success, a match
 * @retval 0 Error, no match
 */
int mutt_label_complete(char *buf, size_t buflen, int numtabs)
{
  char *pt = buf;
  int spaces; /* keep track of the number of leading spaces on the line */

  if (!Context || !Context->label_hash)
    return 0;

  SKIPWS(buf);
  spaces = buf - pt;

  /* first TAB. Collect all the matches */
  if (numtabs == 1)
  {
    struct HashElem *entry = NULL;
    struct HashWalkState state = { 0 };

    NumMatched = 0;
    mutt_str_strfcpy(UserTyped, buf, sizeof(UserTyped));
    memset(Matches, 0, MatchesListsize);
    memset(Completed, 0, sizeof(Completed));
    while ((entry = mutt_hash_walk(Context->label_hash, &state)))
      candidate(UserTyped, entry->key.strkey, Completed, sizeof(Completed));
    matches_ensure_morespace(NumMatched);
    qsort(Matches, NumMatched, sizeof(char *), (sort_t *) mutt_str_strcasecmp);
    Matches[NumMatched++] = UserTyped;

    /* All matches are stored. Longest non-ambiguous string is ""
     * i.e. don't change 'buf'. Fake successful return this time */
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
  strncpy(buf, Completed, buflen - spaces);

  return 1;
}

#ifdef USE_NOTMUCH
/**
 * mutt_nm_query_complete - Complete to the nearest notmuch tag
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param pos     Cursor position in the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval true  Success, a match
 * @retval false Error, no match
 *
 * Complete the nearest "tag:"-prefixed string previous to pos.
 */
bool mutt_nm_query_complete(char *buf, size_t buflen, int pos, int numtabs)
{
  char *pt = buf;
  int spaces;

  SKIPWS(buf);
  spaces = buf - pt;

  pt = (char *) mutt_str_rstrnstr((char *) buf, pos, "tag:");
  if (pt)
  {
    pt += 4;
    if (numtabs == 1)
    {
      /* First TAB. Collect all the matches */
      complete_all_nm_tags(pt);

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. don't change 'buf'. Fake successful return this time.
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
    strncpy(pt, Completed, buf + buflen - pt - spaces);
  }
  else
    return false;

  return true;
}
#endif

#ifdef USE_NOTMUCH
/**
 * mutt_nm_tag_complete - Complete to the nearest notmuch tag
 * @param buf     Buffer for the result
 * @param buflen  Length of the buffer
 * @param numtabs Number of times the user has hit 'tab'
 * @retval true  Success, a match
 * @retval false Error, no match
 *
 * Complete the nearest "+" or "-" -prefixed string previous to pos.
 */
bool mutt_nm_tag_complete(char *buf, size_t buflen, int numtabs)
{
  if (!buf)
    return false;

  char *pt = buf;

  /* Only examine the last token */
  char *last_space = strrchr(buf, ' ');
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
     * i.e. don't change 'buf'. Fake successful return this time.
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
  strncpy(pt, Completed, buf + buflen - pt);

  return true;
}
#endif

/**
 * mutt_var_value_complete - Complete a variable/value
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param pos    Cursor position in the buffer
 */
int mutt_var_value_complete(char *buf, size_t buflen, int pos)
{
  char *pt = buf;

  if (buf[0] == 0)
    return 0;

  SKIPWS(buf);
  const int spaces = buf - pt;

  pt = buf + pos - spaces;
  while ((pt > buf) && !isspace((unsigned char) *pt))
    pt--;
  pt++;           /* move past the space */
  if (*pt == '=') /* abort if no var before the '=' */
    return 0;

  if (mutt_str_strncmp(buf, "set", 3) == 0)
  {
    const char *myvarval = NULL;
    char var[STRING];
    mutt_str_strfcpy(var, pt, sizeof(var));
    /* ignore the trailing '=' when comparing */
    int vlen = mutt_str_strlen(var);
    if (vlen == 0)
      return 0;

    var[vlen - 1] = '\0';

    struct HashElem *he = cs_get_elem(Config, var);
    if (!he)
    {
      myvarval = myvar_get(var);
      if (myvarval)
      {
        struct Buffer *pretty = mutt_buffer_alloc(STRING);
        pretty_var(myvarval, pretty);
        snprintf(pt, buflen - (pt - buf), "%s=%s", var, pretty->data);
        mutt_buffer_free(&pretty);
        return 1;
      }
      return 0; /* no such variable. */
    }
    else
    {
      struct Buffer *value = mutt_buffer_alloc(STRING);
      struct Buffer *pretty = mutt_buffer_alloc(STRING);
      int rc = cs_he_string_get(Config, he, value);
      if (CSR_RESULT(rc) == CSR_SUCCESS)
      {
        pretty_var(value->data, pretty);
        snprintf(pt, buflen - (pt - buf), "%s=%s", var, pretty->data);
        mutt_buffer_free(&value);
        mutt_buffer_free(&pretty);
        return 0;
      }
      mutt_buffer_free(&value);
      mutt_buffer_free(&pretty);
      return 1;
    }
  }
  return 0;
}

/**
 * init_config - Initialise the config system
 * @retval ptr New Config Set
 */
struct ConfigSet *init_config(size_t size)
{
  struct ConfigSet *cs = cs_create(size);

  address_init(cs);
  bool_init(cs);
  command_init(cs);
  long_init(cs);
  magic_init(cs);
  mbtable_init(cs);
  number_init(cs);
  path_init(cs);
  quad_init(cs);
  regex_init(cs);
  sort_init(cs);
  string_init(cs);

  if (!cs_register_variables(cs, MuttVars, 0))
  {
    mutt_error("cs_register_variables() failed");
    cs_free(&cs);
    return NULL;
  }

  return cs;
}

/**
 * charset_validator - Validate the "charset" config variable - Implements ::cs_validator
 */
int charset_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                      intptr_t value, struct Buffer *err)
{
  if (value == 0)
    return CSR_SUCCESS;

  const char *str = (const char *) value;

  int rc = CSR_SUCCESS;
  bool strict = (strcmp(cdef->name, "send_charset") == 0);
  char *q = NULL;
  char *s = mutt_str_strdup(str);

  for (char *p = strtok_r(s, ":", &q); p; p = strtok_r(NULL, ":", &q))
  {
    if (!*p)
      continue;
    if (!mutt_ch_check_charset(p, strict))
    {
      rc = CSR_ERR_INVALID;
      mutt_buffer_printf(err, _("Invalid value for option %s: %s"), cdef->name, p);
      break;
    }
  }

  FREE(&s);
  return rc;
}

#ifdef USE_HCACHE
/**
 * hcache_validator - Validate the "header_cache_backend" config variable - Implements ::cs_validator
 */
int hcache_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                     intptr_t value, struct Buffer *err)
{
  if (value == 0)
    return CSR_SUCCESS;

  const char *str = (const char *) value;

  if (mutt_hcache_is_valid_backend(str))
    return CSR_SUCCESS;

  mutt_buffer_printf(err, _("Invalid value for option %s: %s"), cdef->name, str);
  return CSR_ERR_INVALID;
}
#endif

/**
 * pager_validator - Check for config variables that can't be set from the pager - Implements ::cs_validator
 */
int pager_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                    intptr_t value, struct Buffer *err)
{
  if (CurrentMenu == MENU_PAGER)
  {
    mutt_buffer_printf(err, _("Option %s may not be set or reset from the pager"),
                       cdef->name);
    return CSR_ERR_INVALID;
  }

  return CSR_SUCCESS;
}

/**
 * multipart_validator - Validate the "show_multipart_alternative" config variable - Implements ::cs_validator
 */
int multipart_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                        intptr_t value, struct Buffer *err)
{
  if (value == 0)
    return CSR_SUCCESS;

  const char *str = (const char *) value;

  if ((mutt_str_strcmp(str, "inline") == 0) || (mutt_str_strcmp(str, "info") == 0))
    return CSR_SUCCESS;

  mutt_buffer_printf(err, _("Invalid value for option %s: %s"), cdef->name, str);
  return CSR_ERR_INVALID;
}

/**
 * reply_validator - Validate the "reply_regex" config variable - Implements ::cs_validator
 */
int reply_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                    intptr_t value, struct Buffer *err)
{
  if (pager_validator(cs, cdef, value, err) != CSR_SUCCESS)
    return CSR_ERR_INVALID;

  if (!OptAttachMsg)
    return CSR_SUCCESS;

  mutt_buffer_printf(err, _("Option %s may not be set when in attach-message mode"),
                     cdef->name);
  return CSR_ERR_INVALID;
}
