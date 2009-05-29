/*
 * Copyright (C) 2001,2002 Oliver Ehli <elmy@acm.org>
 * Copyright (C) 2002 Mike Schiraldi <raldi@research.netsol.com>
 * Copyright (C) 2004 g10 Code GmbH
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "smime.h"
#include "mime.h"
#include "copy.h"

#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#ifdef CRYPT_BACKEND_CLASSIC_SMIME

#include "mutt_crypt.h"

struct smime_command_context {
  const char *key;		    /* %k */
  const char *cryptalg;		    /* %a */
  const char *fname;		    /* %f */
  const char *sig_fname;	    /* %s */
  const char *certificates;	    /* %c */
  const char *intermediates;        /* %i */
};


typedef struct {
  unsigned int hash;
  char suffix;
  char email[256];
  char nick[256];
  char trust; /* i=Invalid r=revoked e=expired u=unverified v=verified t=trusted */
  short public; /* 1=public 0=private */
} smime_id;


char SmimePass[STRING];
time_t SmimeExptime = 0; /* when does the cached passphrase expire? */


static char SmimeKeyToUse[_POSIX_PATH_MAX] = { 0 };
static char SmimeCertToUse[_POSIX_PATH_MAX];
static char SmimeIntermediateToUse[_POSIX_PATH_MAX];


/*
 *     Queries and passphrase handling.
 */




/* these are copies from pgp.c */


void smime_void_passphrase (void)
{
  memset (SmimePass, 0, sizeof (SmimePass));
  SmimeExptime = 0;
}

int smime_valid_passphrase (void)
{
  time_t now = time (NULL);

  if (now < SmimeExptime)
    /* Use cached copy.  */
    return 1;

  smime_void_passphrase();
  
  if (mutt_get_password (_("Enter S/MIME passphrase:"), SmimePass, sizeof (SmimePass)) == 0)
    {
      SmimeExptime = time (NULL) + SmimeTimeout;
      return (1);
    }
  else
    SmimeExptime = 0;

  return 0;
}


/*
 *     The OpenSSL interface
 */

/* This is almost identical to ppgp's invoking interface. */

static const char *_mutt_fmt_smime_command (char *dest,
					    size_t destlen,
					    size_t col,
					    char op,
					    const char *src,
					    const char *prefix,
					    const char *ifstring,
					    const char *elsestring,
					    unsigned long data,
					    format_flag flags)
{
  char fmt[16];
  struct smime_command_context *cctx = (struct smime_command_context *) data;
  int optional = (flags & M_FORMAT_OPTIONAL);
  
  switch (op)
  {
    case 'C':
    {
      if (!optional)
      {
	char path[_POSIX_PATH_MAX];
	char buf1[LONG_STRING], buf2[LONG_STRING];
	struct stat sb;

	strfcpy (path, NONULL (SmimeCALocation), sizeof (path));
	mutt_expand_path (path, sizeof (path));
	mutt_quote_filename (buf1, sizeof (buf1), path);

	if (stat (path, &sb) != 0 || !S_ISDIR (sb.st_mode))
	  snprintf (buf2, sizeof (buf2), "-CAfile %s", buf1);
	else
	  snprintf (buf2, sizeof (buf2), "-CApath %s", buf1);
	
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, buf2);
      }
      else if (!SmimeCALocation)
	optional = 0;
      break;
    }
    
    case 'c':
    {           /* certificate (list) */
      if (!optional) {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, NONULL(cctx->certificates));
      }
      else if (!cctx->certificates)
	optional = 0;
      break;
    }
    
    case 'i':
    {           /* intermediate certificates  */
      if (!optional) {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, NONULL(cctx->intermediates));
      }
      else if (!cctx->intermediates)
	optional = 0;
      break;
    }
    
    case 's':
    {           /* detached signature */
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, NONULL (cctx->sig_fname));
      }
      else if (!cctx->sig_fname)
	optional = 0;
      break;
    }
    
    case 'k':
    {           /* private key */
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, NONULL (cctx->key));
      }
      else if (!cctx->key)
	optional = 0;
      break;
    }
    
    case 'a':
    {           /* algorithm for encryption */
      if (!optional) {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, NONULL (cctx->cryptalg));
      }
      else if (!cctx->key)
	optional = 0;
      break;
    }
    
    case 'f':
    {           /* file to process */
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, NONULL (cctx->fname));
      }
      else if (!cctx->fname)
	optional = 0;
      break;
    }
    
    default:
      *dest = '\0';
      break;
  }

  if (optional)
    mutt_FormatString (dest, destlen, col, ifstring, _mutt_fmt_smime_command,
		       data, 0);
  else if (flags & M_FORMAT_OPTIONAL)
    mutt_FormatString (dest, destlen, col, elsestring, _mutt_fmt_smime_command,
		       data, 0);

  return (src);
}



static void mutt_smime_command (char *d, size_t dlen,
				struct smime_command_context *cctx, const char *fmt)
{
  mutt_FormatString (d, dlen, 0, NONULL(fmt), _mutt_fmt_smime_command,
		    (unsigned long) cctx, 0);
  dprint (2,(debugfile, "mutt_smime_command: %s\n", d));
}




static pid_t smime_invoke (FILE **smimein, FILE **smimeout, FILE **smimeerr,
			   int smimeinfd, int smimeoutfd, int smimeerrfd,
			   const char *fname,
			   const char *sig_fname,
			   const char *cryptalg,
			   const char *key,
			   const char *certificates,
			   const char *intermediates,
			   const char *format)
{
  struct smime_command_context cctx;
  char cmd[HUGE_STRING];
  
  memset (&cctx, 0, sizeof (cctx));

  if (!format || !*format)
    return (pid_t) -1;
  
  cctx.fname	       = fname;
  cctx.sig_fname       = sig_fname;
  cctx.key	       = key;
  cctx.cryptalg	       = cryptalg;
  cctx.certificates    = certificates;
  cctx.intermediates   = intermediates;
  
  mutt_smime_command (cmd, sizeof (cmd), &cctx, format);

  return mutt_create_filter_fd (cmd, smimein, smimeout, smimeerr,
				smimeinfd, smimeoutfd, smimeerrfd);
}






/*
 *    Key and certificate handling.
 */



/* 
   Search the certificate index for given mailbox.
   return certificate file name.
*/

static void smime_entry (char *s, size_t l, MUTTMENU * menu, int num)
{
  smime_id *Table = (smime_id*) menu->data;
  smime_id this = Table[num];
  char* truststate;
  switch(this.trust) {
    case 't':
      truststate = N_("Trusted   ");
      break;
    case 'v':
      truststate = N_("Verified  ");
      break;
    case 'u':
      truststate = N_("Unverified");
      break;
    case 'e':
      truststate = N_("Expired   ");
      break;
    case 'r':
      truststate = N_("Revoked   ");
      break;
    case 'i':
      truststate = N_("Invalid   ");
      break;
    default:
      truststate = N_("Unknown   ");
  }
  if (this.public)
    snprintf(s, l, " 0x%.8X.%i %s %-35.35s %s", this.hash, this.suffix, truststate, this.email, this.nick);
  else
    snprintf(s, l, " 0x%.8X.%i %-35.35s %s", this.hash, this.suffix, this.email, this.nick);
}





char* smime_ask_for_key (char *prompt, char *mailbox, short public)
{
  char *fname;
  smime_id *Table;
  long cert_num; /* Will contain the number of certificates.
      * To be able to get it, the .index file will be read twice... */
  char index_file[_POSIX_PATH_MAX];
  FILE *index;
  char buf[LONG_STRING];
  char fields[5][STRING];
  int numFields, hash_suffix, done, cur; /* The current entry */
  MUTTMENU* menu;
  unsigned int hash;
  char helpstr[HUGE_STRING*3];
  char qry[256];
  char title[256];

  if (!prompt) prompt = _("Enter keyID: ");
  snprintf(index_file, sizeof (index_file), "%s/.index",
    public ? NONULL(SmimeCertificates) : NONULL(SmimeKeys));
  
  index = fopen(index_file, "r");
  if (index == NULL) 
  {
    mutt_perror (index_file);      
    return NULL;
  }
  /* Count Lines */
  cert_num = 0;
  while (!feof(index)) {
    if (fgets(buf, sizeof(buf), index)) cert_num++;
  }
  safe_fclose (&index);

  FOREVER
  {
    *qry = 0;
    if (mutt_get_field(prompt,
      qry, sizeof(qry), 0))
      return NULL;
    snprintf(title, sizeof(title), _("S/MIME certificates matching \"%s\"."),
      qry);

    
    index = fopen(index_file, "r");
    if (index == NULL) 
    {
      mutt_perror (index_file);      
      return NULL;
    }
    /* Read Entries */
    cur = 0;
    Table = safe_calloc(cert_num, sizeof (smime_id));
    while (!feof(index)) {
        numFields = fscanf (index, MUTT_FORMAT(STRING) " %x.%i " MUTT_FORMAT(STRING), fields[0], &hash,
          &hash_suffix, fields[2]);
        if (public)
          fscanf (index, MUTT_FORMAT(STRING) " " MUTT_FORMAT(STRING) "\n", fields[3], fields[4]);
  
      /* 0=email 1=name 2=nick 3=intermediate 4=trust */
      if (numFields < 2) continue;
  
      /* Check if query matches this certificate */
      if (!mutt_stristr(fields[0], qry) &&
          !mutt_stristr(fields[2], qry))
        continue;
  
      Table[cur].hash = hash;
      Table[cur].suffix = hash_suffix;
      strncpy(Table[cur].email, fields[0], sizeof(Table[cur].email));
      strncpy(Table[cur].nick, fields[2], sizeof(Table[cur].nick));
      Table[cur].trust = *fields[4];
      Table[cur].public = public;
  
      cur++;
    }
    safe_fclose (&index);
  
    /* Make Helpstring */
    helpstr[0] = 0;
    mutt_make_help (buf, sizeof (buf), _("Exit  "), MENU_SMIME, OP_EXIT);
    strcat (helpstr, buf);	/* __STRCAT_CHECKED__ */
    mutt_make_help (buf, sizeof (buf), _("Select  "), MENU_SMIME,
        OP_GENERIC_SELECT_ENTRY);
    strcat (helpstr, buf);	/* __STRCAT_CHECKED__ */
    mutt_make_help (buf, sizeof(buf), _("Help"), MENU_SMIME, OP_HELP);
    strcat (helpstr, buf);	/* __STRCAT_CHECKED__ */
  
    /* Create the menu */
    menu = mutt_new_menu(MENU_SMIME);
    menu->max = cur;
    menu->make_entry = smime_entry;
    menu->help = helpstr;
    menu->data = Table;
    menu->title = title;
    /* sorting keys might be done later - TODO */
  
    mutt_clear_error();
  
    done = 0;
    hash = 0;
    while (!done) {
      switch (mutt_menuLoop (menu)) {
        case OP_GENERIC_SELECT_ENTRY:
          cur = menu->current;
  	hash = 1;
          done = 1;
          break;
        case OP_EXIT:
          hash = 0;
          done = 1;
          break;
      }
    }
    if (hash) {
      fname = safe_malloc(13); /* Hash + '.' + Suffix + \0 */
      sprintf(fname, "%.8x.%i", Table[cur].hash, Table[cur].suffix);
    }
    else fname = NULL;
  
    mutt_menuDestroy (&menu);
    FREE (&Table);
    set_option (OPTNEEDREDRAW);
  
    if (fname) return fname;
  }
}



char *smime_get_field_from_db (char *mailbox, char *query, short public, short may_ask)
{
  int addr_len, query_len, found = 0, ask = 0, choice = 0;
  char cert_path[_POSIX_PATH_MAX];
  char buf[LONG_STRING], prompt[STRING];
  char fields[5][STRING];
  char key[STRING];  
  int numFields;
  struct stat info;
  char key_trust_level = 0;
  FILE *fp;

  if(!mailbox && !query) return(NULL);

  addr_len = mailbox ? mutt_strlen (mailbox) : 0;
  query_len = query ? mutt_strlen (query) : 0;
  
  *key = '\0';

  /* index-file format:
     mailbox certfile label issuer_certfile trust_flags\n

     certfile is a hash value generated by openssl.
     Note that this was done according to the OpenSSL
     specs on their CA-directory.

  */
  snprintf (cert_path, sizeof (cert_path), "%s/.index",
	    (public ? NONULL(SmimeCertificates) : NONULL(SmimeKeys)));

  if (!stat (cert_path, &info))
  {
    if ((fp = safe_fopen (cert_path, "r")) == NULL)
    {
      mutt_perror (cert_path);
      return (NULL);
    }

    while (fgets (buf, sizeof (buf) - 1, fp) != NULL)
      if (mailbox && !(mutt_strncasecmp (mailbox, buf, addr_len)))
      {
	numFields = sscanf (buf, 
			    MUTT_FORMAT(STRING) " " MUTT_FORMAT(STRING) " " 
			    MUTT_FORMAT(STRING) " " MUTT_FORMAT(STRING) " " 
			    MUTT_FORMAT(STRING) "\n", 
			    fields[0], fields[1],
			   fields[2], fields[3], 
			    fields[4]);
	if (numFields < 2)
	    continue;
	if (mailbox && public && 
	    (*fields[4] == 'i' || *fields[4] == 'e' || *fields[4] == 'r'))
	    continue;

	if (found)
	{
	  if (public && *fields[4] == 'u' )
	    snprintf (prompt, sizeof (prompt),
		      _("ID %s is unverified. Do you want to use it for %s ?"),
		      fields[1], mailbox);
	  else if (public && *fields[4] == 'v' )
	    snprintf (prompt, sizeof (prompt),
		      _("Use (untrusted!) ID %s for %s ?"),
		      fields[1], mailbox);
	  else
	    snprintf (prompt, sizeof (prompt), _("Use ID %s for %s ?"),
		      fields[1], mailbox);
	  if (may_ask == 0)
	    choice = M_YES;
	  if (may_ask && (choice = mutt_yesorno (prompt, M_NO)) == -1)
	  {
	    found = 0;
	    ask = 0;
	    *key = '\0';
	    break;
	  }
	  else if (choice == M_NO) 
	  {
	    ask = 1;
	    continue;
	  }
	  else if (choice == M_YES)
	  {
	    strfcpy (key, fields[1], sizeof (key));
	    ask = 0;
	    break;
	  }
	}
	else
	{
	  if (public) 
	    key_trust_level = *fields[4];
	  strfcpy (key, fields[1], sizeof (key));
	}
	found = 1;
      }
      else if(query)
      {
	numFields = sscanf (buf, 
			    MUTT_FORMAT(STRING) " " MUTT_FORMAT(STRING) " " 
			    MUTT_FORMAT(STRING) " " MUTT_FORMAT(STRING) " " 
			    MUTT_FORMAT(STRING) "\n", 
			    fields[0], fields[1],
			    fields[2], fields[3], 
			    fields[4]);

	/* query = label: return certificate. */
	if (numFields >= 3 && 
	    !(mutt_strncasecmp (query, fields[2], query_len)))
	{
	  ask = 0;
	  strfcpy (key, fields[1], sizeof (key));
	}
	/* query = certificate: return intermediate certificate. */
	else if (numFields >= 4 && 
		 !(mutt_strncasecmp (query, fields[1], query_len)))
	{
	  ask = 0;
	  strfcpy (key, fields[3], sizeof (key));
	}
      }

    safe_fclose (&fp);

    if (ask)
    {
      if (public && *fields[4] == 'u' )
	snprintf (prompt, sizeof (prompt),
		  _("ID %s is unverified. Do you want to use it for %s ?"),
		  fields[1], mailbox);
      else if (public && *fields[4] == 'v' )
	snprintf (prompt, sizeof (prompt),
		  _("Use (untrusted!) ID %s for %s ?"),
		  fields[1], mailbox);
      else
	snprintf (prompt, sizeof(prompt), _("Use ID %s for %s ?"), key,
		  mailbox);
      choice = mutt_yesorno (prompt, M_NO);
      if (choice == -1 || choice == M_NO)
	*key = '\0';
    }
    else if (key_trust_level && may_ask)
    {
      if (key_trust_level == 'u' )
      {
	snprintf (prompt, sizeof (prompt),
		  _("ID %s is unverified. Do you want to use it for %s ?"),
		  key, mailbox);
	choice = mutt_yesorno (prompt, M_NO);
	if (choice != M_YES)
	  *key = '\0';
      }
      else if (key_trust_level == 'v' )
      {
	mutt_error (_("Warning: You have not yet decided to trust ID %s. (any key to continue)"), key);
	mutt_sleep (5);
      }
    }

  }

  /* Note: safe_strdup ("") returns NULL. */
  return safe_strdup (key);
}




/* 
   This sets the '*ToUse' variables for an upcoming decryption, where
   the reuquired key is different from SmimeDefaultKey.
*/

void _smime_getkeys (char *mailbox)
{
  char *k = NULL;
  char buf[STRING];

  k = smime_get_field_from_db (mailbox, NULL, 0, 1);

  if (!k)
  {
    snprintf(buf, sizeof(buf), _("Enter keyID for %s: "),
	     mailbox);
    k = smime_ask_for_key(buf, mailbox, 0);
  }

  if (k)
  {
    /* the key used last time. */
    if (*SmimeKeyToUse && 
        !mutt_strcasecmp (k, SmimeKeyToUse + mutt_strlen (SmimeKeys)+1))
    {
      FREE (&k);
      return;
    }
    else smime_void_passphrase ();

    snprintf (SmimeKeyToUse, sizeof (SmimeKeyToUse), "%s/%s", 
	      NONULL(SmimeKeys), k);
    
    snprintf (SmimeCertToUse, sizeof (SmimeCertToUse), "%s/%s",
	      NONULL(SmimeCertificates), k);

    if (mutt_strcasecmp (k, SmimeDefaultKey))
      smime_void_passphrase ();

    FREE (&k);
    return;
  }

  if (*SmimeKeyToUse)
  {
    if (!mutt_strcasecmp (SmimeDefaultKey, 
                          SmimeKeyToUse + mutt_strlen (SmimeKeys)+1))
      return;

    smime_void_passphrase ();
  }

  snprintf (SmimeKeyToUse, sizeof (SmimeKeyToUse), "%s/%s", 
	    NONULL (SmimeKeys), NONULL (SmimeDefaultKey));
  
  snprintf (SmimeCertToUse, sizeof (SmimeCertToUse), "%s/%s",
	    NONULL (SmimeCertificates), NONULL (SmimeDefaultKey));
}

void smime_getkeys (ENVELOPE *env)
{
  ADDRESS *t;
  int found = 0;

  if (option (OPTSDEFAULTDECRYPTKEY) && SmimeDefaultKey && *SmimeDefaultKey)
  {
    snprintf (SmimeKeyToUse, sizeof (SmimeKeyToUse), "%s/%s", 
	      NONULL (SmimeKeys), SmimeDefaultKey);
    
    snprintf (SmimeCertToUse, sizeof (SmimeCertToUse), "%s/%s",
	      NONULL(SmimeCertificates), SmimeDefaultKey);

    return;
  }

  for (t = env->to; !found && t; t = t->next)
    if (mutt_addr_is_user (t))
    {
      found = 1;
      _smime_getkeys (t->mailbox);
    }
  for (t = env->cc; !found && t; t = t->next)
    if (mutt_addr_is_user (t))
    {
      found = 1;
      _smime_getkeys (t->mailbox);
    }
  if (!found && (t = mutt_default_from()))
  {
    _smime_getkeys (t->mailbox);
    rfc822_free_address (&t);
  }
}

/* This routine attempts to find the keyids of the recipients of a message.
 * It returns NULL if any of the keys can not be found.
 */

char *smime_findKeys (ADDRESS *to, ADDRESS *cc, ADDRESS *bcc)
{
  char *keyID, *keylist = NULL;
  size_t keylist_size = 0;
  size_t keylist_used = 0;
  ADDRESS *tmp = NULL, *addr = NULL;
  ADDRESS **last = &tmp;
  ADDRESS *p, *q;
  int i;

  const char *fqdn = mutt_fqdn (1);
  
  for (i = 0; i < 3; i++)
  {
    switch (i)
    {
      case 0: p = to; break;
      case 1: p = cc; break;
      case 2: p = bcc; break;
      default: abort ();
    }
    
    *last = rfc822_cpy_adr (p, 0);
    while (*last)
      last = &((*last)->next);
  }

  if (fqdn)
    rfc822_qualify (tmp, fqdn);

  tmp = mutt_remove_duplicates (tmp);
  
  for (p = tmp; p ; p = p->next)
  {
    char buf[LONG_STRING];

    q = p;

    if ((keyID = smime_get_field_from_db (q->mailbox, NULL, 1, 1)) == NULL)
    {
      snprintf(buf, sizeof(buf),
	       _("Enter keyID for %s: "),
	       q->mailbox);
      keyID = smime_ask_for_key(buf, q->mailbox, 1);
    }
    if(!keyID)
    {
      mutt_message (_("No (valid) certificate found for %s."), q->mailbox);
      FREE (&keylist);
      rfc822_free_address (&tmp);
      rfc822_free_address (&addr);
      return NULL;
    }
    
    keylist_size += mutt_strlen (keyID) + 2;
    safe_realloc (&keylist, keylist_size);
    sprintf (keylist + keylist_used, "%s\n", keyID);	/* __SPRINTF_CHECKED__ */
    keylist_used = mutt_strlen (keylist);

    rfc822_free_address (&addr);

  }
  rfc822_free_address (&tmp);
  return (keylist);
}






static int smime_handle_cert_email (char *certificate, char *mailbox,
				   int copy, char ***buffer, int *num)
{
  FILE *fpout = NULL, *fperr = NULL;
  char tmpfname[_POSIX_PATH_MAX];
  char email[STRING];
  int ret = -1, count = 0;
  pid_t thepid;

  mutt_mktemp (tmpfname);
  if ((fperr = safe_fopen (tmpfname, "w+")) == NULL)
  {
    mutt_perror (tmpfname);
    return 1;
  }
  mutt_unlink (tmpfname);

  mutt_mktemp (tmpfname);
  if ((fpout = safe_fopen (tmpfname, "w+")) == NULL)
  {
    safe_fclose (&fperr);
    mutt_perror (tmpfname);
    return 1;
  }
  mutt_unlink (tmpfname);

  if ((thepid =  smime_invoke (NULL, NULL, NULL,
			       -1, fileno (fpout), fileno (fperr),
			       certificate, NULL, NULL, NULL, NULL, NULL,
			       SmimeGetCertEmailCommand))== -1)
  {
    mutt_message (_("Error: unable to create OpenSSL subprocess!"));
    safe_fclose (&fperr);
    safe_fclose (&fpout);
    return 1;
  }

  mutt_wait_filter (thepid);

  fflush (fpout);
  rewind (fpout);
  fflush (fperr);
  rewind (fperr);


  while ((fgets (email, sizeof (email), fpout)))
  {
    *(email + mutt_strlen (email)-1) = '\0';
    if(mutt_strncasecmp (email, mailbox, mutt_strlen (mailbox)) == 0)
      ret=1;

    ret = ret < 0 ? 0 : ret;
    count++;
  }

  if (ret == -1)
  {
    mutt_endwin(NULL);
    mutt_copy_stream (fperr, stdout);
    mutt_any_key_to_continue (_("Error: unable to create OpenSSL subprocess!"));
    ret = 1;
  }
  else if (!ret)
    ret = 1;
  else ret = 0;

  if(copy && buffer && num)
  {
    (*num) = count;
    *buffer =  safe_calloc(sizeof(char*), count);
    count = 0;

    rewind (fpout);
    while ((fgets (email, sizeof (email), fpout)))
    {
      *(email + mutt_strlen (email) - 1) = '\0';
      (*buffer)[count] = safe_calloc(1, mutt_strlen (email) + 1);
      strncpy((*buffer)[count], email, mutt_strlen (email));
      count++;
    }
  }
  else if(copy) ret = 2;

  safe_fclose (&fpout);
  safe_fclose (&fperr);

  return ret;
}



static char *smime_extract_certificate (char *infile)
{
  FILE *fpout = NULL, *fperr = NULL;
  char pk7out[_POSIX_PATH_MAX], certfile[_POSIX_PATH_MAX];
  char tmpfname[_POSIX_PATH_MAX];
  pid_t thepid;
  int empty;


  mutt_mktemp (tmpfname);
  if ((fperr = safe_fopen (tmpfname, "w+")) == NULL)
  {
    mutt_perror (tmpfname);
    return NULL;
  }
  mutt_unlink (tmpfname);

  mutt_mktemp (pk7out);
  if ((fpout = safe_fopen (pk7out, "w+")) == NULL)
  {
    safe_fclose (&fperr);
    mutt_perror (pk7out);
    return NULL;
  }

  /* Step 1: Convert the signature to a PKCS#7 structure, as we can't
     extract the full set of certificates directly.
  */
  if ((thepid =  smime_invoke (NULL, NULL, NULL,
			       -1, fileno (fpout), fileno (fperr),
			       infile, NULL, NULL, NULL, NULL, NULL,
			       SmimePk7outCommand))== -1)
  {
    mutt_any_key_to_continue (_("Error: unable to create OpenSSL subprocess!"));
    safe_fclose (&fperr);
    safe_fclose (&fpout);
    mutt_unlink (pk7out);
    return NULL;
  }

  mutt_wait_filter (thepid);


  fflush (fpout);
  rewind (fpout);
  fflush (fperr);
  rewind (fperr);
  empty = (fgetc (fpout) == EOF);
  if (empty)
  {
    mutt_perror (pk7out);
    mutt_copy_stream (fperr, stdout);
    safe_fclose (&fpout);
    safe_fclose (&fperr);
    mutt_unlink (pk7out);
    return NULL;
    
  }


  safe_fclose (&fpout);
  mutt_mktemp (certfile);
  if ((fpout = safe_fopen (certfile, "w+")) == NULL)
  {
    safe_fclose (&fperr);
    mutt_unlink (pk7out);
    mutt_perror (certfile);
    return NULL;
  }
  
  /* Step 2: Extract the certificates from a PKCS#7 structure.
   */
  if ((thepid =  smime_invoke (NULL, NULL, NULL,
			       -1, fileno (fpout), fileno (fperr),
			       pk7out, NULL, NULL, NULL, NULL, NULL,
			       SmimeGetCertCommand))== -1)
  {
    mutt_any_key_to_continue (_("Error: unable to create OpenSSL subprocess!"));
    safe_fclose (&fperr);
    safe_fclose (&fpout);
    mutt_unlink (pk7out);
    mutt_unlink (certfile);
    return NULL;
  }

  mutt_wait_filter (thepid);

  mutt_unlink (pk7out);

  fflush (fpout);
  rewind (fpout);
  fflush (fperr);
  rewind (fperr);
  empty =  (fgetc (fpout) == EOF);
  if (empty)
  {
    mutt_copy_stream (fperr, stdout);
    safe_fclose (&fpout);
    safe_fclose (&fperr);
    mutt_unlink (certfile);
    return NULL;
  }

  safe_fclose (&fpout);
  safe_fclose (&fperr);

  return safe_strdup (certfile);
}

static char *smime_extract_signer_certificate (char *infile)
{
  FILE *fpout = NULL, *fperr = NULL;
  char pk7out[_POSIX_PATH_MAX], certfile[_POSIX_PATH_MAX];
  char tmpfname[_POSIX_PATH_MAX];
  pid_t thepid;
  int empty;


  mutt_mktemp (tmpfname);
  if ((fperr = safe_fopen (tmpfname, "w+")) == NULL)
  {
    mutt_perror (tmpfname);
    return NULL;
  }
  mutt_unlink (tmpfname);


  mutt_mktemp (certfile);
  if ((fpout = safe_fopen (certfile, "w+")) == NULL)
  {
    safe_fclose (&fperr);
    mutt_perror (certfile);
    return NULL;
  }
  
  /* Extract signer's certificate
   */
  if ((thepid =  smime_invoke (NULL, NULL, NULL,
			       -1, -1, fileno (fperr),
			       infile, NULL, NULL, NULL, certfile, NULL,
			       SmimeGetSignerCertCommand))== -1)
  {
    mutt_any_key_to_continue (_("Error: unable to create OpenSSL subprocess!"));
    safe_fclose (&fperr);
    safe_fclose (&fpout);
    mutt_unlink (pk7out);
    mutt_unlink (certfile);
    return NULL;
  }

  mutt_wait_filter (thepid);

  fflush (fpout);
  rewind (fpout);
  fflush (fperr);
  rewind (fperr);
  empty =  (fgetc (fpout) == EOF);
  if (empty)
  {
    mutt_endwin (NULL);
    mutt_copy_stream (fperr, stdout);
    mutt_any_key_to_continue (NULL);
    safe_fclose (&fpout);
    safe_fclose (&fperr);
    mutt_unlink (certfile);
    return NULL;
  }

  safe_fclose (&fpout);
  safe_fclose (&fperr);

  return safe_strdup (certfile);
}




/* Add a certificate and update index file (externally). */

void smime_invoke_import (char *infile, char *mailbox)
{
  char tmpfname[_POSIX_PATH_MAX], *certfile = NULL, buf[STRING];
  FILE *smimein=NULL, *fpout = NULL, *fperr = NULL;
  pid_t thepid=-1;

  mutt_mktemp (tmpfname);
  if ((fperr = safe_fopen (tmpfname, "w+")) == NULL)
  {
    mutt_perror (tmpfname);
    return;
  }
  mutt_unlink (tmpfname);

  mutt_mktemp (tmpfname);
  if ((fpout = safe_fopen (tmpfname, "w+")) == NULL)
  {
    safe_fclose (&fperr);
    mutt_perror (tmpfname);
    return;
  }
  mutt_unlink (tmpfname);


  buf[0] = '\0';
  if (option (OPTASKCERTLABEL))
    mutt_get_field ("Label for certificate:", buf, sizeof (buf), 0);

  mutt_endwin (NULL);
  if ((certfile = smime_extract_certificate(infile)))
  {
    mutt_endwin (NULL);
  
    if ((thepid =  smime_invoke (&smimein, NULL, NULL,
				 -1, fileno(fpout), fileno(fperr),
				 certfile, NULL, NULL, NULL, NULL, NULL,
				 SmimeImportCertCommand))== -1)
    {
      mutt_message (_("Error: unable to create OpenSSL subprocess!"));
      return;
    }
    fputs (buf, smimein);
    fputc ('\n', smimein);
    safe_fclose (&smimein);

    mutt_wait_filter (thepid);
  
    mutt_unlink (certfile);
    FREE (&certfile);
  }

  fflush (fpout);
  rewind (fpout);
  fflush (fperr);
  rewind (fperr);

  mutt_copy_stream (fpout, stdout);
  mutt_copy_stream (fperr, stdout);

  safe_fclose (&fpout);
  safe_fclose (&fperr);

}



int smime_verify_sender(HEADER *h)
{
  char *mbox = NULL, *certfile, tempfname[_POSIX_PATH_MAX];
  FILE *fpout;
  int retval=1;

  mutt_mktemp (tempfname);
  if (!(fpout = safe_fopen (tempfname, "w")))
  {
    mutt_perror (tempfname);
    return 1;
  }

  if(h->security & ENCRYPT)
    mutt_copy_message (fpout, Context, h,
		       M_CM_DECODE_CRYPT & M_CM_DECODE_SMIME,
		       CH_MIME|CH_WEED|CH_NONEWLINE);
  else
    mutt_copy_message (fpout, Context, h, 0, 0);

  fflush(fpout);
  safe_fclose (&fpout);

  if (h->env->from)
  {
    h->env->from = mutt_expand_aliases (h->env->from); 
    mbox = h->env->from->mailbox; 
  }
  else if (h->env->sender)
  {
    h->env->sender = mutt_expand_aliases (h->env->sender); 
    mbox = h->env->sender->mailbox; 
  }

  if (mbox)
  {
    if ((certfile = smime_extract_signer_certificate(tempfname)))
    {
      mutt_unlink(tempfname);
      if (smime_handle_cert_email (certfile, mbox, 0, NULL, NULL))
      {
	if(isendwin())
         mutt_any_key_to_continue(NULL);
      }
      else
	retval = 0;
      mutt_unlink(certfile);
      FREE (&certfile);
    }
  else 
	mutt_any_key_to_continue(_("no certfile"));
  }
  else 
	mutt_any_key_to_continue(_("no mbox"));

  mutt_unlink(tempfname);
  return retval;
}









/*
 *    Creating S/MIME - bodies.
 */




static
pid_t smime_invoke_encrypt (FILE **smimein, FILE **smimeout, FILE **smimeerr,
			    int smimeinfd, int smimeoutfd, int smimeerrfd,
			    const char *fname, const char *uids)
{
  return smime_invoke (smimein, smimeout, smimeerr,
		       smimeinfd, smimeoutfd, smimeerrfd,
		       fname, NULL, SmimeCryptAlg, NULL, uids, NULL,
		       SmimeEncryptCommand);
}


static
pid_t smime_invoke_sign (FILE **smimein, FILE **smimeout, FILE **smimeerr,
			 int smimeinfd, int smimeoutfd, int smimeerrfd, 
			 const char *fname)
{
  return smime_invoke (smimein, smimeout, smimeerr, smimeinfd, smimeoutfd,
		       smimeerrfd, fname, NULL, NULL, SmimeKeyToUse,
		       SmimeCertToUse, SmimeIntermediateToUse,
		       SmimeSignCommand);
}




BODY *smime_build_smime_entity (BODY *a, char *certlist)
{
  char buf[LONG_STRING], certfile[LONG_STRING];
  char tempfile[_POSIX_PATH_MAX], smimeerrfile[_POSIX_PATH_MAX];
  char smimeinfile[_POSIX_PATH_MAX];
  char *cert_start = certlist, *cert_end = certlist;
  FILE *smimein = NULL, *smimeerr = NULL, *fpout = NULL, *fptmp = NULL;
  BODY *t;
  int err = 0, empty;
  pid_t thepid;
  
  mutt_mktemp (tempfile);
  if ((fpout = safe_fopen (tempfile, "w+")) == NULL)
  {
    mutt_perror (tempfile);
    return (NULL);
  }

  mutt_mktemp (smimeerrfile);
  if ((smimeerr = safe_fopen (smimeerrfile, "w+")) == NULL)
  {
    mutt_perror (smimeerrfile);
    safe_fclose (&fpout);
    mutt_unlink (tempfile);
    return NULL;
  }
  mutt_unlink (smimeerrfile);
  
  mutt_mktemp (smimeinfile);
  if ((fptmp = safe_fopen (smimeinfile, "w+")) == NULL)
  {
    mutt_perror (smimeinfile);
    mutt_unlink (tempfile);
    safe_fclose (&fpout);
    safe_fclose (&smimeerr);
    return NULL;
  }

  *certfile = '\0';
  while (1)
  {
    int off = mutt_strlen (certfile);
    while (*++cert_end && *cert_end != '\n');
    if (!*cert_end) break;
    *cert_end = '\0';
    snprintf (certfile+off, sizeof (certfile)-off, " %s/%s",
	      NONULL(SmimeCertificates), cert_start);
    *cert_end = '\n';
    cert_start = cert_end;
    cert_start++;
  }

  /* write a MIME entity */
  mutt_write_mime_header (a, fptmp);
  fputc ('\n', fptmp);
  mutt_write_mime_body (a, fptmp);
  safe_fclose (&fptmp);

  if ((thepid =
       smime_invoke_encrypt (&smimein, NULL, NULL, -1,
			     fileno (fpout), fileno (smimeerr),
			     smimeinfile, certfile)) == -1)
  {
    safe_fclose (&smimeerr);
    mutt_unlink (smimeinfile);
    mutt_unlink (certfile);
    return (NULL);
  }

  safe_fclose (&smimein);
  
  mutt_wait_filter (thepid);
  mutt_unlink (smimeinfile);
  mutt_unlink (certfile);
  
  fflush (fpout);
  rewind (fpout);
  empty = (fgetc (fpout) == EOF);
  safe_fclose (&fpout);
 
  fflush (smimeerr);
  rewind (smimeerr);
  while (fgets (buf, sizeof (buf) - 1, smimeerr) != NULL)
  {
    err = 1;
    fputs (buf, stdout);
  }
  safe_fclose (&smimeerr);

  /* pause if there is any error output from SMIME */
  if (err)
    mutt_any_key_to_continue (NULL);

  if (empty)
  {
    /* fatal error while trying to encrypt message */
    if (!err) mutt_any_key_to_continue _("No output from OpenSSL..");
    mutt_unlink (tempfile);
    return (NULL);
  }

  t = mutt_new_body ();
  t->type = TYPEAPPLICATION;
  t->subtype = safe_strdup ("x-pkcs7-mime");
  mutt_set_parameter ("name", "smime.p7m", &t->parameter);
  mutt_set_parameter ("smime-type", "enveloped-data", &t->parameter);
  t->encoding = ENCBASE64;  /* The output of OpenSSL SHOULD be binary */
  t->use_disp = 1;
  t->disposition = DISPATTACH;
  t->d_filename = safe_strdup ("smime.p7m");
  t->filename = safe_strdup (tempfile);
  t->unlink = 1; /*delete after sending the message */
  t->parts=0;
  t->next=0;
  
  return (t);
}




BODY *smime_sign_message (BODY *a )
{
  BODY *t;
  char buffer[LONG_STRING];
  char signedfile[_POSIX_PATH_MAX], filetosign[_POSIX_PATH_MAX];
  FILE *smimein = NULL, *smimeout = NULL, *smimeerr = NULL, *sfp = NULL;
  int err = 0;
  int empty = 0;
  pid_t thepid;
  char *intermediates = smime_get_field_from_db(NULL, SmimeDefaultKey, 1, 1);

  if (!SmimeDefaultKey)
  {
    mutt_error _("Can't sign: No key specified. Use Sign As.");
    FREE (&intermediates);
    return NULL;
  }

  if (!intermediates)
  {
    mutt_message(_("Warning: Intermediate certificate not found."));
    intermediates = SmimeDefaultKey; /* so openssl won't complain in any case */
  }

  convert_to_7bit (a); /* Signed data _must_ be in 7-bit format. */

  mutt_mktemp (filetosign);
  if ((sfp = safe_fopen (filetosign, "w+")) == NULL)
  {
    mutt_perror (filetosign);
    if (intermediates != SmimeDefaultKey)
      FREE (&intermediates);
    return NULL;
  }

  mutt_mktemp (signedfile);
  if ((smimeout = safe_fopen (signedfile, "w+")) == NULL)
  {
    mutt_perror (signedfile);
    safe_fclose (&sfp);
    mutt_unlink (filetosign);
    if (intermediates != SmimeDefaultKey)
      FREE (&intermediates);
    return NULL;
  }
  
  mutt_write_mime_header (a, sfp);
  fputc ('\n', sfp);
  mutt_write_mime_body (a, sfp);
  safe_fclose (&sfp);

  

  snprintf (SmimeKeyToUse, sizeof (SmimeKeyToUse), "%s/%s", 
	   NONULL(SmimeKeys), SmimeDefaultKey);

  snprintf (SmimeCertToUse, sizeof (SmimeCertToUse), "%s/%s",
	   NONULL(SmimeCertificates), SmimeDefaultKey);
  
  snprintf (SmimeIntermediateToUse, sizeof (SmimeIntermediateToUse), "%s/%s",
	   NONULL(SmimeCertificates), intermediates);
  


  if ((thepid = smime_invoke_sign (&smimein, NULL, &smimeerr,
				 -1, fileno (smimeout), -1, filetosign)) == -1)
  {
    mutt_perror _("Can't open OpenSSL subprocess!");
    safe_fclose (&smimeout);
    mutt_unlink (signedfile);
    mutt_unlink (filetosign);
    if (intermediates != SmimeDefaultKey)
      FREE (&intermediates);
    return NULL;
  }
  fputs (SmimePass, smimein);
  fputc ('\n', smimein);
  safe_fclose (&smimein);
  

  mutt_wait_filter (thepid);

  /* check for errors from OpenSSL */
  err = 0;
  fflush (smimeerr);
  rewind (smimeerr);
  while (fgets (buffer, sizeof (buffer) - 1, smimeerr) != NULL)
  {
    err = 1;
    fputs (buffer, stdout);
  }
  safe_fclose (&smimeerr);


  fflush (smimeout);
  rewind (smimeout);
  empty = (fgetc (smimeout) == EOF);
  safe_fclose (&smimeout);

  mutt_unlink (filetosign);
  

  if (err)
    mutt_any_key_to_continue (NULL);

  if (empty)
  {
    mutt_any_key_to_continue _("No output from OpenSSL...");
    mutt_unlink (signedfile);
    return (NULL); /* fatal error while signing */
  }

  t = mutt_new_body ();
  t->type = TYPEMULTIPART;
  t->subtype = safe_strdup ("signed");
  t->encoding = ENC7BIT;
  t->use_disp = 0;
  t->disposition = DISPINLINE;

  mutt_generate_boundary (&t->parameter);
  /* check if this can be extracted from private key somehow.... */
  mutt_set_parameter ("micalg", "sha1", &t->parameter);
  mutt_set_parameter ("protocol", "application/x-pkcs7-signature",
		     &t->parameter);

  t->parts = a;
  a = t;

  t->parts->next = mutt_new_body ();
  t = t->parts->next;
  t->type = TYPEAPPLICATION;
  t->subtype = safe_strdup ("x-pkcs7-signature");
  t->filename = safe_strdup (signedfile);
  t->d_filename = safe_strdup ("smime.p7s");
  t->use_disp = 1;
  t->disposition = DISPATTACH;
  t->encoding = ENCBASE64;
  t->unlink = 1; /* ok to remove this file after sending. */

  return (a);

}






/*
 *    Handling S/MIME - bodies.
 */






static
pid_t smime_invoke_verify (FILE **smimein, FILE **smimeout, FILE **smimeerr,
			   int smimeinfd, int smimeoutfd, int smimeerrfd, 
			   const char *fname, const char *sig_fname, int opaque)
{
  return smime_invoke (smimein, smimeout, smimeerr, smimeinfd, smimeoutfd,
		       smimeerrfd, fname, sig_fname, NULL, NULL, NULL, NULL,
		       (opaque ? SmimeVerifyOpaqueCommand : SmimeVerifyCommand));
}


static
pid_t smime_invoke_decrypt (FILE **smimein, FILE **smimeout, FILE **smimeerr,
			    int smimeinfd, int smimeoutfd, int smimeerrfd, 
			    const char *fname)
{
  return smime_invoke (smimein, smimeout, smimeerr, smimeinfd, smimeoutfd,
		       smimeerrfd, fname, NULL, NULL, SmimeKeyToUse,
		       SmimeCertToUse, NULL, SmimeDecryptCommand);
}



int smime_verify_one (BODY *sigbdy, STATE *s, const char *tempfile)
{
  char signedfile[_POSIX_PATH_MAX], smimeerrfile[_POSIX_PATH_MAX];
  FILE *fp=NULL, *smimeout=NULL, *smimeerr=NULL;
  pid_t thepid;
  int badsig = -1;

  long tmpoffset = 0;
  size_t tmplength = 0;
  int origType = sigbdy->type;
  char *savePrefix = NULL;


  snprintf (signedfile, sizeof (signedfile), "%s.sig", tempfile);
  
  /* decode to a tempfile, saving the original destination */
  fp = s->fpout;
  if ((s->fpout = safe_fopen (signedfile, "w")) == NULL)
  {
    mutt_perror (signedfile);
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
  savePrefix = s->prefix;
  s->prefix = NULL;

  mutt_decode_attachment (sigbdy, s);

  sigbdy->length = ftello (s->fpout);
  sigbdy->offset = 0;
  safe_fclose (&s->fpout);

  /* restore final destination and substitute the tempfile for input */
  s->fpout = fp;
  fp = s->fpin;
  s->fpin = fopen (signedfile, "r");

  /* restore the prefix */
  s->prefix = savePrefix;
  
  sigbdy->type = origType;

  
  mutt_mktemp (smimeerrfile);
  if (!(smimeerr = safe_fopen (smimeerrfile, "w+")))
  {
    mutt_perror (smimeerrfile);
    mutt_unlink (signedfile);
    return -1;
  }
  
  crypt_current_time (s, "OpenSSL");
  
  if ((thepid = smime_invoke_verify (NULL, &smimeout, NULL, 
				   -1, -1, fileno (smimeerr),
				   tempfile, signedfile, 0)) != -1)
  {
    fflush (smimeout);
    safe_fclose (&smimeout);
      
    if (mutt_wait_filter (thepid))
      badsig = -1;
    else
    {
      char *line = NULL;
      int lineno = 0;
      size_t linelen;
      
      fflush (smimeerr);
      rewind (smimeerr);
      
      line = mutt_read_line (line, &linelen, smimeerr, &lineno, 0);
      if (linelen && !ascii_strcasecmp (line, "verification successful"))
	badsig = 0;

      FREE (&line);
    }
  }
  
  fflush (smimeerr);
  rewind (smimeerr);
  mutt_copy_stream (smimeerr, s->fpout);
  safe_fclose (&smimeerr);
    
  state_attach_puts (_("[-- End of OpenSSL output --]\n\n"), s);
  
  mutt_unlink (signedfile);
  mutt_unlink (smimeerrfile);

  sigbdy->length = tmplength;
  sigbdy->offset = tmpoffset;
  
  /* restore the original source stream */
  safe_fclose (&s->fpin);
  s->fpin = fp;
  

  return badsig;
}





/*
  This handles application/pkcs7-mime which can either be a signed
  or an encrypted message.
*/

static BODY *smime_handle_entity (BODY *m, STATE *s, FILE *outFile)
{
  int len=0;
  int c;
  long last_pos;
  char buf[HUGE_STRING];
  char outfile[_POSIX_PATH_MAX], errfile[_POSIX_PATH_MAX];
  char tmpfname[_POSIX_PATH_MAX];
  char tmptmpfname[_POSIX_PATH_MAX];
  FILE *smimeout = NULL, *smimein=NULL, *smimeerr=NULL;
  FILE *tmpfp=NULL, *tmpfp_buffer=NULL, *fpout=NULL;
  struct stat info;
  BODY *p=NULL;
  pid_t thepid=-1;
  unsigned int type = mutt_is_application_smime (m);

  if (!(type & APPLICATION_SMIME)) return NULL;

  mutt_mktemp (outfile);
  if ((smimeout = safe_fopen (outfile, "w+")) == NULL)
  {
    mutt_perror (outfile);
    return NULL;
  }
  
  mutt_mktemp (errfile);
  if ((smimeerr = safe_fopen (errfile, "w+")) == NULL)
  {
    mutt_perror (errfile);
    safe_fclose (&smimeout); smimeout = NULL;
    return NULL;
  }
  mutt_unlink (errfile);

  
  mutt_mktemp (tmpfname);
  if ((tmpfp = safe_fopen (tmpfname, "w+")) == NULL)
  {
    mutt_perror (tmpfname);
    safe_fclose (&smimeout); smimeout = NULL;
    safe_fclose (&smimeerr); smimeerr = NULL;
    return NULL;
  }

  fseeko (s->fpin, m->offset, 0);
  last_pos = m->offset;

  mutt_copy_bytes (s->fpin, tmpfp,  m->length);

  fflush (tmpfp);
  safe_fclose (&tmpfp);

  if ((type & ENCRYPT) &&
      (thepid = smime_invoke_decrypt (&smimein, NULL, NULL, -1,
				      fileno (smimeout),  fileno (smimeerr), tmpfname)) == -1)
  {
    safe_fclose (&smimeout); smimeout = NULL;
    mutt_unlink (tmpfname);
    if (s->flags & M_DISPLAY)
      state_attach_puts (_("[-- Error: unable to create OpenSSL subprocess! --]\n"), s);
    return NULL;
  }
  else if ((type & SIGNOPAQUE) &&
	   (thepid = smime_invoke_verify (&smimein, NULL, NULL, -1,
					  fileno (smimeout), fileno (smimeerr), NULL,
					  tmpfname, SIGNOPAQUE)) == -1)
  {
    safe_fclose (&smimeout); smimeout = NULL;
    mutt_unlink (tmpfname);
    if (s->flags & M_DISPLAY)
      state_attach_puts (_("[-- Error: unable to create OpenSSL subprocess! --]\n"), s);
    return NULL;
  }

  
  if (type & ENCRYPT)
  {
    if (!smime_valid_passphrase ())
      smime_void_passphrase ();
    fputs (SmimePass, smimein);
    fputc ('\n', smimein);
  }

  safe_fclose (&smimein);
	
  mutt_wait_filter (thepid);
  mutt_unlink (tmpfname);
  

  if (s->flags & M_DISPLAY)
  {
    fflush (smimeerr);
    rewind (smimeerr);
    
    if ((c = fgetc (smimeerr)) != EOF)
    {
      ungetc (c, smimeerr);
      
      crypt_current_time (s, "OpenSSL");
      mutt_copy_stream (smimeerr, s->fpout);
      state_attach_puts (_("[-- End of OpenSSL output --]\n\n"), s);
    }
    
    if (type & ENCRYPT)
      state_attach_puts (_("[-- The following data is S/MIME"
                           " encrypted --]\n"), s);
    else
      state_attach_puts (_("[-- The following data is S/MIME signed --]\n"), s);
  }

  if (smimeout)
  {
    fflush (smimeout);
    rewind (smimeout);
    
    if (outFile) fpout = outFile;
    else
    {
      mutt_mktemp (tmptmpfname);
      if ((fpout = safe_fopen (tmptmpfname, "w+")) == NULL)
      {
	mutt_perror(tmptmpfname);
	safe_fclose (&smimeout); smimeout = NULL;
	return NULL;
      }
    }
    while (fgets (buf, sizeof (buf) - 1, smimeout) != NULL)
    {
      len = mutt_strlen (buf);
      if (len > 1 && buf[len - 2] == '\r')
      {
	buf[len-2] = '\n';
	buf[len-1] = '\0';
      }
      fputs (buf, fpout);
    }
    fflush (fpout);
    rewind (fpout); 


    if ((p = mutt_read_mime_header (fpout, 0)) != NULL)
    {
      fstat (fileno (fpout), &info);
      p->length = info.st_size - p->offset;
	  
      mutt_parse_part (fpout, p);
      if (s->fpout)
      {
	rewind (fpout);
	tmpfp_buffer = s->fpin;
	s->fpin = fpout;
	mutt_body_handler (p, s);
	s->fpin = tmpfp_buffer;
      }
      
    }
    safe_fclose (&smimeout);
    smimeout = NULL;
    mutt_unlink (outfile);

    if (!outFile)
    {
      safe_fclose (&fpout);
      mutt_unlink (tmptmpfname);
    }
    fpout = NULL;
  }

  if (s->flags & M_DISPLAY)
  {
    if (type & ENCRYPT)
      state_attach_puts (_("\n[-- End of S/MIME encrypted data. --]\n"), s);
    else
      state_attach_puts (_("\n[-- End of S/MIME signed data. --]\n"), s);
  }

  if (type & SIGNOPAQUE)
  {
    char *line = NULL;
    int lineno = 0;
    size_t linelen;
    
    rewind (smimeerr);
    
    line = mutt_read_line (line, &linelen, smimeerr, &lineno, 0);
    if (linelen && !ascii_strcasecmp (line, "verification successful"))
      m->goodsig = 1;
    FREE (&line);
  }
  else 
  {
    m->goodsig = p->goodsig;
    m->badsig  = p->badsig;
  }
  safe_fclose (&smimeerr);

  return (p);
}





int smime_decrypt_mime (FILE *fpin, FILE **fpout, BODY *b, BODY **cur)
{


  char tempfile[_POSIX_PATH_MAX];
  STATE s;
  long tmpoffset = b->offset;
  size_t tmplength = b->length;
  int origType = b->type;
  FILE *tmpfp=NULL;
  int rv = 0;

  if (!mutt_is_application_smime (b))
    return -1;

  if (b->parts)
    return -1;
  
  memset (&s, 0, sizeof (s));
  s.fpin = fpin;
  fseeko (s.fpin, b->offset, 0);

  mutt_mktemp (tempfile);
  if ((tmpfp = safe_fopen (tempfile, "w+")) == NULL)
  {
    mutt_perror (tempfile);
    return (-1);
  }

  mutt_unlink (tempfile);
  s.fpout = tmpfp;
  mutt_decode_attachment (b, &s);
  fflush (tmpfp);
  b->length = ftello (s.fpout);
  b->offset = 0;
  rewind (tmpfp);
  s.fpin = tmpfp;
  s.fpout = 0;

  mutt_mktemp (tempfile);
  if ((*fpout = safe_fopen (tempfile, "w+")) == NULL)
  {
    mutt_perror (tempfile);
    rv = -1;
    goto bail;
  }
  mutt_unlink (tempfile);

  if (!(*cur = smime_handle_entity (b, &s, *fpout)))
  {
    rv = -1;
    goto bail;
  }
    
  (*cur)->goodsig = b->goodsig;
  (*cur)->badsig  = b->badsig;

bail:
  b->type = origType;
  b->length = tmplength;
  b->offset = tmpoffset;
  safe_fclose (&tmpfp);
  if (*fpout)
    rewind (*fpout);
  
  return rv;
}


int smime_application_smime_handler (BODY *m, STATE *s)
{
  return smime_handle_entity (m, s, NULL) ? 0 : -1;
}

int smime_send_menu (HEADER *msg, int *redraw)
{
  char *p;

  if (!(WithCrypto & APPLICATION_SMIME))
    return msg->security;

  switch (mutt_multi_choice (_("S/MIME (e)ncrypt, (s)ign, encrypt (w)ith, sign (a)s, (b)oth, or (c)lear? "),
			     _("eswabfc")))
  {
  case 1: /* (e)ncrypt */
    msg->security |= ENCRYPT;
    msg->security &= ~SIGN;
    break;

  case 3: /* encrypt (w)ith */
    {
      int choice = 0;

      msg->security |= ENCRYPT;
      do
      {
        /* I use "dra" because "123" is recognized anyway */
        switch (mutt_multi_choice (_("Choose algorithm family:"
                                     " 1: DES, 2: RC2, 3: AES,"
                                     " or (c)lear? "),
                                   _("drac")))
        {
        case 1:
          switch (choice = mutt_multi_choice (_("1: DES, 2: Triple-DES "),
                                              _("dt")))
          {
          case 1:
            mutt_str_replace (&SmimeCryptAlg, "des");
            break;
          case 2:
            mutt_str_replace (&SmimeCryptAlg, "des3");
            break;
          }
          break;

        case 2:
          switch (choice = mutt_multi_choice (_("1: RC2-40, 2: RC2-64, 3: RC2-128 "),
                                              _("468")))
          {
          case 1:
            mutt_str_replace (&SmimeCryptAlg, "rc2-40");
            break;
          case 2:
            mutt_str_replace (&SmimeCryptAlg, "rc2-64");
            break;
          case 3:
            mutt_str_replace (&SmimeCryptAlg, "rc2-128");
            break;
          }
          break;

        case 3:
          switch (choice = mutt_multi_choice (_("1: AES128, 2: AES192, 3: AES256 "),
                                              _("895")))
          {
          case 1:
            mutt_str_replace (&SmimeCryptAlg, "aes128");
            break;
          case 2:
            mutt_str_replace (&SmimeCryptAlg, "aes192");
            break;
          case 3:
            mutt_str_replace (&SmimeCryptAlg, "aes256");
            break;
          }
          break;

        case 4: /* (c)lear */
          FREE (&SmimeCryptAlg);
          /* fallback */
        case -1: /* Ctrl-G or Enter */
          choice = 0;
          break;
        }
      } while (choice == -1);
    }
    break;

  case 2: /* (s)ign */
      
    if(!SmimeDefaultKey)
    {
      *redraw = REDRAW_FULL;

      if ((p = smime_ask_for_key (_("Sign as: "), NULL, 0)))
        mutt_str_replace (&SmimeDefaultKey, p);
      else
        break;
    }

    msg->security |= SIGN;
    msg->security &= ~ENCRYPT;
    break;

  case 4: /* sign (a)s */

    if ((p = smime_ask_for_key (_("Sign as: "), NULL, 0))) 
    {
      mutt_str_replace (&SmimeDefaultKey, p);
	
      msg->security |= SIGN;

      /* probably need a different passphrase */
      crypt_smime_void_passphrase ();
    }
#if 0
    else
      msg->security &= ~SIGN;
#endif

    *redraw = REDRAW_FULL;
    break;

  case 5: /* (b)oth */
    msg->security |= (ENCRYPT | SIGN);
    break;

  case 6: /* (f)orget it */
  case 7: /* (c)lear */
    msg->security = 0;
    break;
  }

  if (msg->security && msg->security != APPLICATION_SMIME)
    msg->security |= APPLICATION_SMIME;
  else
    msg->security = 0;

  return (msg->security);
}


#endif /* CRYPT_BACKEND_CLASSIC_SMIME */
