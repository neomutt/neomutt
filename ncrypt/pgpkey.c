/**
 * @file
 * PGP key management routines
 *
 * @authors
 * Copyright (C) 1996-1997,2007 Michael R. Elkins <me@mutt.org>
 * Copyright (c) 1998-2003 Thomas Roessler <roessler@does-not-exist.org>
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
#include <limits.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "pgpkey.h"
#include "body.h"
#include "crypt.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "gnupgparse.h"
#include "keymap.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "pager.h"
#include "pgp.h"
#include "pgpinvoke.h"
#include "pgplib.h"
#include "protos.h"
#include "sort.h"

/**
 * struct PgpCache - List of cached PGP keys
 */
struct PgpCache
{
  char *what;
  char *dflt;
  struct PgpCache *next;
};

static struct PgpCache *id_defaults = NULL;

static const char trust_flags[] = "?- +";

static char *pgp_key_abilities(int flags)
{
  static char buf[3];

  if (!(flags & KEYFLAG_CANENCRYPT))
    buf[0] = '-';
  else if (flags & KEYFLAG_PREFER_SIGNING)
    buf[0] = '.';
  else
    buf[0] = 'e';

  if (!(flags & KEYFLAG_CANSIGN))
    buf[1] = '-';
  else if (flags & KEYFLAG_PREFER_ENCRYPTION)
    buf[1] = '.';
  else
    buf[1] = 's';

  buf[2] = '\0';

  return buf;
}

static char pgp_flags(int flags)
{
  if (flags & KEYFLAG_REVOKED)
    return 'R';
  else if (flags & KEYFLAG_EXPIRED)
    return 'X';
  else if (flags & KEYFLAG_DISABLED)
    return 'd';
  else if (flags & KEYFLAG_CRITICAL)
    return 'c';
  else
    return ' ';
}

static struct PgpKeyInfo *pgp_principal_key(struct PgpKeyInfo *key)
{
  if (key->flags & KEYFLAG_SUBKEY && key->parent)
    return key->parent;
  else
    return key;
}

/*
 * Format an entry on the PGP key selection menu.
 *
 * %n   number
 * %k   key id          %K      key id of the principal key
 * %u   user id
 * %a   algorithm       %A      algorithm of the princ. key
 * %l   length          %L      length of the princ. key
 * %f   flags           %F      flags of the princ. key
 * %c   capabilities    %C      capabilities of the princ. key
 * %t   trust/validity of the key-uid association
 * %[...] date of key using strftime(3)
 */

/**
 * struct PgpEntry - An entry in a PGP key menu
 */
struct PgpEntry
{
  size_t num;
  struct PgpUid *uid;
};

static const char *pgp_entry_fmt(char *buf, size_t buflen, size_t col, int cols,
                                 char op, const char *src, const char *prec,
                                 const char *if_str, const char *else_str,
                                 unsigned long data, enum FormatFlag flags)
{
  char fmt[SHORT_STRING];
  struct PgpEntry *entry = NULL;
  struct PgpUid *uid = NULL;
  struct PgpKeyInfo *key = NULL, *pkey = NULL;
  int kflags = 0;
  int optional = (flags & MUTT_FORMAT_OPTIONAL);

  entry = (struct PgpEntry *) data;
  uid = entry->uid;
  key = uid->parent;
  pkey = pgp_principal_key(key);

  if (isupper((unsigned char) op))
    key = pkey;

  kflags = key->flags | (pkey->flags & KEYFLAG_RESTRICTIONS) | uid->flags;

  switch (tolower(op))
  {
    case '[':

    {
      const char *cp = NULL;
      char buf2[SHORT_STRING], *p = NULL;
      int do_locales;
      struct tm *tm = NULL;
      size_t len;

      p = buf;

      cp = src;
      if (*cp == '!')
      {
        do_locales = 0;
        cp++;
      }
      else
        do_locales = 1;

      len = buflen - 1;
      while (len > 0 && *cp != ']')
      {
        if (*cp == '%')
        {
          cp++;
          if (len >= 2)
          {
            *p++ = '%';
            *p++ = *cp;
            len -= 2;
          }
          else
            break; /* not enough space */
          cp++;
        }
        else
        {
          *p++ = *cp++;
          len--;
        }
      }
      *p = 0;

      tm = localtime(&key->gen_time);

      if (!do_locales)
        setlocale(LC_TIME, "C");
      strftime(buf2, sizeof(buf2), buf, tm);
      if (!do_locales)
        setlocale(LC_TIME, "");

      snprintf(fmt, sizeof(fmt), "%%%ss", prec);
      snprintf(buf, buflen, fmt, buf2);
      if (len > 0)
        src = cp + 1;
    }
    break;
    case 'n':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, entry->num);
      }
      break;
    case 'k':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, pgp_this_keyid(key));
      }
      break;
    case 'u':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(uid->addr));
      }
      break;
    case 'a':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, key->algorithm);
      }
      break;
    case 'l':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, key->keylen);
      }
      break;
    case 'f':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        snprintf(buf, buflen, fmt, pgp_flags(kflags));
      }
      else if (!(kflags & (KEYFLAG_RESTRICTIONS)))
        optional = 0;
      break;
    case 'c':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, pgp_key_abilities(kflags));
      }
      else if (!(kflags & (KEYFLAG_ABILITIES)))
        optional = 0;
      break;
    case 't':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sc", prec);
        snprintf(buf, buflen, fmt, trust_flags[uid->trust & 0x03]);
      }
      else if (!(uid->trust & 0x03))
      {
        /* undefined trust */
        optional = 0;
      }
      break;
    default:
      *buf = '\0';
  }

  if (optional)
    mutt_expando_format(buf, buflen, col, cols, if_str, attach_format_str, data, 0);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(buf, buflen, col, cols, else_str, attach_format_str, data, 0);
  return src;
}

/**
 * pgp_entry - Format a menu item for the pgp key list
 * @param[out] buf    Buffer in which to save string
 * @param[in]  buflen Buffer length
 * @param[in]  menu   Menu containing aliases
 * @param[in]  num    Index into the menu
 */
static void pgp_entry(char *buf, size_t buflen, struct Menu *menu, int num)
{
  struct PgpUid **KeyTable = (struct PgpUid **) menu->data;
  struct PgpEntry entry;

  entry.uid = KeyTable[num];
  entry.num = num + 1;

  mutt_expando_format(buf, buflen, 0, MuttIndexWindow->cols, NONULL(PgpEntryFormat),
                      pgp_entry_fmt, (unsigned long) &entry, MUTT_FORMAT_ARROWCURSOR);
}

static int compare_key_address(const void *a, const void *b)
{
  int r;

  struct PgpUid **s = (struct PgpUid **) a;
  struct PgpUid **t = (struct PgpUid **) b;

  r = mutt_str_strcasecmp((*s)->addr, (*t)->addr);
  if (r != 0)
    return r > 0;
  else
    return (mutt_str_strcasecmp(pgp_fpr_or_lkeyid((*s)->parent),
                                pgp_fpr_or_lkeyid((*t)->parent)) > 0);
}

static int pgp_compare_address(const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !compare_key_address(a, b) :
                                         compare_key_address(a, b));
}

static int compare_keyid(const void *a, const void *b)
{
  int r;

  struct PgpUid **s = (struct PgpUid **) a;
  struct PgpUid **t = (struct PgpUid **) b;

  r = mutt_str_strcasecmp(pgp_fpr_or_lkeyid((*s)->parent), pgp_fpr_or_lkeyid((*t)->parent));
  if (r != 0)
    return r > 0;
  else
    return (mutt_str_strcasecmp((*s)->addr, (*t)->addr) > 0);
}

static int pgp_compare_keyid(const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !compare_keyid(a, b) : compare_keyid(a, b));
}

static int compare_key_date(const void *a, const void *b)
{
  int r;
  struct PgpUid **s = (struct PgpUid **) a;
  struct PgpUid **t = (struct PgpUid **) b;

  r = ((*s)->parent->gen_time - (*t)->parent->gen_time);
  if (r != 0)
    return (r > 0);
  return (mutt_str_strcasecmp((*s)->addr, (*t)->addr) > 0);
}

static int pgp_compare_date(const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !compare_key_date(a, b) : compare_key_date(a, b));
}

static int compare_key_trust(const void *a, const void *b)
{
  int r;

  struct PgpUid **s = (struct PgpUid **) a;
  struct PgpUid **t = (struct PgpUid **) b;

  r = (((*s)->parent->flags & (KEYFLAG_RESTRICTIONS)) -
       ((*t)->parent->flags & (KEYFLAG_RESTRICTIONS)));
  if (r != 0)
    return (r > 0);
  r = ((*s)->trust - (*t)->trust);
  if (r != 0)
    return (r < 0);
  r = ((*s)->parent->keylen - (*t)->parent->keylen);
  if (r != 0)
    return (r < 0);
  r = ((*s)->parent->gen_time - (*t)->parent->gen_time);
  if (r != 0)
    return (r < 0);
  r = mutt_str_strcasecmp((*s)->addr, (*t)->addr);
  if (r != 0)
    return (r > 0);
  return (mutt_str_strcasecmp(pgp_fpr_or_lkeyid((*s)->parent),
                              pgp_fpr_or_lkeyid((*t)->parent)) > 0);
}

static int pgp_compare_trust(const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !compare_key_trust(a, b) :
                                         compare_key_trust(a, b));
}

static bool pgp_key_is_valid(struct PgpKeyInfo *k)
{
  struct PgpKeyInfo *pk = pgp_principal_key(k);
  if (k->flags & KEYFLAG_CANTUSE)
    return false;
  if (pk->flags & KEYFLAG_CANTUSE)
    return false;

  return true;
}

static bool pgp_id_is_strong(struct PgpUid *uid)
{
  if ((uid->trust & 3) < 3)
    return false;
  /* else */
  return true;
}

static bool pgp_id_is_valid(struct PgpUid *uid)
{
  if (!pgp_key_is_valid(uid->parent))
    return false;
  if (uid->flags & KEYFLAG_CANTUSE)
    return false;
  /* else */
  return true;
}

#define PGP_KV_VALID 1
#define PGP_KV_ADDR 2
#define PGP_KV_STRING 4
#define PGP_KV_STRONGID 8

#define PGP_KV_MATCH (PGP_KV_ADDR | PGP_KV_STRING)

static int pgp_id_matches_addr(struct Address *addr, struct Address *u_addr, struct PgpUid *uid)
{
  int rc = 0;

  if (pgp_id_is_valid(uid))
    rc |= PGP_KV_VALID;

  if (pgp_id_is_strong(uid))
    rc |= PGP_KV_STRONGID;

  if (addr->mailbox && u_addr->mailbox &&
      (mutt_str_strcasecmp(addr->mailbox, u_addr->mailbox) == 0))
  {
    rc |= PGP_KV_ADDR;
  }

  if (addr->personal && u_addr->personal &&
      (mutt_str_strcasecmp(addr->personal, u_addr->personal) == 0))
  {
    rc |= PGP_KV_STRING;
  }

  return rc;
}

static struct PgpKeyInfo *pgp_select_key(struct PgpKeyInfo *keys,
                                         struct Address *p, const char *s)
{
  int keymax;
  struct PgpUid **KeyTable = NULL;
  struct Menu *menu = NULL;
  int i;
  bool done = false;
  char helpstr[LONG_STRING], buf[LONG_STRING], tmpbuf[STRING];
  char cmd[LONG_STRING], tempfile[_POSIX_PATH_MAX];
  FILE *fp = NULL, *devnull = NULL;
  pid_t thepid;
  struct PgpKeyInfo *kp = NULL;
  struct PgpUid *a = NULL;
  int (*f)(const void *, const void *);

  bool unusable = false;

  keymax = 0;

  for (i = 0, kp = keys; kp; kp = kp->next)
  {
    if (!PgpShowUnusable && (kp->flags & KEYFLAG_CANTUSE))
    {
      unusable = true;
      continue;
    }

    for (a = kp->address; a; a = a->next)
    {
      if (!PgpShowUnusable && (a->flags & KEYFLAG_CANTUSE))
      {
        unusable = true;
        continue;
      }

      if (i == keymax)
      {
        keymax += 5;
        mutt_mem_realloc(&KeyTable, sizeof(struct PgpUid *) * keymax);
      }

      KeyTable[i++] = a;
    }
  }

  if (!i && unusable)
  {
    mutt_error(_("All matching keys are expired, revoked, or disabled."));
    return NULL;
  }

  switch (PgpSortKeys & SORT_MASK)
  {
    case SORT_DATE:
      f = pgp_compare_date;
      break;
    case SORT_KEYID:
      f = pgp_compare_keyid;
      break;
    case SORT_ADDRESS:
      f = pgp_compare_address;
      break;
    case SORT_TRUST:
    default:
      f = pgp_compare_trust;
      break;
  }
  qsort(KeyTable, i, sizeof(struct PgpUid *), f);

  helpstr[0] = 0;
  mutt_make_help(buf, sizeof(buf), _("Exit  "), MENU_PGP, OP_EXIT);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Select  "), MENU_PGP, OP_GENERIC_SELECT_ENTRY);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Check key  "), MENU_PGP, OP_VERIFY_KEY);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Help"), MENU_PGP, OP_HELP);
  strcat(helpstr, buf);

  menu = mutt_new_menu(MENU_PGP);
  menu->max = i;
  menu->make_entry = pgp_entry;
  menu->help = helpstr;
  menu->data = KeyTable;
  mutt_push_current_menu(menu);

  if (p)
    snprintf(buf, sizeof(buf), _("PGP keys matching <%s>."), p->mailbox);
  else
    snprintf(buf, sizeof(buf), _("PGP keys matching \"%s\"."), s);

  menu->title = buf;

  kp = NULL;

  mutt_clear_error();

  while (!done)
  {
    switch (mutt_menu_loop(menu))
    {
      case OP_VERIFY_KEY:

        mutt_mktemp(tempfile, sizeof(tempfile));
        devnull = fopen("/dev/null", "w");
        if (!devnull)
        {
          mutt_perror(_("Can't open /dev/null"));
          break;
        }
        fp = mutt_file_fopen(tempfile, "w");
        if (!fp)
        {
          mutt_file_fclose(&devnull);
          mutt_perror(_("Can't create temporary file"));
          break;
        }

        mutt_message(_("Invoking PGP..."));

        snprintf(tmpbuf, sizeof(tmpbuf), "0x%s",
                 pgp_fpr_or_lkeyid(pgp_principal_key(KeyTable[menu->current]->parent)));

        thepid = pgp_invoke_verify_key(NULL, NULL, NULL, -1, fileno(fp),
                                       fileno(devnull), tmpbuf);
        if (thepid == -1)
        {
          mutt_perror(_("Can't create filter"));
          unlink(tempfile);
          mutt_file_fclose(&fp);
          mutt_file_fclose(&devnull);
        }

        mutt_wait_filter(thepid);
        mutt_file_fclose(&fp);
        mutt_file_fclose(&devnull);
        mutt_clear_error();
        snprintf(cmd, sizeof(cmd), _("Key ID: 0x%s"),
                 pgp_keyid(pgp_principal_key(KeyTable[menu->current]->parent)));
        mutt_do_pager(cmd, tempfile, 0, NULL);
        menu->redraw = REDRAW_FULL;

        break;

      case OP_VIEW_ID:

        mutt_message("%s", NONULL(KeyTable[menu->current]->addr));
        break;

      case OP_GENERIC_SELECT_ENTRY:

        /* XXX make error reporting more verbose */

        if (OPT_PGP_CHECK_TRUST)
        {
          if (!pgp_key_is_valid(KeyTable[menu->current]->parent))
          {
            mutt_error(_("This key can't be used: expired/disabled/revoked."));
            break;
          }
        }

        if (OPT_PGP_CHECK_TRUST && (!pgp_id_is_valid(KeyTable[menu->current]) ||
                                    !pgp_id_is_strong(KeyTable[menu->current])))
        {
          char *str = "";
          char buf2[LONG_STRING];

          if (KeyTable[menu->current]->flags & KEYFLAG_CANTUSE)
            str = N_("ID is expired/disabled/revoked.");
          else
            switch (KeyTable[menu->current]->trust & 0x03)
            {
              case 0:
                str = N_("ID has undefined validity.");
                break;
              case 1:
                str = N_("ID is not valid.");
                break;
              case 2:
                str = N_("ID is only marginally valid.");
                break;
            }

          snprintf(buf2, sizeof(buf2), _("%s Do you really want to use the key?"), _(str));

          if (mutt_yesorno(buf2, MUTT_NO) != MUTT_YES)
          {
            mutt_clear_error();
            break;
          }
        }

        kp = KeyTable[menu->current]->parent;
        done = true;
        break;

      case OP_EXIT:

        kp = NULL;
        done = true;
        break;
    }
  }

  mutt_pop_current_menu(menu);
  mutt_menu_destroy(&menu);
  FREE(&KeyTable);

  return kp;
}

struct PgpKeyInfo *pgp_ask_for_key(char *tag, char *whatfor, short abilities, enum PgpRing keyring)
{
  struct PgpKeyInfo *key = NULL;
  char resp[SHORT_STRING];
  struct PgpCache *l = NULL;

  mutt_clear_error();

  resp[0] = 0;
  if (whatfor)
  {
    for (l = id_defaults; l; l = l->next)
    {
      if (mutt_str_strcasecmp(whatfor, l->what) == 0)
      {
        mutt_str_strfcpy(resp, NONULL(l->dflt), sizeof(resp));
        break;
      }
    }
  }

  while (true)
  {
    resp[0] = 0;
    if (mutt_get_field(tag, resp, sizeof(resp), MUTT_CLEAR) != 0)
      return NULL;

    if (whatfor)
    {
      if (l)
        mutt_str_replace(&l->dflt, resp);
      else
      {
        l = mutt_mem_malloc(sizeof(struct PgpCache));
        l->next = id_defaults;
        id_defaults = l;
        l->what = mutt_str_strdup(whatfor);
        l->dflt = mutt_str_strdup(resp);
      }
    }

    key = pgp_getkeybystr(resp, abilities, keyring);
    if (key)
      return key;

    mutt_error(_("No matching keys found for \"%s\""), resp);
  }
  /* not reached */
}

/**
 * pgp_make_key_attachment - generate a public key attachment
 */
struct Body *pgp_make_key_attachment(char *tempf)
{
  struct Body *att = NULL;
  char buf[LONG_STRING];
  char tempfb[_POSIX_PATH_MAX], tmp[STRING];
  FILE *tempfp = NULL;
  FILE *devnull = NULL;
  struct stat sb;
  pid_t thepid;
  struct PgpKeyInfo *key = NULL;
  OPT_PGP_CHECK_TRUST = false;

  key = pgp_ask_for_key(_("Please enter the key ID: "), NULL, 0, PGP_PUBRING);

  if (!key)
    return NULL;

  snprintf(tmp, sizeof(tmp), "0x%s", pgp_fpr_or_lkeyid(pgp_principal_key(key)));
  pgp_free_key(&key);

  if (!tempf)
  {
    mutt_mktemp(tempfb, sizeof(tempfb));
    tempf = tempfb;
  }

  tempfp = mutt_file_fopen(tempf, tempf == tempfb ? "w" : "a");
  if (!tempfp)
  {
    mutt_perror(_("Can't create temporary file"));
    return NULL;
  }

  devnull = fopen("/dev/null", "w");
  if (!devnull)
  {
    mutt_perror(_("Can't open /dev/null"));
    mutt_file_fclose(&tempfp);
    if (tempf == tempfb)
      unlink(tempf);
    return NULL;
  }

  mutt_message(_("Invoking PGP..."));

  thepid = pgp_invoke_export(NULL, NULL, NULL, -1, fileno(tempfp), fileno(devnull), tmp);
  if (thepid == -1)
  {
    mutt_perror(_("Can't create filter"));
    unlink(tempf);
    mutt_file_fclose(&tempfp);
    mutt_file_fclose(&devnull);
    return NULL;
  }

  mutt_wait_filter(thepid);

  mutt_file_fclose(&tempfp);
  mutt_file_fclose(&devnull);

  att = mutt_new_body();
  att->filename = mutt_str_strdup(tempf);
  att->unlink = true;
  att->use_disp = false;
  att->type = TYPEAPPLICATION;
  att->subtype = mutt_str_strdup("pgp-keys");
  snprintf(buf, sizeof(buf), _("PGP Key %s."), tmp);
  att->description = mutt_str_strdup(buf);
  mutt_update_encoding(att);

  stat(tempf, &sb);
  att->length = sb.st_size;

  return att;
}

static void pgp_add_string_to_hints(struct ListHead *hints, const char *str)
{
  char *scratch = NULL;
  char *t = NULL;

  scratch = mutt_str_strdup(str);
  if (!scratch)
    return;

  for (t = strtok(scratch, " ,.:\"()<>\n"); t; t = strtok(NULL, " ,.:\"()<>\n"))
  {
    if (strlen(t) > 3)
      mutt_list_insert_tail(hints, mutt_str_strdup(t));
  }

  FREE(&scratch);
}

static struct PgpKeyInfo **pgp_get_lastp(struct PgpKeyInfo *p)
{
  for (; p; p = p->next)
    if (!p->next)
      return &p->next;

  return NULL;
}

struct PgpKeyInfo *pgp_getkeybyaddr(struct Address *a, short abilities,
                                    enum PgpRing keyring, int oppenc_mode)
{
  struct Address *r = NULL, *p = NULL;
  struct ListHead hints = STAILQ_HEAD_INITIALIZER(hints);

  bool multi = false;

  struct PgpKeyInfo *keys = NULL, *k = NULL, *kn = NULL;
  struct PgpKeyInfo *the_strong_valid_key = NULL;
  struct PgpKeyInfo *a_valid_addrmatch_key = NULL;
  struct PgpKeyInfo *matches = NULL;
  struct PgpKeyInfo **last = &matches;
  struct PgpUid *q = NULL;

  if (a && a->mailbox)
    pgp_add_string_to_hints(&hints, a->mailbox);
  if (a && a->personal)
    pgp_add_string_to_hints(&hints, a->personal);

  if (!oppenc_mode)
    mutt_message(_("Looking for keys matching \"%s\"..."), a ? a->mailbox : "");
  keys = pgp_get_candidates(keyring, &hints);

  mutt_list_free(&hints);

  if (!keys)
    return NULL;

  mutt_debug(5, "looking for %s <%s>.\n", a->personal, a->mailbox);

  for (k = keys; k; k = kn)
  {
    kn = k->next;

    mutt_debug(5, "  looking at key: %s\n", pgp_keyid(k));

    if (abilities && !(k->flags & abilities))
    {
      mutt_debug(5, "  insufficient abilities: Has %x, want %x\n", k->flags, abilities);
      continue;
    }

    bool match = false; /* any match */

    for (q = k->address; q; q = q->next)
    {
      r = mutt_addr_parse_list(NULL, NONULL(q->addr));

      for (p = r; p; p = p->next)
      {
        int validity = pgp_id_matches_addr(a, p, q);

        if (validity & PGP_KV_MATCH) /* something matches */
          match = true;

        if ((validity & PGP_KV_VALID) && (validity & PGP_KV_ADDR))
        {
          if (validity & PGP_KV_STRONGID)
          {
            if (the_strong_valid_key && the_strong_valid_key != k)
              multi = true;
            the_strong_valid_key = k;
          }
          else
          {
            a_valid_addrmatch_key = k;
          }
        }
      }

      mutt_addr_free(&r);
    }

    if (match)
    {
      *last = pgp_principal_key(k);
      kn = pgp_remove_key(&keys, *last);
      last = pgp_get_lastp(k);
    }
  }

  pgp_free_key(&keys);

  if (matches)
  {
    if (oppenc_mode)
    {
      if (the_strong_valid_key)
      {
        pgp_remove_key(&matches, the_strong_valid_key);
        k = the_strong_valid_key;
      }
      else if (a_valid_addrmatch_key)
      {
        pgp_remove_key(&matches, a_valid_addrmatch_key);
        k = a_valid_addrmatch_key;
      }
      else
        k = NULL;
    }
    else if (the_strong_valid_key && !multi)
    {
      /*
       * There was precisely one strong match on a valid ID.
       *
       * Proceed without asking the user.
       */
      pgp_remove_key(&matches, the_strong_valid_key);
      k = the_strong_valid_key;
    }
    else
    {
      /*
       * Else: Ask the user.
       */
      k = pgp_select_key(matches, a, NULL);
      if (k)
        pgp_remove_key(&matches, k);
    }

    pgp_free_key(&matches);

    return k;
  }

  return NULL;
}

struct PgpKeyInfo *pgp_getkeybystr(char *p, short abilities, enum PgpRing keyring)
{
  struct ListHead hints = STAILQ_HEAD_INITIALIZER(hints);
  struct PgpKeyInfo *keys = NULL;
  struct PgpKeyInfo *matches = NULL;
  struct PgpKeyInfo **last = &matches;
  struct PgpKeyInfo *k = NULL, *kn = NULL;
  struct PgpUid *a = NULL;
  size_t l;
  const char *ps = NULL, *pl = NULL, *pfcopy = NULL, *phint = NULL;

  l = mutt_str_strlen(p);
  if ((l > 0) && (p[l - 1] == '!'))
    p[l - 1] = 0;

  mutt_message(_("Looking for keys matching \"%s\"..."), p);

  pfcopy = crypt_get_fingerprint_or_id(p, &phint, &pl, &ps);
  pgp_add_string_to_hints(&hints, phint);
  keys = pgp_get_candidates(keyring, &hints);
  mutt_list_free(&hints);

  if (!keys)
    goto out;

  for (k = keys; k; k = kn)
  {
    kn = k->next;
    if (abilities && !(k->flags & abilities))
      continue;

    /* This shouldn't happen, but keys without any addresses aren't selectable
     * in pgp_select_key().
     */
    if (!k->address)
      continue;

    bool match = false;

    mutt_debug(5, "matching \"%s\" against key %s:\n", p, pgp_long_keyid(k));

    if (!*p || (pfcopy && (mutt_str_strcasecmp(pfcopy, k->fingerprint) == 0)) ||
        (pl && (mutt_str_strcasecmp(pl, pgp_long_keyid(k)) == 0)) ||
        (ps && (mutt_str_strcasecmp(ps, pgp_short_keyid(k)) == 0)))
    {
      mutt_debug(5, "\t\tmatch #1\n");
      match = true;
    }
    else
    {
      for (a = k->address; a; a = a->next)
      {
        mutt_debug(5, "matching \"%s\" against key %s, \"%s\":\n", p,
                   pgp_long_keyid(k), NONULL(a->addr));
        if (mutt_str_stristr(a->addr, p))
        {
          mutt_debug(5, "\t\tmatch #2\n");
          match = true;
          break;
        }
      }
    }

    if (match)
    {
      *last = pgp_principal_key(k);
      kn = pgp_remove_key(&keys, *last);
      last = pgp_get_lastp(k);
    }
  }

  pgp_free_key(&keys);

  if (matches)
  {
    k = pgp_select_key(matches, NULL, p);
    if (k)
      pgp_remove_key(&matches, k);

    pgp_free_key(&matches);
    FREE(&pfcopy);
    if (l && !p[l - 1])
      p[l - 1] = '!';
    return k;
  }

out:
  FREE(&pfcopy);
  if (l && !p[l - 1])
    p[l - 1] = '!';
  return NULL;
}
