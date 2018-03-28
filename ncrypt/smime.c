/**
 * @file
 * SMIME helper routines
 *
 * @authors
 * Copyright (C) 2001-2002 Oliver Ehli <elmy@acm.org>
 * Copyright (C) 2002 Mike Schiraldi <raldi@research.netsol.com>
 * Copyright (C) 2004 g10 Code GmbH
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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "smime.h"
#include "alias.h"
#include "body.h"
#include "copy.h"
#include "crypt.h"
#include "cryptglue.h"
#include "envelope.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "header.h"
#include "keymap.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "state.h"

/**
 * struct SmimeCommandContext - Data for a SIME command
 */
struct SmimeCommandContext
{
  const char *key;           /**< %k */
  const char *cryptalg;      /**< %a */
  const char *digestalg;     /**< %d */
  const char *fname;         /**< %f */
  const char *sig_fname;     /**< %s */
  const char *certificates;  /**< %c */
  const char *intermediates; /**< %i */
};

char SmimePass[STRING];
time_t SmimeExptime = 0; /* when does the cached passphrase expire? */

static char SmimeKeyToUse[_POSIX_PATH_MAX] = { 0 };
static char SmimeCertToUse[_POSIX_PATH_MAX];
static char SmimeIntermediateToUse[_POSIX_PATH_MAX];

static void smime_free_key(struct SmimeKey **keylist)
{
  struct SmimeKey *key = NULL;

  if (!keylist)
    return;

  while (*keylist)
  {
    key = *keylist;
    *keylist = (*keylist)->next;

    FREE(&key->email);
    FREE(&key->hash);
    FREE(&key->label);
    FREE(&key->issuer);
    FREE(&key);
  }
}

static struct SmimeKey *smime_copy_key(struct SmimeKey *key)
{
  struct SmimeKey *copy = NULL;

  if (!key)
    return NULL;

  copy = mutt_mem_calloc(1, sizeof(struct SmimeKey));
  copy->email = mutt_str_strdup(key->email);
  copy->hash = mutt_str_strdup(key->hash);
  copy->label = mutt_str_strdup(key->label);
  copy->issuer = mutt_str_strdup(key->issuer);
  copy->trust = key->trust;
  copy->flags = key->flags;

  return copy;
}

/*
 *     Queries and passphrase handling.
 */

/* these are copies from pgp.c */

void smime_void_passphrase(void)
{
  memset(SmimePass, 0, sizeof(SmimePass));
  SmimeExptime = 0;
}

int smime_valid_passphrase(void)
{
  time_t now = time(NULL);

  if (now < SmimeExptime)
  {
    /* Use cached copy.  */
    return 1;
  }

  smime_void_passphrase();

  if (mutt_get_password(_("Enter S/MIME passphrase:"), SmimePass, sizeof(SmimePass)) == 0)
  {
    SmimeExptime = time(NULL) + SmimeTimeout;
    return 1;
  }
  else
    SmimeExptime = 0;

  return 0;
}

/*
 *     The OpenSSL interface
 */

/**
 * fmt_smime_command - Format an SMIME command
 *
 * This is almost identical to pgp's invoking interface.
 */
static const char *fmt_smime_command(char *buf, size_t buflen, size_t col, int cols,
                                     char op, const char *src, const char *prec,
                                     const char *if_str, const char *else_str,
                                     unsigned long data, enum FormatFlag flags)
{
  char fmt[SHORT_STRING];
  struct SmimeCommandContext *cctx = (struct SmimeCommandContext *) data;
  int optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'C':
    {
      if (!optional)
      {
        char path[_POSIX_PATH_MAX];
        char buf1[LONG_STRING], buf2[LONG_STRING];
        struct stat sb;

        mutt_str_strfcpy(path, NONULL(SmimeCALocation), sizeof(path));
        mutt_expand_path(path, sizeof(path));
        mutt_file_quote_filename(buf1, sizeof(buf1), path);

        if (stat(path, &sb) != 0 || !S_ISDIR(sb.st_mode))
          snprintf(buf2, sizeof(buf2), "-CAfile %s", buf1);
        else
          snprintf(buf2, sizeof(buf2), "-CApath %s", buf1);

        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, buf2);
      }
      else if (!SmimeCALocation)
        optional = 0;
      break;
    }

    case 'c':
    { /* certificate (list) */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->certificates));
      }
      else if (!cctx->certificates)
        optional = 0;
      break;
    }

    case 'i':
    { /* intermediate certificates  */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->intermediates));
      }
      else if (!cctx->intermediates)
        optional = 0;
      break;
    }

    case 's':
    { /* detached signature */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->sig_fname));
      }
      else if (!cctx->sig_fname)
        optional = 0;
      break;
    }

    case 'k':
    { /* private key */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->key));
      }
      else if (!cctx->key)
        optional = 0;
      break;
    }

    case 'a':
    { /* algorithm for encryption */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->cryptalg));
      }
      else if (!cctx->key)
        optional = 0;
      break;
    }

    case 'f':
    { /* file to process */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->fname));
      }
      else if (!cctx->fname)
        optional = 0;
      break;
    }

    case 'd':
    { /* algorithm for the signature message digest */
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, NONULL(cctx->digestalg));
      }
      else if (!cctx->key)
        optional = 0;
      break;
    }

    default:
      *buf = '\0';
      break;
  }

  if (optional)
    mutt_expando_format(buf, buflen, col, cols, if_str, fmt_smime_command, data, 0);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(buf, buflen, col, cols, else_str, fmt_smime_command, data, 0);

  return src;
}

static void smime_command(char *buf, size_t buflen,
                          struct SmimeCommandContext *cctx, const char *fmt)
{
  mutt_expando_format(buf, buflen, 0, MuttIndexWindow->cols, NONULL(fmt),
                      fmt_smime_command, (unsigned long) cctx, 0);
  mutt_debug(2, "%s\n", buf);
}

static pid_t smime_invoke(FILE **smimein, FILE **smimeout, FILE **smimeerr,
                          int smimeinfd, int smimeoutfd, int smimeerrfd,
                          const char *fname, const char *sig_fname, const char *cryptalg,
                          const char *digestalg, const char *key, const char *certificates,
                          const char *intermediates, const char *format)
{
  struct SmimeCommandContext cctx;
  char cmd[HUGE_STRING];

  memset(&cctx, 0, sizeof(cctx));

  if (!format || !*format)
    return (pid_t) -1;

  cctx.fname = fname;
  cctx.sig_fname = sig_fname;
  cctx.key = key;
  cctx.cryptalg = cryptalg;
  cctx.digestalg = digestalg;
  cctx.certificates = certificates;
  cctx.intermediates = intermediates;

  smime_command(cmd, sizeof(cmd), &cctx, format);

  return mutt_create_filter_fd(cmd, smimein, smimeout, smimeerr, smimeinfd,
                               smimeoutfd, smimeerrfd);
}

/*
 *    Key and certificate handling.
 */

static char *smime_key_flags(int flags)
{
  static char buf[3];

  if (!(flags & KEYFLAG_CANENCRYPT))
    buf[0] = '-';
  else
    buf[0] = 'e';

  if (!(flags & KEYFLAG_CANSIGN))
    buf[1] = '-';
  else
    buf[1] = 's';

  buf[2] = '\0';

  return buf;
}

/**
 * smime_entry - Format a menu item for the smime key list
 * @param[out] buf    Buffer in which to save string
 * @param[in]  buflen Buffer length
 * @param[in]  menu   Menu containing aliases
 * @param[in]  num    Index into the menu
 */
static void smime_entry(char *buf, size_t buflen, struct Menu *menu, int num)
{
  struct SmimeKey **Table = (struct SmimeKey **) menu->data;
  struct SmimeKey *this = Table[num];
  char *truststate = NULL;
  switch (this->trust)
  {
    case 'e':
      truststate = N_("Expired   ");
      break;
    case 'i':
      truststate = N_("Invalid   ");
      break;
    case 'r':
      truststate = N_("Revoked   ");
      break;
    case 't':
      truststate = N_("Trusted   ");
      break;
    case 'u':
      truststate = N_("Unverified");
      break;
    case 'v':
      truststate = N_("Verified  ");
      break;
    default:
      truststate = N_("Unknown   ");
  }
  snprintf(buf, buflen, " 0x%s %s %s %-35.35s %s", this->hash,
           smime_key_flags(this->flags), truststate, this->email, this->label);
}

static struct SmimeKey *smime_select_key(struct SmimeKey *keys, char *query)
{
  struct SmimeKey **table = NULL;
  int table_size = 0;
  int table_index = 0;
  struct SmimeKey *key = NULL;
  struct SmimeKey *selected_key = NULL;
  char helpstr[LONG_STRING];
  char buf[LONG_STRING];
  char title[256];
  struct Menu *menu = NULL;
  char *s = "";
  bool done = false;

  for (table_index = 0, key = keys; key; key = key->next)
  {
    if (table_index == table_size)
    {
      table_size += 5;
      mutt_mem_realloc(&table, sizeof(struct SmimeKey *) * table_size);
    }

    table[table_index++] = key;
  }

  snprintf(title, sizeof(title), _("S/MIME certificates matching \"%s\"."), query);

  /* Make Helpstring */
  helpstr[0] = 0;
  mutt_make_help(buf, sizeof(buf), _("Exit  "), MENU_SMIME, OP_EXIT);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Select  "), MENU_SMIME, OP_GENERIC_SELECT_ENTRY);
  strcat(helpstr, buf);
  mutt_make_help(buf, sizeof(buf), _("Help"), MENU_SMIME, OP_HELP);
  strcat(helpstr, buf);

  /* Create the menu */
  menu = mutt_menu_new(MENU_SMIME);
  menu->max = table_index;
  menu->make_entry = smime_entry;
  menu->help = helpstr;
  menu->data = table;
  menu->title = title;
  mutt_menu_push_current(menu);
  /* sorting keys might be done later - TODO */

  mutt_clear_error();

  done = false;
  while (!done)
  {
    switch (mutt_menu_loop(menu))
    {
      case OP_GENERIC_SELECT_ENTRY:
        if (table[menu->current]->trust != 't')
        {
          switch (table[menu->current]->trust)
          {
            case 'e':
            case 'i':
            case 'r':
              s = N_("ID is expired/disabled/revoked.");
              break;
            case 'u':
              s = N_("ID has undefined validity.");
              break;
            case 'v':
              s = N_("ID is not trusted.");
              break;
          }

          snprintf(buf, sizeof(buf), _("%s Do you really want to use the key?"), _(s));

          if (mutt_yesorno(buf, MUTT_NO) != MUTT_YES)
          {
            mutt_clear_error();
            break;
          }
        }

        selected_key = table[menu->current];
        done = true;
        break;
      case OP_EXIT:
        done = true;
        break;
    }
  }

  mutt_menu_pop_current(menu);
  mutt_menu_destroy(&menu);
  FREE(&table);

  return selected_key;
}

static struct SmimeKey *smime_parse_key(char *buf)
{
  char *pend = NULL, *p = NULL;
  int field = 0;

  struct SmimeKey *key = mutt_mem_calloc(1, sizeof(struct SmimeKey));

  for (p = buf; p; p = pend)
  {
    /* Some users manually maintain their .index file, and use a tab
     * as a delimiter, which the old parsing code (using fscanf)
     * happened to allow.  smime_keys uses a space, so search for both.
     */
    if ((pend = strchr(p, ' ')) || (pend = strchr(p, '\t')) || (pend = strchr(p, '\n')))
      *pend++ = 0;

    /* For backward compatibility, don't count consecutive delimiters
     * as an empty field.
     */
    if (!*p)
      continue;

    field++;

    switch (field)
    {
      case 1: /* mailbox */
        key->email = mutt_str_strdup(p);
        break;
      case 2: /* hash */
        key->hash = mutt_str_strdup(p);
        break;
      case 3: /* label */
        key->label = mutt_str_strdup(p);
        break;
      case 4: /* issuer */
        key->issuer = mutt_str_strdup(p);
        break;
      case 5: /* trust */
        key->trust = *p;
        break;
      case 6: /* purpose */
        while (*p)
        {
          switch (*p++)
          {
            case 'e':
              key->flags |= KEYFLAG_CANENCRYPT;
              break;

            case 's':
              key->flags |= KEYFLAG_CANSIGN;
              break;
          }
        }
        break;
    }
  }

  /* Old index files could be missing issuer, trust, and purpose,
   * but anything less than that is an error. */
  if (field < 3)
  {
    smime_free_key(&key);
    return NULL;
  }

  if (field < 4)
    key->issuer = mutt_str_strdup("?");

  if (field < 5)
    key->trust = 't';

  if (field < 6)
    key->flags = (KEYFLAG_CANENCRYPT | KEYFLAG_CANSIGN);

  return key;
}

static struct SmimeKey *smime_get_candidates(char *search, short public)
{
  char index_file[_POSIX_PATH_MAX];
  char buf[LONG_STRING];
  struct SmimeKey *key = NULL, *results = NULL;
  struct SmimeKey **results_end = &results;

  snprintf(index_file, sizeof(index_file), "%s/.index",
           public ? NONULL(SmimeCertificates) : NONULL(SmimeKeys));

  FILE *fp = mutt_file_fopen(index_file, "r");
  if (!fp)
  {
    mutt_perror(index_file);
    return NULL;
  }

  while (fgets(buf, sizeof(buf), fp))
  {
    if ((!*search) || mutt_str_stristr(buf, search))
    {
      key = smime_parse_key(buf);
      if (key)
      {
        *results_end = key;
        results_end = &key->next;
      }
    }
  }

  mutt_file_fclose(&fp);

  return results;
}

/**
 * smime_get_key_by_hash - Find a key by its hash
 *
 * Returns the first matching key record, without prompting or checking of
 * abilities or trust.
 */
static struct SmimeKey *smime_get_key_by_hash(char *hash, short public)
{
  struct SmimeKey *match = NULL;
  struct SmimeKey *results = smime_get_candidates(hash, public);
  for (struct SmimeKey *result = results; result; result = result->next)
  {
    if (mutt_str_strcasecmp(hash, result->hash) == 0)
    {
      match = smime_copy_key(result);
      break;
    }
  }

  smime_free_key(&results);

  return match;
}

static struct SmimeKey *smime_get_key_by_addr(char *mailbox, short abilities,
                                              short public, short may_ask)
{
  struct SmimeKey *results = NULL, *result = NULL;
  struct SmimeKey *matches = NULL;
  struct SmimeKey **matches_end = &matches;
  struct SmimeKey *match = NULL;
  struct SmimeKey *trusted_match = NULL;
  struct SmimeKey *valid_match = NULL;
  struct SmimeKey *return_key = NULL;
  int multi_trusted_matches = 0;

  if (!mailbox)
    return NULL;

  results = smime_get_candidates(mailbox, public);
  for (result = results; result; result = result->next)
  {
    if (abilities && !(result->flags & abilities))
    {
      continue;
    }

    if (mutt_str_strcasecmp(mailbox, result->email) == 0)
    {
      match = smime_copy_key(result);
      *matches_end = match;
      matches_end = &match->next;

      if (match->trust == 't')
      {
        if (trusted_match && (mutt_str_strcasecmp(match->hash, trusted_match->hash) != 0))
        {
          multi_trusted_matches = 1;
        }
        trusted_match = match;
      }
      else if ((match->trust == 'u') || (match->trust == 'v'))
      {
        valid_match = match;
      }
    }
  }

  smime_free_key(&results);

  if (matches)
  {
    if (!may_ask)
    {
      if (trusted_match)
        return_key = smime_copy_key(trusted_match);
      else if (valid_match)
        return_key = smime_copy_key(valid_match);
      else
        return_key = NULL;
    }
    else if (trusted_match && !multi_trusted_matches)
    {
      return_key = smime_copy_key(trusted_match);
    }
    else
    {
      return_key = smime_copy_key(smime_select_key(matches, mailbox));
    }

    smime_free_key(&matches);
  }

  return return_key;
}

static struct SmimeKey *smime_get_key_by_str(char *str, short abilities, short public)
{
  struct SmimeKey *results = NULL, *result = NULL;
  struct SmimeKey *matches = NULL;
  struct SmimeKey **matches_end = &matches;
  struct SmimeKey *match = NULL;
  struct SmimeKey *return_key = NULL;

  if (!str)
    return NULL;

  results = smime_get_candidates(str, public);
  for (result = results; result; result = result->next)
  {
    if (abilities && !(result->flags & abilities))
    {
      continue;
    }

    if ((mutt_str_strcasecmp(str, result->hash) == 0) ||
        mutt_str_stristr(result->email, str) || mutt_str_stristr(result->label, str))
    {
      match = smime_copy_key(result);
      *matches_end = match;
      matches_end = &match->next;
    }
  }

  smime_free_key(&results);

  if (matches)
  {
    return_key = smime_copy_key(smime_select_key(matches, str));
    smime_free_key(&matches);
  }

  return return_key;
}

static struct SmimeKey *smime_ask_for_key(char *prompt, short abilities, short public)
{
  struct SmimeKey *key = NULL;
  char resp[SHORT_STRING];

  if (!prompt)
    prompt = _("Enter keyID: ");

  mutt_clear_error();

  while (true)
  {
    resp[0] = 0;
    if (mutt_get_field(prompt, resp, sizeof(resp), MUTT_CLEAR) != 0)
      return NULL;

    key = smime_get_key_by_str(resp, abilities, public);
    if (key)
      return key;

    mutt_error(_("No matching keys found for \"%s\""), resp);
  }
}

/**
 * getkeys - Get the keys for a mailbox
 *
 * This sets the '*ToUse' variables for an upcoming decryption, where the
 * required key is different from SmimeDefaultKey.
 */
static void getkeys(char *mailbox)
{
  char *k = NULL;

  struct SmimeKey *key = smime_get_key_by_addr(mailbox, KEYFLAG_CANENCRYPT, 0, 1);

  if (!key)
  {
    char buf[STRING];
    snprintf(buf, sizeof(buf), _("Enter keyID for %s: "), mailbox);
    key = smime_ask_for_key(buf, KEYFLAG_CANENCRYPT, 0);
  }

  if (key)
  {
    k = key->hash;

    /* the key used last time. */
    if (*SmimeKeyToUse &&
        (mutt_str_strcasecmp(k, SmimeKeyToUse + mutt_str_strlen(SmimeKeys) + 1) == 0))
    {
      smime_free_key(&key);
      return;
    }
    else
      smime_void_passphrase();

    snprintf(SmimeKeyToUse, sizeof(SmimeKeyToUse), "%s/%s", NONULL(SmimeKeys), k);

    snprintf(SmimeCertToUse, sizeof(SmimeCertToUse), "%s/%s", NONULL(SmimeCertificates), k);

    if (mutt_str_strcasecmp(k, SmimeDefaultKey) != 0)
      smime_void_passphrase();

    smime_free_key(&key);
    return;
  }

  if (*SmimeKeyToUse)
  {
    if (mutt_str_strcasecmp(SmimeDefaultKey,
                            SmimeKeyToUse + mutt_str_strlen(SmimeKeys) + 1) == 0)
    {
      return;
    }

    smime_void_passphrase();
  }

  snprintf(SmimeKeyToUse, sizeof(SmimeKeyToUse), "%s/%s", NONULL(SmimeKeys),
           NONULL(SmimeDefaultKey));

  snprintf(SmimeCertToUse, sizeof(SmimeCertToUse), "%s/%s",
           NONULL(SmimeCertificates), NONULL(SmimeDefaultKey));
}

void smime_getkeys(struct Envelope *env)
{
  struct Address *t = NULL;
  bool found = false;

  if (SmimeDecryptUseDefaultKey && SmimeDefaultKey && *SmimeDefaultKey)
  {
    snprintf(SmimeKeyToUse, sizeof(SmimeKeyToUse), "%s/%s", NONULL(SmimeKeys), SmimeDefaultKey);

    snprintf(SmimeCertToUse, sizeof(SmimeCertToUse), "%s/%s",
             NONULL(SmimeCertificates), SmimeDefaultKey);

    return;
  }

  for (t = env->to; !found && t; t = t->next)
  {
    if (mutt_addr_is_user(t))
    {
      found = true;
      getkeys(t->mailbox);
    }
  }
  for (t = env->cc; !found && t; t = t->next)
  {
    if (mutt_addr_is_user(t))
    {
      found = true;
      getkeys(t->mailbox);
    }
  }
  if (!found && (t = mutt_default_from()))
  {
    getkeys(t->mailbox);
    mutt_addr_free(&t);
  }
}

/**
 * smime_find_keys - Find the keys of the recipients of a message
 * @retval NULL if any of the keys can not be found
 *
 * If oppenc_mode is true, only keys that can be determined without
 * prompting will be used.
 */
char *smime_find_keys(struct Address *addrlist, int oppenc_mode)
{
  struct SmimeKey *key = NULL;
  char *keyID = NULL, *keylist = NULL;
  size_t keylist_size = 0;
  size_t keylist_used = 0;
  struct Address *p = NULL, *q = NULL;

  for (p = addrlist; p; p = p->next)
  {
    q = p;

    key = smime_get_key_by_addr(q->mailbox, KEYFLAG_CANENCRYPT, 1, !oppenc_mode);
    if (!key && !oppenc_mode)
    {
      char buf[LONG_STRING];
      snprintf(buf, sizeof(buf), _("Enter keyID for %s: "), q->mailbox);
      key = smime_ask_for_key(buf, KEYFLAG_CANENCRYPT, 1);
    }
    if (!key)
    {
      if (!oppenc_mode)
        mutt_message(_("No (valid) certificate found for %s."), q->mailbox);
      FREE(&keylist);
      return NULL;
    }

    keyID = key->hash;
    keylist_size += mutt_str_strlen(keyID) + 2;
    mutt_mem_realloc(&keylist, keylist_size);
    sprintf(keylist + keylist_used, "%s%s", keylist_used ? " " : "", keyID);
    keylist_used = mutt_str_strlen(keylist);

    smime_free_key(&key);
  }
  return keylist;
}

static int smime_handle_cert_email(char *certificate, char *mailbox, int copy,
                                   char ***buffer, int *num)
{
  char tmpfname[_POSIX_PATH_MAX];
  char email[STRING];
  int rc = -1, count = 0;
  pid_t thepid;
  size_t len = 0;

  mutt_mktemp(tmpfname, sizeof(tmpfname));
  FILE *fperr = mutt_file_fopen(tmpfname, "w+");
  if (!fperr)
  {
    mutt_perror(tmpfname);
    return 1;
  }
  mutt_file_unlink(tmpfname);

  mutt_mktemp(tmpfname, sizeof(tmpfname));
  FILE *fpout = mutt_file_fopen(tmpfname, "w+");
  if (!fpout)
  {
    mutt_file_fclose(&fperr);
    mutt_perror(tmpfname);
    return 1;
  }
  mutt_file_unlink(tmpfname);

  thepid = smime_invoke(NULL, NULL, NULL, -1, fileno(fpout), fileno(fperr), certificate,
                        NULL, NULL, NULL, NULL, NULL, NULL, SmimeGetCertEmailCommand);
  if (thepid == -1)
  {
    mutt_message(_("Error: unable to create OpenSSL subprocess!"));
    mutt_file_fclose(&fperr);
    mutt_file_fclose(&fpout);
    return 1;
  }

  mutt_wait_filter(thepid);

  fflush(fpout);
  rewind(fpout);
  fflush(fperr);
  rewind(fperr);

  while ((fgets(email, sizeof(email), fpout)))
  {
    len = mutt_str_strlen(email);
    if (len && (email[len - 1] == '\n'))
      email[len - 1] = '\0';
    if (mutt_str_strncasecmp(email, mailbox, mutt_str_strlen(mailbox)) == 0)
      rc = 1;

    rc = rc < 0 ? 0 : rc;
    count++;
  }

  if (rc == -1)
  {
    mutt_endwin();
    mutt_file_copy_stream(fperr, stdout);
    mutt_any_key_to_continue(_("Error: unable to create OpenSSL subprocess!"));
    rc = 1;
  }
  else if (!rc)
    rc = 1;
  else
    rc = 0;

  if (copy && buffer && num)
  {
    (*num) = count;
    *buffer = mutt_mem_calloc(count, sizeof(char *));
    count = 0;

    rewind(fpout);
    while ((fgets(email, sizeof(email), fpout)))
    {
      len = mutt_str_strlen(email);
      if (len && (email[len - 1] == '\n'))
        email[len - 1] = '\0';
      (*buffer)[count] = mutt_mem_calloc(mutt_str_strlen(email) + 1, sizeof(char));
      strncpy((*buffer)[count], email, mutt_str_strlen(email));
      count++;
    }
  }
  else if (copy)
    rc = 2;

  mutt_file_fclose(&fpout);
  mutt_file_fclose(&fperr);

  return rc;
}

static char *smime_extract_certificate(char *infile)
{
  char pk7out[_POSIX_PATH_MAX], certfile[_POSIX_PATH_MAX];
  char tmpfname[_POSIX_PATH_MAX];
  pid_t thepid;
  int empty;

  mutt_mktemp(tmpfname, sizeof(tmpfname));
  FILE *fperr = mutt_file_fopen(tmpfname, "w+");
  if (!fperr)
  {
    mutt_perror(tmpfname);
    return NULL;
  }
  mutt_file_unlink(tmpfname);

  mutt_mktemp(pk7out, sizeof(pk7out));
  FILE *fpout = mutt_file_fopen(pk7out, "w+");
  if (!fpout)
  {
    mutt_file_fclose(&fperr);
    mutt_perror(pk7out);
    return NULL;
  }

  /* Step 1: Convert the signature to a PKCS#7 structure, as we can't
     extract the full set of certificates directly.
  */
  thepid = smime_invoke(NULL, NULL, NULL, -1, fileno(fpout), fileno(fperr), infile,
                        NULL, NULL, NULL, NULL, NULL, NULL, SmimePk7outCommand);
  if (thepid == -1)
  {
    mutt_any_key_to_continue(_("Error: unable to create OpenSSL subprocess!"));
    mutt_file_fclose(&fperr);
    mutt_file_fclose(&fpout);
    mutt_file_unlink(pk7out);
    return NULL;
  }

  mutt_wait_filter(thepid);

  fflush(fpout);
  rewind(fpout);
  fflush(fperr);
  rewind(fperr);
  empty = (fgetc(fpout) == EOF);
  if (empty)
  {
    mutt_perror(pk7out);
    mutt_file_copy_stream(fperr, stdout);
    mutt_file_fclose(&fpout);
    mutt_file_fclose(&fperr);
    mutt_file_unlink(pk7out);
    return NULL;
  }

  mutt_file_fclose(&fpout);
  mutt_mktemp(certfile, sizeof(certfile));
  fpout = mutt_file_fopen(certfile, "w+");
  if (!fpout)
  {
    mutt_file_fclose(&fperr);
    mutt_file_unlink(pk7out);
    mutt_perror(certfile);
    return NULL;
  }

  /* Step 2: Extract the certificates from a PKCS#7 structure.
   */
  thepid = smime_invoke(NULL, NULL, NULL, -1, fileno(fpout), fileno(fperr), pk7out,
                        NULL, NULL, NULL, NULL, NULL, NULL, SmimeGetCertCommand);
  if (thepid == -1)
  {
    mutt_any_key_to_continue(_("Error: unable to create OpenSSL subprocess!"));
    mutt_file_fclose(&fperr);
    mutt_file_fclose(&fpout);
    mutt_file_unlink(pk7out);
    mutt_file_unlink(certfile);
    return NULL;
  }

  mutt_wait_filter(thepid);

  mutt_file_unlink(pk7out);

  fflush(fpout);
  rewind(fpout);
  fflush(fperr);
  rewind(fperr);
  empty = (fgetc(fpout) == EOF);
  if (empty)
  {
    mutt_file_copy_stream(fperr, stdout);
    mutt_file_fclose(&fpout);
    mutt_file_fclose(&fperr);
    mutt_file_unlink(certfile);
    return NULL;
  }

  mutt_file_fclose(&fpout);
  mutt_file_fclose(&fperr);

  return mutt_str_strdup(certfile);
}

static char *smime_extract_signer_certificate(char *infile)
{
  char certfile[_POSIX_PATH_MAX];
  char tmpfname[_POSIX_PATH_MAX];
  pid_t thepid;
  int empty;

  mutt_mktemp(tmpfname, sizeof(tmpfname));
  FILE *fperr = mutt_file_fopen(tmpfname, "w+");
  if (!fperr)
  {
    mutt_perror(tmpfname);
    return NULL;
  }
  mutt_file_unlink(tmpfname);

  mutt_mktemp(certfile, sizeof(certfile));
  FILE *fpout = mutt_file_fopen(certfile, "w+");
  if (!fpout)
  {
    mutt_file_fclose(&fperr);
    mutt_perror(certfile);
    return NULL;
  }

  /* Extract signer's certificate
   */
  thepid = smime_invoke(NULL, NULL, NULL, -1, -1, fileno(fperr), infile, NULL, NULL,
                        NULL, NULL, certfile, NULL, SmimeGetSignerCertCommand);
  if (thepid == -1)
  {
    mutt_any_key_to_continue(_("Error: unable to create OpenSSL subprocess!"));
    mutt_file_fclose(&fperr);
    mutt_file_fclose(&fpout);
    mutt_file_unlink(certfile);
    return NULL;
  }

  mutt_wait_filter(thepid);

  fflush(fpout);
  rewind(fpout);
  fflush(fperr);
  rewind(fperr);
  empty = (fgetc(fpout) == EOF);
  if (empty)
  {
    mutt_endwin();
    mutt_file_copy_stream(fperr, stdout);
    mutt_any_key_to_continue(NULL);
    mutt_file_fclose(&fpout);
    mutt_file_fclose(&fperr);
    mutt_file_unlink(certfile);
    return NULL;
  }

  mutt_file_fclose(&fpout);
  mutt_file_fclose(&fperr);

  return mutt_str_strdup(certfile);
}

/**
 * smime_invoke_import - Add a certificate and update index file (externally)
 */
void smime_invoke_import(char *infile, char *mailbox)
{
  char tmpfname[_POSIX_PATH_MAX], *certfile = NULL, buf[STRING];
  FILE *smimein = NULL;

  mutt_mktemp(tmpfname, sizeof(tmpfname));
  FILE *fperr = mutt_file_fopen(tmpfname, "w+");
  if (!fperr)
  {
    mutt_perror(tmpfname);
    return;
  }
  mutt_file_unlink(tmpfname);

  mutt_mktemp(tmpfname, sizeof(tmpfname));
  FILE *fpout = mutt_file_fopen(tmpfname, "w+");
  if (!fpout)
  {
    mutt_file_fclose(&fperr);
    mutt_perror(tmpfname);
    return;
  }
  mutt_file_unlink(tmpfname);

  buf[0] = '\0';
  if (SmimeAskCertLabel)
  {
    if ((mutt_get_field(_("Label for certificate: "), buf, sizeof(buf), 0) != 0) ||
        (buf[0] == 0))
    {
      mutt_file_fclose(&fpout);
      mutt_file_fclose(&fperr);
      return;
    }
  }

  mutt_endwin();
  certfile = smime_extract_certificate(infile);
  if (certfile)
  {
    mutt_endwin();

    pid_t thepid = smime_invoke(&smimein, NULL, NULL, -1, fileno(fpout), fileno(fperr), certfile,
                          NULL, NULL, NULL, NULL, NULL, NULL, SmimeImportCertCommand);
    if (thepid == -1)
    {
      mutt_message(_("Error: unable to create OpenSSL subprocess!"));
      return;
    }
    fputs(buf, smimein);
    fputc('\n', smimein);
    mutt_file_fclose(&smimein);

    mutt_wait_filter(thepid);

    mutt_file_unlink(certfile);
    FREE(&certfile);
  }

  fflush(fpout);
  rewind(fpout);
  fflush(fperr);
  rewind(fperr);

  mutt_file_copy_stream(fpout, stdout);
  mutt_file_copy_stream(fperr, stdout);

  mutt_file_fclose(&fpout);
  mutt_file_fclose(&fperr);
}

int smime_verify_sender(struct Header *h)
{
  char *mbox = NULL, *certfile = NULL, tempfname[_POSIX_PATH_MAX];
  int retval = 1;

  mutt_mktemp(tempfname, sizeof(tempfname));
  FILE *fpout = mutt_file_fopen(tempfname, "w");
  if (!fpout)
  {
    mutt_perror(tempfname);
    return 1;
  }

  if (h->security & ENCRYPT)
    mutt_copy_message_ctx(fpout, Context, h, MUTT_CM_DECODE_CRYPT & MUTT_CM_DECODE_SMIME,
                          CH_MIME | CH_WEED | CH_NONEWLINE);
  else
    mutt_copy_message_ctx(fpout, Context, h, 0, 0);

  fflush(fpout);
  mutt_file_fclose(&fpout);

  if (h->env->from)
  {
    h->env->from = mutt_expand_aliases(h->env->from);
    mbox = h->env->from->mailbox;
  }
  else if (h->env->sender)
  {
    h->env->sender = mutt_expand_aliases(h->env->sender);
    mbox = h->env->sender->mailbox;
  }

  if (mbox)
  {
    certfile = smime_extract_signer_certificate(tempfname);
    if (certfile)
    {
      mutt_file_unlink(tempfname);
      if (smime_handle_cert_email(certfile, mbox, 0, NULL, NULL))
      {
        if (isendwin())
          mutt_any_key_to_continue(NULL);
      }
      else
        retval = 0;
      mutt_file_unlink(certfile);
      FREE(&certfile);
    }
    else
      mutt_any_key_to_continue(_("no certfile"));
  }
  else
    mutt_any_key_to_continue(_("no mbox"));

  mutt_file_unlink(tempfname);
  return retval;
}

/*
 *    Creating S/MIME - bodies.
 */

static pid_t smime_invoke_encrypt(FILE **smimein, FILE **smimeout, FILE **smimeerr,
                                  int smimeinfd, int smimeoutfd, int smimeerrfd,
                                  const char *fname, const char *uids)
{
  return smime_invoke(smimein, smimeout, smimeerr, smimeinfd, smimeoutfd,
                      smimeerrfd, fname, NULL, SmimeEncryptWith, NULL, NULL,
                      uids, NULL, SmimeEncryptCommand);
}

static pid_t smime_invoke_sign(FILE **smimein, FILE **smimeout, FILE **smimeerr, int smimeinfd,
                               int smimeoutfd, int smimeerrfd, const char *fname)
{
  return smime_invoke(smimein, smimeout, smimeerr, smimeinfd, smimeoutfd, smimeerrfd,
                      fname, NULL, NULL, SmimeSignDigestAlg, SmimeKeyToUse,
                      SmimeCertToUse, SmimeIntermediateToUse, SmimeSignCommand);
}

struct Body *smime_build_smime_entity(struct Body *a, char *certlist)
{
  char buf[LONG_STRING], certfile[LONG_STRING];
  char tempfile[_POSIX_PATH_MAX], smimeerrfile[_POSIX_PATH_MAX];
  char smimeinfile[_POSIX_PATH_MAX];
  char *cert_start, *cert_end;
  FILE *smimein = NULL;
  int err = 0, empty, off;
  pid_t thepid;

  mutt_mktemp(tempfile, sizeof(tempfile));
  FILE *fpout = mutt_file_fopen(tempfile, "w+");
  if (!fpout)
  {
    mutt_perror(tempfile);
    return NULL;
  }

  mutt_mktemp(smimeerrfile, sizeof(smimeerrfile));
  FILE *smimeerr = mutt_file_fopen(smimeerrfile, "w+");
  if (!smimeerr)
  {
    mutt_perror(smimeerrfile);
    mutt_file_unlink(tempfile);
    mutt_file_fclose(&fpout);
    return NULL;
  }
  mutt_file_unlink(smimeerrfile);

  mutt_mktemp(smimeinfile, sizeof(smimeinfile));
  FILE *fptmp = mutt_file_fopen(smimeinfile, "w+");
  if (!fptmp)
  {
    mutt_perror(smimeinfile);
    mutt_file_unlink(tempfile);
    mutt_file_fclose(&fpout);
    mutt_file_fclose(&smimeerr);
    return NULL;
  }

  *certfile = '\0';
  for (cert_start = certlist; cert_start; cert_start = cert_end)
  {
    cert_end = strchr(cert_start, ' ');
    if (cert_end)
      *cert_end = '\0';
    if (*cert_start)
    {
      off = mutt_str_strlen(certfile);
      snprintf(certfile + off, sizeof(certfile) - off, "%s%s/%s",
               off ? " " : "", NONULL(SmimeCertificates), cert_start);
    }
    if (cert_end)
      *cert_end++ = ' ';
  }

  /* write a MIME entity */
  mutt_write_mime_header(a, fptmp);
  fputc('\n', fptmp);
  mutt_write_mime_body(a, fptmp);
  mutt_file_fclose(&fptmp);

  thepid = smime_invoke_encrypt(&smimein, NULL, NULL, -1, fileno(fpout),
                                fileno(smimeerr), smimeinfile, certfile);
  if (thepid == -1)
  {
    mutt_file_unlink(tempfile);
    mutt_file_fclose(&fpout);
    mutt_file_fclose(&smimeerr);
    mutt_file_unlink(smimeinfile);
    return NULL;
  }

  mutt_file_fclose(&smimein);

  mutt_wait_filter(thepid);
  mutt_file_unlink(smimeinfile);

  fflush(fpout);
  rewind(fpout);
  empty = (fgetc(fpout) == EOF);
  mutt_file_fclose(&fpout);

  fflush(smimeerr);
  rewind(smimeerr);
  while (fgets(buf, sizeof(buf) - 1, smimeerr) != NULL)
  {
    err = 1;
    fputs(buf, stdout);
  }
  mutt_file_fclose(&smimeerr);

  /* pause if there is any error output from SMIME */
  if (err)
    mutt_any_key_to_continue(NULL);

  if (empty)
  {
    /* fatal error while trying to encrypt message */
    if (!err)
      mutt_any_key_to_continue(_("No output from OpenSSL..."));
    mutt_file_unlink(tempfile);
    return NULL;
  }

  struct Body *t = mutt_body_new();
  t->type = TYPEAPPLICATION;
  t->subtype = mutt_str_strdup("x-pkcs7-mime");
  mutt_param_set(&t->parameter, "name", "smime.p7m");
  mutt_param_set(&t->parameter, "smime-type", "enveloped-data");
  t->encoding = ENCBASE64; /* The output of OpenSSL SHOULD be binary */
  t->use_disp = true;
  t->disposition = DISPATTACH;
  t->d_filename = mutt_str_strdup("smime.p7m");
  t->filename = mutt_str_strdup(tempfile);
  t->unlink = true; /* delete after sending the message */
  t->parts = 0;
  t->next = 0;

  return t;
}

/**
 * openssl_md_to_smime_micalg - Change the algorithm names
 *
 * The openssl -md doesn't want hyphens:
 *   md5, sha1,  sha224,  sha256,  sha384,  sha512
 * However, the micalg does:
 *   md5, sha-1, sha-224, sha-256, sha-384, sha-512
 */
static char *openssl_md_to_smime_micalg(char *md)
{
  if (!md)
    return 0;

  char *micalg = NULL;
  if (mutt_str_strncasecmp("sha", md, 3) == 0)
  {
    const size_t l = strlen(md) + 2;
    micalg = mutt_mem_malloc(l);
    snprintf(micalg, l, "sha-%s", md + 3);
  }
  else
  {
    micalg = mutt_str_strdup(md);
  }

  return micalg;
}

struct Body *smime_sign_message(struct Body *a)
{
  char buffer[LONG_STRING];
  char signedfile[_POSIX_PATH_MAX], filetosign[_POSIX_PATH_MAX];
  FILE *smimein = NULL, *smimeout = NULL, *smimeerr = NULL, *sfp = NULL;
  int err = 0;
  int empty = 0;
  pid_t thepid;
  char *intermediates = NULL;

  char *signas = (SmimeSignAs && *SmimeSignAs) ? SmimeSignAs : SmimeDefaultKey;
  if (!signas || !*signas)
  {
    mutt_error(_("Can't sign: No key specified. Use Sign As."));
    return NULL;
  }

  convert_to_7bit(a); /* Signed data _must_ be in 7-bit format. */

  mutt_mktemp(filetosign, sizeof(filetosign));
  sfp = mutt_file_fopen(filetosign, "w+");
  if (!sfp)
  {
    mutt_perror(filetosign);
    return NULL;
  }

  mutt_mktemp(signedfile, sizeof(signedfile));
  smimeout = mutt_file_fopen(signedfile, "w+");
  if (!smimeout)
  {
    mutt_perror(signedfile);
    mutt_file_fclose(&sfp);
    mutt_file_unlink(filetosign);
    return NULL;
  }

  mutt_write_mime_header(a, sfp);
  fputc('\n', sfp);
  mutt_write_mime_body(a, sfp);
  mutt_file_fclose(&sfp);

  snprintf(SmimeKeyToUse, sizeof(SmimeKeyToUse), "%s/%s", NONULL(SmimeKeys), signas);

  snprintf(SmimeCertToUse, sizeof(SmimeCertToUse), "%s/%s",
           NONULL(SmimeCertificates), signas);

  struct SmimeKey *signas_key = smime_get_key_by_hash(signas, 1);
  if ((!signas_key) || (!mutt_str_strcmp("?", signas_key->issuer)))
    intermediates = signas; /* so openssl won't complain in any case */
  else
    intermediates = signas_key->issuer;

  snprintf(SmimeIntermediateToUse, sizeof(SmimeIntermediateToUse), "%s/%s",
           NONULL(SmimeCertificates), intermediates);

  smime_free_key(&signas_key);

  thepid = smime_invoke_sign(&smimein, NULL, &smimeerr, -1, fileno(smimeout), -1, filetosign);
  if (thepid == -1)
  {
    mutt_perror(_("Can't open OpenSSL subprocess!"));
    mutt_file_fclose(&smimeout);
    mutt_file_unlink(signedfile);
    mutt_file_unlink(filetosign);
    return NULL;
  }
  fputs(SmimePass, smimein);
  fputc('\n', smimein);
  mutt_file_fclose(&smimein);

  mutt_wait_filter(thepid);

  /* check for errors from OpenSSL */
  err = 0;
  fflush(smimeerr);
  rewind(smimeerr);
  while (fgets(buffer, sizeof(buffer) - 1, smimeerr) != NULL)
  {
    err = 1;
    fputs(buffer, stdout);
  }
  mutt_file_fclose(&smimeerr);

  fflush(smimeout);
  rewind(smimeout);
  empty = (fgetc(smimeout) == EOF);
  mutt_file_fclose(&smimeout);

  mutt_file_unlink(filetosign);

  if (err)
    mutt_any_key_to_continue(NULL);

  if (empty)
  {
    mutt_any_key_to_continue(_("No output from OpenSSL..."));
    mutt_file_unlink(signedfile);
    return NULL; /* fatal error while signing */
  }

  struct Body *t = mutt_body_new();
  t->type = TYPEMULTIPART;
  t->subtype = mutt_str_strdup("signed");
  t->encoding = ENC7BIT;
  t->use_disp = false;
  t->disposition = DISPINLINE;

  mutt_generate_boundary(&t->parameter);

  char *micalg = openssl_md_to_smime_micalg(SmimeSignDigestAlg);
  mutt_param_set(&t->parameter, "micalg", micalg);
  FREE(&micalg);

  mutt_param_set(&t->parameter, "protocol", "application/x-pkcs7-signature");

  t->parts = a;
  a = t;

  t->parts->next = mutt_body_new();
  t = t->parts->next;
  t->type = TYPEAPPLICATION;
  t->subtype = mutt_str_strdup("x-pkcs7-signature");
  t->filename = mutt_str_strdup(signedfile);
  t->d_filename = mutt_str_strdup("smime.p7s");
  t->use_disp = true;
  t->disposition = DISPATTACH;
  t->encoding = ENCBASE64;
  t->unlink = true; /* ok to remove this file after sending. */

  return a;
}

/*
 *    Handling S/MIME - bodies.
 */

static pid_t smime_invoke_verify(FILE **smimein, FILE **smimeout, FILE **smimeerr,
                                 int smimeinfd, int smimeoutfd, int smimeerrfd,
                                 const char *fname, const char *sig_fname, int opaque)
{
  return smime_invoke(smimein, smimeout, smimeerr, smimeinfd, smimeoutfd,
                      smimeerrfd, fname, sig_fname, NULL, NULL, NULL, NULL, NULL,
                      (opaque ? SmimeVerifyOpaqueCommand : SmimeVerifyCommand));
}

static pid_t smime_invoke_decrypt(FILE **smimein, FILE **smimeout,
                                  FILE **smimeerr, int smimeinfd, int smimeoutfd,
                                  int smimeerrfd, const char *fname)
{
  return smime_invoke(smimein, smimeout, smimeerr, smimeinfd, smimeoutfd,
                      smimeerrfd, fname, NULL, NULL, NULL, SmimeKeyToUse,
                      SmimeCertToUse, NULL, SmimeDecryptCommand);
}

int smime_verify_one(struct Body *sigbdy, struct State *s, const char *tempfile)
{
  char signedfile[_POSIX_PATH_MAX], smimeerrfile[_POSIX_PATH_MAX];
  FILE *smimeout = NULL;
  pid_t thepid;
  int badsig = -1;

  LOFF_T tmpoffset = 0;
  size_t tmplength = 0;
  int orig_type = sigbdy->type;

  snprintf(signedfile, sizeof(signedfile), "%s.sig", tempfile);

  /* decode to a tempfile, saving the original destination */
  FILE *fp = s->fpout;
  s->fpout = mutt_file_fopen(signedfile, "w");
  if (!s->fpout)
  {
    mutt_perror(signedfile);
    return -1;
  }
  /* decoding the attachment changes the size and offset, so save a copy
   * of the "real" values now, and restore them after processing
   */
  tmplength = sigbdy->length;
  tmpoffset = sigbdy->offset;

  /* if we are decoding binary bodies, we don't want to prefix each
   * line with the prefix or else the data will get corrupted.
   */
  char *save_prefix = s->prefix;
  s->prefix = NULL;

  mutt_decode_attachment(sigbdy, s);

  sigbdy->length = ftello(s->fpout);
  sigbdy->offset = 0;
  mutt_file_fclose(&s->fpout);

  /* restore final destination and substitute the tempfile for input */
  s->fpout = fp;
  fp = s->fpin;
  s->fpin = fopen(signedfile, "r");

  /* restore the prefix */
  s->prefix = save_prefix;

  sigbdy->type = orig_type;

  mutt_mktemp(smimeerrfile, sizeof(smimeerrfile));
  FILE *smimeerr = mutt_file_fopen(smimeerrfile, "w+");
  if (!smimeerr)
  {
    mutt_perror(smimeerrfile);
    mutt_file_unlink(signedfile);
    return -1;
  }

  crypt_current_time(s, "OpenSSL");

  thepid = smime_invoke_verify(NULL, &smimeout, NULL, -1, -1, fileno(smimeerr),
                               tempfile, signedfile, 0);
  if (thepid != -1)
  {
    fflush(smimeout);
    mutt_file_fclose(&smimeout);

    if (mutt_wait_filter(thepid))
      badsig = -1;
    else
    {
      char *line = NULL;
      int lineno = 0;
      size_t linelen;

      fflush(smimeerr);
      rewind(smimeerr);

      line = mutt_file_read_line(line, &linelen, smimeerr, &lineno, 0);
      if (linelen && (mutt_str_strcasecmp(line, "verification successful") == 0))
        badsig = 0;

      FREE(&line);
    }
  }

  fflush(smimeerr);
  rewind(smimeerr);
  mutt_file_copy_stream(smimeerr, s->fpout);
  mutt_file_fclose(&smimeerr);

  state_attach_puts(_("[-- End of OpenSSL output --]\n\n"), s);

  mutt_file_unlink(signedfile);
  mutt_file_unlink(smimeerrfile);

  sigbdy->length = tmplength;
  sigbdy->offset = tmpoffset;

  /* restore the original source stream */
  mutt_file_fclose(&s->fpin);
  s->fpin = fp;

  return badsig;
}

/**
 * smime_handle_entity - Handle type application/pkcs7-mime
 *
 * This can either be a signed or an encrypted message.
 */
static struct Body *smime_handle_entity(struct Body *m, struct State *s, FILE *out_file)
{
  char outfile[_POSIX_PATH_MAX], errfile[_POSIX_PATH_MAX];
  char tmpfname[_POSIX_PATH_MAX];
  FILE *smimeout = NULL, *smimein = NULL, *smimeerr = NULL;
  FILE *tmpfp = NULL, *tmpfp_buffer = NULL, *fpout = NULL;
  struct stat info;
  struct Body *p = NULL;
  pid_t thepid = -1;
  unsigned int type = mutt_is_application_smime(m);

  if (!(type & APPLICATION_SMIME))
    return NULL;

  mutt_mktemp(outfile, sizeof(outfile));
  smimeout = mutt_file_fopen(outfile, "w+");
  if (!smimeout)
  {
    mutt_perror(outfile);
    return NULL;
  }

  mutt_mktemp(errfile, sizeof(errfile));
  smimeerr = mutt_file_fopen(errfile, "w+");
  if (!smimeerr)
  {
    mutt_perror(errfile);
    mutt_file_fclose(&smimeout);
    return NULL;
  }
  mutt_file_unlink(errfile);

  mutt_mktemp(tmpfname, sizeof(tmpfname));
  tmpfp = mutt_file_fopen(tmpfname, "w+");
  if (!tmpfp)
  {
    mutt_perror(tmpfname);
    mutt_file_fclose(&smimeout);
    mutt_file_fclose(&smimeerr);
    return NULL;
  }

  fseeko(s->fpin, m->offset, SEEK_SET);

  mutt_file_copy_bytes(s->fpin, tmpfp, m->length);

  fflush(tmpfp);
  mutt_file_fclose(&tmpfp);

  if ((type & ENCRYPT) &&
      (thepid = smime_invoke_decrypt(&smimein, NULL, NULL, -1, fileno(smimeout),
                                     fileno(smimeerr), tmpfname)) == -1)
  {
    mutt_file_fclose(&smimeout);
    mutt_file_unlink(tmpfname);
    if (s->flags & MUTT_DISPLAY)
      state_attach_puts(
          _("[-- Error: unable to create OpenSSL subprocess! --]\n"), s);
    mutt_file_fclose(&smimeerr);
    return NULL;
  }
  else if ((type & SIGNOPAQUE) &&
           (thepid = smime_invoke_verify(&smimein, NULL, NULL, -1, fileno(smimeout),
                                         fileno(smimeerr), NULL, tmpfname, SIGNOPAQUE)) == -1)
  {
    mutt_file_fclose(&smimeout);
    mutt_file_unlink(tmpfname);
    if (s->flags & MUTT_DISPLAY)
      state_attach_puts(
          _("[-- Error: unable to create OpenSSL subprocess! --]\n"), s);
    mutt_file_fclose(&smimeerr);
    return NULL;
  }

  if (type & ENCRYPT)
  {
    if (!smime_valid_passphrase())
      smime_void_passphrase();
    fputs(SmimePass, smimein);
    fputc('\n', smimein);
  }

  mutt_file_fclose(&smimein);

  mutt_wait_filter(thepid);
  mutt_file_unlink(tmpfname);

  if (s->flags & MUTT_DISPLAY)
  {
    fflush(smimeerr);
    rewind(smimeerr);

    const int c = fgetc(smimeerr);
    if (c != EOF)
    {
      ungetc(c, smimeerr);

      crypt_current_time(s, "OpenSSL");
      mutt_file_copy_stream(smimeerr, s->fpout);
      state_attach_puts(_("[-- End of OpenSSL output --]\n\n"), s);
    }

    if (type & ENCRYPT)
      state_attach_puts(_("[-- The following data is S/MIME"
                          " encrypted --]\n"),
                        s);
    else
      state_attach_puts(_("[-- The following data is S/MIME signed --]\n"), s);
  }

  if (smimeout)
  {
    fflush(smimeout);
    rewind(smimeout);

    char tmptmpfname[_POSIX_PATH_MAX];
    if (out_file)
      fpout = out_file;
    else
    {
      mutt_mktemp(tmptmpfname, sizeof(tmptmpfname));
      fpout = mutt_file_fopen(tmptmpfname, "w+");
      if (!fpout)
      {
        mutt_perror(tmptmpfname);
        mutt_file_fclose(&smimeout);
        mutt_file_fclose(&smimeerr);
        return NULL;
      }
    }
    char buf[HUGE_STRING];
    while (fgets(buf, sizeof(buf) - 1, smimeout) != NULL)
    {
      const size_t len = mutt_str_strlen(buf);
      if (len > 1 && buf[len - 2] == '\r')
      {
        buf[len - 2] = '\n';
        buf[len - 1] = '\0';
      }
      fputs(buf, fpout);
    }
    fflush(fpout);
    rewind(fpout);

    p = mutt_read_mime_header(fpout, 0);
    if (p)
    {
      fstat(fileno(fpout), &info);
      p->length = info.st_size - p->offset;

      mutt_parse_part(fpout, p);
      if (s->fpout)
      {
        rewind(fpout);
        tmpfp_buffer = s->fpin;
        s->fpin = fpout;
        mutt_body_handler(p, s);
        s->fpin = tmpfp_buffer;
      }
    }
    mutt_file_fclose(&smimeout);
    smimeout = NULL;
    mutt_file_unlink(outfile);

    if (!out_file)
    {
      mutt_file_fclose(&fpout);
      mutt_file_unlink(tmptmpfname);
    }
    fpout = NULL;
  }

  if (s->flags & MUTT_DISPLAY)
  {
    if (type & ENCRYPT)
      state_attach_puts(_("\n[-- End of S/MIME encrypted data. --]\n"), s);
    else
      state_attach_puts(_("\n[-- End of S/MIME signed data. --]\n"), s);
  }

  if (type & SIGNOPAQUE)
  {
    char *line = NULL;
    int lineno = 0;
    size_t linelen;

    rewind(smimeerr);

    line = mutt_file_read_line(line, &linelen, smimeerr, &lineno, 0);
    if (linelen && (mutt_str_strcasecmp(line, "verification successful") == 0))
      m->goodsig = true;
    FREE(&line);
  }
  else
  {
    m->goodsig = p->goodsig;
    m->badsig = p->badsig;
  }
  mutt_file_fclose(&smimeerr);

  return p;
}

int smime_decrypt_mime(FILE *fpin, FILE **fpout, struct Body *b, struct Body **cur)
{
  char tempfile[_POSIX_PATH_MAX];
  struct State s;
  LOFF_T tmpoffset = b->offset;
  size_t tmplength = b->length;
  int orig_type = b->type;
  FILE *tmpfp = NULL;
  int rc = 0;

  if (!mutt_is_application_smime(b))
    return -1;

  if (b->parts)
    return -1;

  memset(&s, 0, sizeof(s));
  s.fpin = fpin;
  fseeko(s.fpin, b->offset, SEEK_SET);

  mutt_mktemp(tempfile, sizeof(tempfile));
  tmpfp = mutt_file_fopen(tempfile, "w+");
  if (!tmpfp)
  {
    mutt_perror(tempfile);
    return -1;
  }

  mutt_file_unlink(tempfile);
  s.fpout = tmpfp;
  mutt_decode_attachment(b, &s);
  fflush(tmpfp);
  b->length = ftello(s.fpout);
  b->offset = 0;
  rewind(tmpfp);
  s.fpin = tmpfp;
  s.fpout = 0;

  mutt_mktemp(tempfile, sizeof(tempfile));
  *fpout = mutt_file_fopen(tempfile, "w+");
  if (!*fpout)
  {
    mutt_perror(tempfile);
    rc = -1;
    goto bail;
  }
  mutt_file_unlink(tempfile);

  *cur = smime_handle_entity(b, &s, *fpout);
  if (!*cur)
  {
    rc = -1;
    goto bail;
  }

  (*cur)->goodsig = b->goodsig;
  (*cur)->badsig = b->badsig;

bail:
  b->type = orig_type;
  b->length = tmplength;
  b->offset = tmpoffset;
  mutt_file_fclose(&tmpfp);
  if (*fpout)
    rewind(*fpout);

  return rc;
}

int smime_application_smime_handler(struct Body *m, struct State *s)
{
  return smime_handle_entity(m, s, NULL) ? 0 : -1;
}

int smime_send_menu(struct Header *msg)
{
  struct SmimeKey *key = NULL;
  char *prompt = NULL, *letters = NULL, *choices = NULL;
  int choice;

  if (!(WithCrypto & APPLICATION_SMIME))
    return msg->security;

  msg->security |= APPLICATION_SMIME;

  /*
   * Opportunistic encrypt is controlling encryption.
   * NOTE: "Signing" and "Clearing" only adjust the sign bit, so we have different
   *       letter choices for those.
   */
  if (CryptOpportunisticEncrypt && (msg->security & OPPENCRYPT))
  {
    /* L10N: S/MIME options (opportunistic encryption is on) */
    prompt = _("S/MIME (s)ign, encrypt (w)ith, sign (a)s, (c)lear, or (o)ppenc "
               "mode off? ");
    /* L10N: S/MIME options (opportunistic encryption is on) */
    letters = _("swaco");
    choices = "SwaCo";
  }
  /*
   * Opportunistic encryption option is set, but is toggled off
   * for this message.
   */
  else if (CryptOpportunisticEncrypt)
  {
    /* L10N: S/MIME options (opportunistic encryption is off) */
    prompt = _("S/MIME (e)ncrypt, (s)ign, encrypt (w)ith, sign (a)s, (b)oth, "
               "(c)lear, or (o)ppenc mode? ");
    /* L10N: S/MIME options (opportunistic encryption is off) */
    letters = _("eswabco");
    choices = "eswabcO";
  }
  /*
   * Opportunistic encryption is unset
   */
  else
  {
    /* L10N: S/MIME options */
    prompt = _("S/MIME (e)ncrypt, (s)ign, encrypt (w)ith, sign (a)s, (b)oth, "
               "or (c)lear? ");
    /* L10N: S/MIME options */
    letters = _("eswabc");
    choices = "eswabc";
  }

  choice = mutt_multi_choice(prompt, letters);
  if (choice > 0)
  {
    switch (choices[choice - 1])
    {
      case 'a': /* sign (a)s */
        key = smime_ask_for_key(_("Sign as: "), KEYFLAG_CANSIGN, 0);
        if (key)
        {
          mutt_str_replace(&SmimeSignAs, key->hash);
          smime_free_key(&key);

          msg->security |= SIGN;

          /* probably need a different passphrase */
          crypt_smime_void_passphrase();
        }

        break;

      case 'b': /* (b)oth */
        msg->security |= (ENCRYPT | SIGN);
        break;

      case 'c': /* (c)lear */
        msg->security &= ~(ENCRYPT | SIGN);
        break;

      case 'C':
        msg->security &= ~SIGN;
        break;

      case 'e': /* (e)ncrypt */
        msg->security |= ENCRYPT;
        msg->security &= ~SIGN;
        break;

      case 'O': /* oppenc mode on */
        msg->security |= OPPENCRYPT;
        crypt_opportunistic_encrypt(msg);
        break;

      case 'o': /* oppenc mode off */
        msg->security &= ~OPPENCRYPT;
        break;

      case 'S': /* (s)ign in oppenc mode */
        msg->security |= SIGN;
        break;

      case 's': /* (s)ign */
        msg->security &= ~ENCRYPT;
        msg->security |= SIGN;
        break;

      case 'w': /* encrypt (w)ith */
      {
        msg->security |= ENCRYPT;
        do
        {
          /* I use "dra" because "123" is recognized anyway */
          switch (mutt_multi_choice(_("Choose algorithm family:"
                                      " 1: DES, 2: RC2, 3: AES,"
                                      " or (c)lear? "),
                                    _("123c")))
          {
            case 1:
              switch (choice = mutt_multi_choice(_("1: DES, 2: Triple-DES "), _("12")))
              {
                case 1:
                  mutt_str_replace(&SmimeEncryptWith, "des");
                  break;
                case 2:
                  mutt_str_replace(&SmimeEncryptWith, "des3");
                  break;
              }
              break;

            case 2:
              switch (choice = mutt_multi_choice(
                          _("1: RC2-40, 2: RC2-64, 3: RC2-128 "), _("123")))
              {
                case 1:
                  mutt_str_replace(&SmimeEncryptWith, "rc2-40");
                  break;
                case 2:
                  mutt_str_replace(&SmimeEncryptWith, "rc2-64");
                  break;
                case 3:
                  mutt_str_replace(&SmimeEncryptWith, "rc2-128");
                  break;
              }
              break;

            case 3:
              switch (choice = mutt_multi_choice(
                          _("1: AES128, 2: AES192, 3: AES256 "), _("123")))
              {
                case 1:
                  mutt_str_replace(&SmimeEncryptWith, "aes128");
                  break;
                case 2:
                  mutt_str_replace(&SmimeEncryptWith, "aes192");
                  break;
                case 3:
                  mutt_str_replace(&SmimeEncryptWith, "aes256");
                  break;
              }
              break;

            case 4:
              FREE(&SmimeEncryptWith);
            /* (c)lear */
            /* fallthrough */
            case -1: /* Ctrl-G or Enter */
              choice = 0;
              break;
          }
        } while (choice == -1);
      }
      break;
    }
  }

  return msg->security;
}
