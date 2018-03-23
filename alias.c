/**
 * @file
 * Representation of a single alias to an email address
 *
 * @authors
 * Copyright (C) 1996-2002 Michael R. Elkins <me@mutt.org>
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
#include <stddef.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "alias.h"
#include "envelope.h"
#include "globals.h"
#include "mutt_curses.h"
#include "options.h"
#include "protos.h"

struct Address *mutt_lookup_alias(const char *s)
{
  struct Alias *t = Aliases;

  for (; t; t = t->next)
    if (mutt_str_strcasecmp(s, t->name) == 0)
      return t->addr;
  return NULL; /* no such alias */
}

static struct Address *expand_aliases_r(struct Address *a, struct ListHead *expn)
{
  struct Address *head = NULL, *last = NULL, *t = NULL, *w = NULL;
  bool i;
  const char *fqdn = NULL;

  while (a)
  {
    if (!a->group && !a->personal && a->mailbox && strchr(a->mailbox, '@') == NULL)
    {
      t = mutt_lookup_alias(a->mailbox);

      if (t)
      {
        i = false;
        struct ListNode *np;
        STAILQ_FOREACH(np, expn, entries)
        {
          if (mutt_str_strcmp(a->mailbox, np->data) == 0) /* alias already found */
          {
            mutt_debug(1, "loop in alias found for '%s'\n", a->mailbox);
            i = true;
            break;
          }
        }

        if (!i)
        {
          mutt_list_insert_head(expn, mutt_str_strdup(a->mailbox));
          w = mutt_addr_copy_list(t, false);
          w = expand_aliases_r(w, expn);
          if (head)
            last->next = w;
          else
            head = last = w;
          while (last && last->next)
            last = last->next;
        }
        t = a;
        a = a->next;
        t->next = NULL;
        mutt_addr_free(&t);
        continue;
      }
      else
      {
        struct passwd *pw = getpwnam(a->mailbox);

        if (pw)
        {
          char namebuf[STRING];

          mutt_gecos_name(namebuf, sizeof(namebuf), pw);
          mutt_str_replace(&a->personal, namebuf);
        }
      }
    }

    if (head)
    {
      last->next = a;
      last = last->next;
    }
    else
      head = last = a;
    a = a->next;
    last->next = NULL;
  }

  if (UseDomain && (fqdn = mutt_fqdn(1)))
  {
    /* now qualify all local addresses */
    mutt_addr_qualify(head, fqdn);
  }

  return head;
}

struct Address *mutt_expand_aliases(struct Address *a)
{
  struct Address *t = NULL;
  struct ListHead expn; /* previously expanded aliases to avoid loops */

  STAILQ_INIT(&expn);
  t = expand_aliases_r(a, &expn);
  mutt_list_free(&expn);
  return (mutt_remove_duplicates(t));
}

void mutt_expand_aliases_env(struct Envelope *env)
{
  env->from = mutt_expand_aliases(env->from);
  env->to = mutt_expand_aliases(env->to);
  env->cc = mutt_expand_aliases(env->cc);
  env->bcc = mutt_expand_aliases(env->bcc);
  env->reply_to = mutt_expand_aliases(env->reply_to);
  env->mail_followup_to = mutt_expand_aliases(env->mail_followup_to);
}

/**
 * write_safe_address - Defang malicious email addresses
 *
 * if someone has an address like
 *      From: Michael `/bin/rm -f ~` Elkins <me@mutt.org>
 * and the user creates an alias for this, NeoMutt could wind up executing
 * the backticks because it writes aliases like
 *      alias me Michael `/bin/rm -f ~` Elkins <me@mutt.org>
 * To avoid this problem, use a backslash (\) to quote any backticks.  We also
 * need to quote backslashes as well, since you could defeat the above by
 * doing
 *      From: Michael \`/bin/rm -f ~\` Elkins <me@mutt.org>
 * since that would get aliased as
 *      alias me Michael \\`/bin/rm -f ~\\` Elkins <me@mutt.org>
 * which still gets evaluated because the double backslash is not a quote.
 *
 * Additionally, we need to quote ' and " characters - otherwise, neomutt will
 * interpret them on the wrong parsing step.
 *
 * $ wants to be quoted since it may indicate the start of an environment
 * variable.
 */
static void write_safe_address(FILE *fp, char *s)
{
  while (*s)
  {
    if (*s == '\\' || *s == '`' || *s == '\'' || *s == '"' || *s == '$')
      fputc('\\', fp);
    fputc(*s, fp);
    s++;
  }
}

struct Address *mutt_get_address(struct Envelope *env, char **pfxp)
{
  struct Address *addr = NULL;
  char *pfx = NULL;

  if (mutt_addr_is_user(env->from))
  {
    if (env->to && !mutt_is_mail_list(env->to))
    {
      pfx = "To";
      addr = env->to;
    }
    else
    {
      pfx = "Cc";
      addr = env->cc;
    }
  }
  else if (env->reply_to && !mutt_is_mail_list(env->reply_to))
  {
    pfx = "Reply-To";
    addr = env->reply_to;
  }
  else
  {
    addr = env->from;
    pfx = "From";
  }

  if (pfxp)
    *pfxp = pfx;

  return addr;
}

static void recode_buf(char *buf, size_t buflen)
{
  char *s = NULL;

  if (!ConfigCharset || !*ConfigCharset || !Charset)
    return;
  s = mutt_str_strdup(buf);
  if (!s)
    return;
  if (mutt_ch_convert_string(&s, Charset, ConfigCharset, 0) == 0)
    mutt_str_strfcpy(buf, s, buflen);
  FREE(&s);
}

/**
 * check_alias_name - Sanity-check an alias name
 *
 * Only characters which are non-special to both the RFC822 and the neomutt
 * configuration parser are permitted.
 */
int check_alias_name(const char *s, char *dest, size_t destlen)
{
  wchar_t wc;
  mbstate_t mb;
  size_t l;
  int rc = 0, dry = !dest || !destlen;

  memset(&mb, 0, sizeof(mbstate_t));

  if (!dry)
    destlen--;
  for (; s && *s && (dry || destlen) && (l = mbrtowc(&wc, s, MB_CUR_MAX, &mb)) != 0;
       s += l, destlen -= l)
  {
    int bad = l == (size_t)(-1) || l == (size_t)(-2); /* conversion error */
    bad = bad || (!dry && l > destlen); /* too few room for mb char */
    if (l == 1)
      bad = bad || (strchr("-_+=.", *s) == NULL && !iswalnum(wc));
    else
      bad = bad || !iswalnum(wc);
    if (bad)
    {
      if (dry)
        return -1;
      if (l == (size_t)(-1))
        memset(&mb, 0, sizeof(mbstate_t));
      *dest++ = '_';
      rc = -1;
    }
    else if (!dry)
    {
      memcpy(dest, s, l);
      dest += l;
    }
  }
  if (!dry)
    *dest = '\0';
  return rc;
}

void mutt_create_alias(struct Envelope *cur, struct Address *iaddr)
{
  struct Alias *new = NULL, *t = NULL;
  char buf[LONG_STRING], tmp[LONG_STRING], prompt[SHORT_STRING], *pc = NULL;
  char *err = NULL;
  char fixed[LONG_STRING];
  FILE *rc = NULL;
  struct Address *addr = NULL;

  if (cur)
  {
    addr = mutt_get_address(cur, NULL);
  }
  else if (iaddr)
  {
    addr = iaddr;
  }

  if (addr && addr->mailbox)
  {
    mutt_str_strfcpy(tmp, addr->mailbox, sizeof(tmp));
    pc = strchr(tmp, '@');
    if (pc)
      *pc = '\0';
  }
  else
    tmp[0] = '\0';

  /* Don't suggest a bad alias name in the event of a strange local part. */
  check_alias_name(tmp, buf, sizeof(buf));

retry_name:
  /* L10N: prompt to add a new alias */
  if (mutt_get_field(_("Alias as: "), buf, sizeof(buf), 0) != 0 || !buf[0])
    return;

  /* check to see if the user already has an alias defined */
  if (mutt_lookup_alias(buf))
  {
    mutt_error(_("You already have an alias defined with that name!"));
    return;
  }

  if (check_alias_name(buf, fixed, sizeof(fixed)))
  {
    switch (mutt_yesorno(_("Warning: This alias name may not work.  Fix it?"), MUTT_YES))
    {
      case MUTT_YES:
        mutt_str_strfcpy(buf, fixed, sizeof(buf));
        goto retry_name;
      case MUTT_ABORT:
        return;
    }
  }

  new = mutt_mem_calloc(1, sizeof(struct Alias));
  new->name = mutt_str_strdup(buf);

  mutt_addrlist_to_local(addr);

  if (addr && addr->mailbox)
    mutt_str_strfcpy(buf, addr->mailbox, sizeof(buf));
  else
    buf[0] = '\0';

  mutt_addrlist_to_intl(addr, NULL);

  do
  {
    if (mutt_get_field(_("Address: "), buf, sizeof(buf), 0) != 0 || !buf[0])
    {
      mutt_free_alias(&new);
      return;
    }

    new->addr = mutt_addr_parse_list(new->addr, buf);
    if (!new->addr)
      BEEP();
    if (mutt_addrlist_to_intl(new->addr, &err))
    {
      mutt_error(_("Error: '%s' is a bad IDN."), err);
      continue;
    }
  } while (!new->addr);

  if (addr && addr->personal && !mutt_is_mail_list(addr))
    mutt_str_strfcpy(buf, addr->personal, sizeof(buf));
  else
    buf[0] = '\0';

  if (mutt_get_field(_("Personal name: "), buf, sizeof(buf), 0) != 0)
  {
    mutt_free_alias(&new);
    return;
  }
  new->addr->personal = mutt_str_strdup(buf);

  buf[0] = '\0';
  mutt_addr_write(buf, sizeof(buf), new->addr, true);
  snprintf(prompt, sizeof(prompt), _("[%s = %s] Accept?"), new->name, buf);
  if (mutt_yesorno(prompt, MUTT_YES) != MUTT_YES)
  {
    mutt_free_alias(&new);
    return;
  }

  mutt_alias_add_reverse(new);

  t = Aliases;
  if (t)
  {
    while (t->next)
      t = t->next;
    t->next = new;
  }
  else
    Aliases = new;

  mutt_str_strfcpy(buf, NONULL(AliasFile), sizeof(buf));
  if (mutt_get_field(_("Save to file: "), buf, sizeof(buf), MUTT_FILE) != 0)
    return;
  mutt_expand_path(buf, sizeof(buf));
  rc = fopen(buf, "a+");
  if (rc)
  {
    /* terminate existing file with \n if necessary */
    if (fseek(rc, 0, SEEK_END))
      goto fseek_err;
    if (ftell(rc) > 0)
    {
      if (fseek(rc, -1, SEEK_CUR) < 0)
        goto fseek_err;
      if (fread(buf, 1, 1, rc) != 1)
      {
        mutt_perror(_("Error reading alias file"));
        mutt_file_fclose(&rc);
        return;
      }
      if (fseek(rc, 0, SEEK_END) < 0)
        goto fseek_err;
      if (buf[0] != '\n')
        fputc('\n', rc);
    }

    if (check_alias_name(new->name, NULL, 0))
      mutt_file_quote_filename(buf, sizeof(buf), new->name);
    else
      mutt_str_strfcpy(buf, new->name, sizeof(buf));
    recode_buf(buf, sizeof(buf));
    fprintf(rc, "alias %s ", buf);
    buf[0] = '\0';
    mutt_addr_write(buf, sizeof(buf), new->addr, false);
    recode_buf(buf, sizeof(buf));
    write_safe_address(rc, buf);
    fputc('\n', rc);
    if (mutt_file_fsync_close(&rc) != 0)
      mutt_message("Trouble adding alias: %s.", strerror(errno));
    else
      mutt_message(_("Alias added."));
  }
  else
    mutt_perror(buf);

  return;

fseek_err:
  mutt_perror(_("Error seeking in alias file"));
  mutt_file_fclose(&rc);
  return;
}

/**
 * alias_reverse_lookup - Does the user have an alias for the given address
 */
struct Address *alias_reverse_lookup(struct Address *a)
{
  if (!a || !a->mailbox)
    return NULL;

  return mutt_hash_find(ReverseAliases, a->mailbox);
}

void mutt_alias_add_reverse(struct Alias *t)
{
  struct Address *ap = NULL;
  if (!t)
    return;

  /* Note that the address mailbox should be converted to intl form
   * before using as a key in the hash.  This is currently done
   * by all callers, but added here mostly as documentation.. */
  mutt_addrlist_to_intl(t->addr, NULL);

  for (ap = t->addr; ap; ap = ap->next)
  {
    if (!ap->group && ap->mailbox)
      mutt_hash_insert(ReverseAliases, ap->mailbox, ap);
  }
}

void mutt_alias_delete_reverse(struct Alias *t)
{
  struct Address *ap = NULL;
  if (!t)
    return;

  /* If the alias addresses were converted to local form, they won't
   * match the hash entries. */
  mutt_addrlist_to_intl(t->addr, NULL);

  for (ap = t->addr; ap; ap = ap->next)
  {
    if (!ap->group && ap->mailbox)
      mutt_hash_delete(ReverseAliases, ap->mailbox, ap);
  }
}

/**
 * mutt_alias_complete - alias completion routine
 *
 * given a partial alias, this routine attempts to fill in the alias
 * from the alias list as much as possible. if given empty search string
 * or found nothing, present all aliases
 */
int mutt_alias_complete(char *s, size_t buflen)
{
  struct Alias *a = Aliases;
  struct Alias *a_list = NULL, *a_cur = NULL;
  char bestname[HUGE_STRING];

  if (s[0] != 0) /* avoid empty string as strstr argument */
  {
    memset(bestname, 0, sizeof(bestname));

    while (a)
    {
      if (a->name && strstr(a->name, s) == a->name)
      {
        if (!bestname[0]) /* init */
          mutt_str_strfcpy(bestname, a->name,
                           MIN(mutt_str_strlen(a->name) + 1, sizeof(bestname)));
        else
        {
          int i;
          for (i = 0; a->name[i] && a->name[i] == bestname[i]; i++)
            ;
          bestname[i] = '\0';
        }
      }
      a = a->next;
    }

    if (bestname[0] != 0)
    {
      if (mutt_str_strcmp(bestname, s) != 0)
      {
        /* we are adding something to the completion */
        mutt_str_strfcpy(s, bestname, mutt_str_strlen(bestname) + 1);
        return 1;
      }

      /* build alias list and show it */

      a = Aliases;
      while (a)
      {
        if (a->name && (strstr(a->name, s) == a->name))
        {
          if (!a_list) /* init */
            a_cur = a_list = mutt_mem_malloc(sizeof(struct Alias));
          else
          {
            a_cur->next = mutt_mem_malloc(sizeof(struct Alias));
            a_cur = a_cur->next;
          }
          memcpy(a_cur, a, sizeof(struct Alias));
          a_cur->next = NULL;
        }
        a = a->next;
      }
    }
  }

  bestname[0] = '\0';
  mutt_alias_menu(bestname, sizeof(bestname), a_list ? a_list : Aliases);
  if (bestname[0] != 0)
    mutt_str_strfcpy(s, bestname, buflen);

  /* free the alias list */
  while (a_list)
  {
    a_cur = a_list;
    a_list = a_list->next;
    FREE(&a_cur);
  }

  /* remove any aliases marked for deletion */
  a_list = NULL;
  for (a_cur = Aliases; a_cur;)
  {
    if (a_cur->del)
    {
      if (a_list)
        a_list->next = a_cur->next;
      else
        Aliases = a_cur->next;

      a_cur->next = NULL;
      mutt_free_alias(&a_cur);

      if (a_list)
        a_cur = a_list;
      else
        a_cur = Aliases;
    }
    else
    {
      a_list = a_cur;
      a_cur = a_cur->next;
    }
  }

  return 0;
}

static bool string_is_address(const char *str, const char *u, const char *d)
{
  char buf[LONG_STRING];

  snprintf(buf, sizeof(buf), "%s@%s", NONULL(u), NONULL(d));
  if (mutt_str_strcasecmp(str, buf) == 0)
    return true;

  return false;
}

/**
 * mutt_addr_is_user - Does the address belong to the user
 * @retval true if the given address belongs to the user
 */
bool mutt_addr_is_user(struct Address *addr)
{
  const char *fqdn = NULL;

  /* NULL address is assumed to be the user. */
  if (!addr)
  {
    mutt_debug(5, "yes, NULL address\n");
    return true;
  }
  if (!addr->mailbox)
  {
    mutt_debug(5, "no, no mailbox\n");
    return false;
  }

  if (mutt_str_strcasecmp(addr->mailbox, Username) == 0)
  {
    mutt_debug(5, "#1 yes, %s = %s\n", addr->mailbox, Username);
    return true;
  }
  if (string_is_address(addr->mailbox, Username, ShortHostname))
  {
    mutt_debug(5, "#2 yes, %s = %s @ %s\n", addr->mailbox, Username, ShortHostname);
    return true;
  }
  fqdn = mutt_fqdn(0);
  if (string_is_address(addr->mailbox, Username, fqdn))
  {
    mutt_debug(5, "#3 yes, %s = %s @ %s\n", addr->mailbox, Username, NONULL(fqdn));
    return true;
  }
  fqdn = mutt_fqdn(1);
  if (string_is_address(addr->mailbox, Username, fqdn))
  {
    mutt_debug(5, "#4 yes, %s = %s @ %s\n", addr->mailbox, Username, NONULL(fqdn));
    return true;
  }

  if (From && (mutt_str_strcasecmp(From->mailbox, addr->mailbox) == 0))
  {
    mutt_debug(5, "#5 yes, %s = %s\n", addr->mailbox, From->mailbox);
    return true;
  }

  if (mutt_regexlist_match(Alternates, addr->mailbox))
  {
    mutt_debug(5, "yes, %s matched by alternates.\n", addr->mailbox);
    if (mutt_regexlist_match(UnAlternates, addr->mailbox))
      mutt_debug(5, "but, %s matched by unalternates.\n", addr->mailbox);
    else
      return true;
  }

  mutt_debug(5, "no, all failed.\n");
  return false;
}

void mutt_free_alias(struct Alias **p)
{
  struct Alias *t = NULL;

  while (*p)
  {
    t = *p;
    *p = (*p)->next;
    mutt_alias_delete_reverse(t);
    FREE(&t->name);
    mutt_addr_free(&t->addr);
    FREE(&t);
  }
}
