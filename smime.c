/*
 * Copyright (C) 2001,2002 Oliver Ehli <elmy@acm.org>
 * Copyright (C) 2002 Mike Schiraldi <raldi@research.netsol.com>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

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

#ifdef HAVE_SMIME

#include "crypt.h"


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



int mutt_is_application_smime (BODY *m)
{
  char *t=NULL;
  int len, complain=0;

  if (m->type & TYPEAPPLICATION && m->subtype)
  {
    if (!mutt_strcasecmp (m->subtype, "x-pkcs7-mime"))
    {
      if ((t = mutt_get_parameter ("smime-type", m->parameter)))
      {
	if (!mutt_strcasecmp (t, "enveloped-data"))
	  return SMIMEENCRYPT;
	else if (!mutt_strcasecmp (t, "signed-data"))
	  return (SMIMESIGN|SMIMEOPAQUE);
	else return 0;
      }
      complain = 1;
    }
    else if (mutt_strcasecmp (m->subtype, "octet-stream"))
      return 0;

    t = mutt_get_parameter ("name", m->parameter);

    if (!t) t = m->d_filename;
    if (!t) t = m->filename;
    if (!t) {
      if (complain)
	mutt_message (_("S/MIME messages with no hints on content are unsupported."));
      return 0;
    }

    /* no .p7c, .p10 support yet. */

    len = mutt_strlen (t) - 4;
    if (len > 0 && *(t+len) == '.')
    {
      len++;
      if (!mutt_strcasecmp ((t+len), "p7m"))
#if 0
       return SMIMEENCRYPT;
#else
      /* Not sure if this is the correct thing to do, but 
         it's required for compatibility with Outlook */
       return (SMIMESIGN|SMIMEOPAQUE);
#endif
      else if (!mutt_strcasecmp ((t+len), "p7s"))
	return (SMIMESIGN|SMIMEOPAQUE);
    }
  }

  return 0;
}






/*
 *     The OpenSSL interface
 */

/* This is almost identical to ppgp's invoking interface. */

static const char *_mutt_fmt_smime_command (char *dest,
					    size_t destlen,
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
    mutt_FormatString (dest, destlen, ifstring, _mutt_fmt_smime_command,
		       data, 0);
  else if (flags & M_FORMAT_OPTIONAL)
    mutt_FormatString (dest, destlen, elsestring, _mutt_fmt_smime_command,
		       data, 0);

  return (src);
}



static void mutt_smime_command (char *d, size_t dlen,
				struct smime_command_context *cctx, const char *fmt)
{
  mutt_FormatString (d, dlen, NONULL(fmt), _mutt_fmt_smime_command,
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
    snprintf(s, l, "  0x%.8X%i %s %-35.35s %s", this.hash, this.suffix, truststate, this.email, this.nick);
  else
    snprintf(s, l, "  0x%.8X%i %-35.35s %s", this.hash, this.suffix, this.email, this.nick);
}





char* smime_ask_for_key (char *prompt, char *mailbox, short public)
{
  char *fname;
  smime_id *Table;
  long cert_num; /* Will contain the number of certificates.
      * To be able to get it, the .index file will be read twice... */
  char index_file[_POSIX_PATH_MAX];
  FILE *index;
  char buf[256];
  char fields[5][STRING];
  int numFields, hash_suffix, done, cur; /* The current entry */
  MUTTMENU* menu;
  unsigned int hash;
  char helpstr[128];
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
  fclose(index);

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
    Table = safe_malloc(sizeof (smime_id) * cert_num);
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
    fclose(index);
  
    /* Make Helpstring */
    helpstr[0] = 0;
    mutt_make_help (buf, sizeof (buf), _("Exit  "), MENU_SMIME, OP_EXIT);
    strcat (helpstr, buf);
    mutt_make_help (buf, sizeof (buf), _("Select  "), MENU_SMIME,
        OP_GENERIC_SELECT_ENTRY);
    strcat (helpstr, buf);
    mutt_make_help (buf, sizeof(buf), _("Help"), MENU_SMIME, OP_HELP);
    strcat (helpstr, buf);
  
    /* Create the menu */
    menu = mutt_new_menu();
    menu->max = cur;
    menu->make_entry = smime_entry;
    menu->menu = MENU_SMIME;
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
      fname = safe_malloc(14); /* Hash + '.' + Suffix + \n + \0 */
      sprintf(fname, "%.8x.%i\n", Table[cur].hash, Table[cur].suffix);
    }
    else fname = NULL;
  
    mutt_menuDestroy (&menu);
    safe_free ((void**)&Table);
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
  int numFields;
  struct stat info;
  char *key=NULL, key_trust_level = 0;
  FILE *fp;

  if(!mailbox && !query) return(NULL);

  addr_len = mailbox ? mutt_strlen (mailbox) : 0;
  query_len = query ? mutt_strlen (query) : 0;

  /* index-file format:
     mailbox certfile label issuer_certfile trust_flags\n

     \n is also copied here, serving as delimitation.
     
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
	    (!fields[4] ||
	     *fields[4] == 'i' || *fields[4] == 'e' || *fields[4] == 'r'))
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
	    safe_free((void **) &key);
	    key = NULL;
	    break;
	  }
	  else if (choice == M_NO) 
	  {
	    ask = 1;
	    continue;
	  }
	  else if (choice == M_YES)
	  {
	    snprintf (key,mutt_strlen(key), fields[1]);
	    ask = 0;
	    break;
	  }
	}
	else
	{
	  key = safe_calloc(1, mutt_strlen(fields[1])+2);
	  if (public) key_trust_level = *fields[4];
	  snprintf(key, mutt_strlen(fields[1])+1, "%s", fields[1]);
	}
	found = 1;
      }
      else if(query)
      {
	numFields = sscanf (buf, "%s %s %s %s %s\n", fields[0], fields[1],
			    fields[2], fields[3], fields[4]);

	/* query = label: return certificate. */
	if (numFields >= 3 && 
	    !(mutt_strncasecmp (query, fields[2], query_len)))
	{
	  ask = 0;
	  key = safe_calloc(1, mutt_strlen(fields[1])+2);
	  snprintf(key, mutt_strlen(fields[1])+1, "%s", fields[1]);
	}
	/* query = certificate: return intermediate certificate. */
	else if (numFields >= 4 && 
		 !(mutt_strncasecmp (query, fields[1], query_len)))
	{
	  ask = 0;
	  key = safe_calloc(1, mutt_strlen(fields[3])+2);
	  snprintf(key, mutt_strlen(fields[3])+1, "%s", fields[3]);
	}
      }

    fclose (fp);

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
      {
	safe_free ((void **) &key);
	key = NULL;
      }
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
	{
	  safe_free ((void **) &key);
	  key = NULL;
	}

      }
      else if (key_trust_level == 'v' )
      {
	mutt_error (_("Warning: You have not yet decided to trust ID %s. (any key to continue)"), key);
	
	/* XXX - bad */
  	mutt_any_key_to_continue ("");
/*  	mutt_any_key_to_continue (prompt); */
      }
    }

  }

  if (key)
  {
    key[mutt_strlen(key)+1] = 0;
    key[mutt_strlen(key)] = '\n';
  }

  return key;
}




/* 
   This sets the '*ToUse' variables for an upcoming decryption, where
   the reuquired key is different from SmimeSignAs.
*/

void _smime_getkeys (char *mailbox)
{
  char *k = smime_get_field_from_db (mailbox, NULL, 0, 0);	/* XXX - or sohuld we ask? */
  char buf[STRING];

  if (!k)
  {
    snprintf(buf, sizeof(buf), _("Enter keyID for %s: "),
	     mailbox);
    k = smime_ask_for_key(buf, mailbox, 0);
  }

  if (k)
  {
    k[mutt_strlen (k)-1] = '\0';
    
    /* the key used last time. */
    if (*SmimeKeyToUse && 
        !mutt_strcasecmp (k, SmimeKeyToUse + mutt_strlen (SmimeKeys)+1))
    {
      safe_free ((void **) &k);
      return;
    }
    else smime_void_passphrase ();

    snprintf (SmimeKeyToUse, sizeof (SmimeKeyToUse), "%s/%s", 
	      NONULL(SmimeKeys), k);
    
    snprintf (SmimeCertToUse, sizeof (SmimeCertToUse), "%s/%s",
	      NONULL(SmimeCertificates), k);

    if (mutt_strcasecmp (k, SmimeSignAs))
    {
      endwin ();
      mutt_clear_error ();
      snprintf (buf, sizeof (buf), _("This message seems to require key"
                                     " %s. (Any key to continue)"), k);
      mutt_any_key_to_continue (buf);
      endwin ();
      smime_void_passphrase ();
    }

    safe_free ((void **) &k);
    return;
  }

  if (*SmimeKeyToUse)
  {
    if (!mutt_strcasecmp (SmimeSignAs, 
                          SmimeKeyToUse + mutt_strlen (SmimeKeys)+1))
      return;

    smime_void_passphrase ();
  }

  snprintf (SmimeKeyToUse, sizeof (SmimeKeyToUse), "%s/%s", 
	    NONULL (SmimeKeys), SmimeSignAs);
  
  snprintf (SmimeCertToUse, sizeof (SmimeCertToUse), "%s/%s",
	    NONULL (SmimeCertificates), SmimeSignAs);
}

void smime_getkeys (ENVELOPE *env)
{
  ADDRESS *t;
  int found = 0;

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
    
    *last = rfc822_cpy_adr (p);
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
      safe_free ((void **)&keylist);
      rfc822_free_address (&tmp);
      rfc822_free_address (&addr);
      return NULL;
    }
    
    keylist_size += mutt_strlen (keyID) + 1;
    safe_realloc ((void **)&keylist, keylist_size);
    sprintf (keylist + keylist_used, "%s", keyID);
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
    fclose (fperr);
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
    fclose (fperr);
    fclose (fpout);
    return 1;
  }

  mutt_wait_filter (thepid);

  fflush (fpout);
  rewind (fpout);
  rewind (fperr);
  fflush (fperr);


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
    mutt_copy_stream (fperr, stdout);
    mutt_endwin(NULL);
    mutt_error (_("Alert: No mailbox specified in certificate.\n"));
    ret = 1;
  }
  else if (!ret)
  {
    mutt_endwin(NULL);
    mutt_error (_("Alert: Certificate does *NOT* belong to \"%s\".\n"), mailbox);
    ret = 1;
  }
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

  fclose (fpout);
  fclose (fperr);

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
    fclose (fperr);
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
    fclose (fperr);
    fclose (fpout);
    mutt_unlink (pk7out);
    return NULL;
  }

  mutt_wait_filter (thepid);


  fflush (fpout);
  rewind (fpout);
  rewind (fperr);
  fflush (fperr);
  empty = (fgetc (fpout) == EOF);
  if (empty)
  {
    mutt_perror (pk7out);
    mutt_copy_stream (fperr, stdout);
    fclose (fpout);
    fclose (fperr);
    mutt_unlink (pk7out);
    return NULL;
    
  }


  fclose (fpout);
  mutt_mktemp (certfile);
  if ((fpout = safe_fopen (certfile, "w+")) == NULL)
  {
    fclose (fperr);
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
    fclose (fperr);
    fclose (fpout);
    mutt_unlink (pk7out);
    mutt_unlink (certfile);
    return NULL;
  }

  mutt_wait_filter (thepid);

  mutt_unlink (pk7out);

  fflush (fpout);
  rewind (fpout);
  rewind (fperr);
  fflush (fperr);
  empty =  (fgetc (fpout) == EOF);
  if (empty)
  {
    mutt_copy_stream (fperr, stdout);
    fclose (fpout);
    fclose (fperr);
    mutt_unlink (certfile);
    return NULL;
  }

  fclose (fpout);
  fclose (fperr);

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
    fclose (fperr);
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
    fclose (fperr);
    fclose (fpout);
    mutt_unlink (pk7out);
    mutt_unlink (certfile);
    return NULL;
  }

  mutt_wait_filter (thepid);

  fflush (fpout);
  rewind (fpout);
  rewind (fperr);
  fflush (fperr);
  empty =  (fgetc (fpout) == EOF);
  if (empty)
  {
    mutt_endwin (NULL);
    mutt_copy_stream (fperr, stdout);
    mutt_any_key_to_continue (NULL);
    fclose (fpout);
    fclose (fperr);
    mutt_unlink (certfile);
    return NULL;
  }

  fclose (fpout);
  fclose (fperr);

  return safe_strdup (certfile);
}



static int smime_compare_fingerprint (char *certificate, char *hashval, char *dest)
{
  FILE *fpout = NULL, *fperr = NULL;
  char tmpfname[_POSIX_PATH_MAX];
  char md5New[STRING], md5Old[STRING];
  pid_t thepid;

  mutt_mktemp (tmpfname);
  if ((fperr = safe_fopen (tmpfname, "w+")) == NULL)
  {
    mutt_perror (tmpfname);
    return -1;
  }
  mutt_unlink (tmpfname);

  mutt_mktemp (tmpfname);
  if ((fpout = safe_fopen (tmpfname, "w+")) == NULL)
  {
    fclose (fperr);
    mutt_perror (tmpfname);
    return -1;
  }
  mutt_unlink (tmpfname);

  /* fingerprint the certificate to add */
  if ((thepid =  smime_invoke (NULL, NULL, NULL,
			       -1, fileno (fpout), fileno (fperr),
			       certificate, NULL, NULL, NULL, NULL, NULL,
			       SmimeFingerprintCertCommand))== -1)
  {
    mutt_message (_("Error: unable to create OpenSSL subprocess!"));
    fclose (fperr);
    fclose (fpout);
    return -1;
  }

  mutt_wait_filter (thepid);
  
  fflush (fpout);
  rewind (fpout);
  fflush (fperr);
  rewind (fperr);

  if (!(fgets (md5New, sizeof (md5New), fpout)))
  {
    mutt_copy_stream (fperr, stdout);
    fclose (fpout);
    fclose (fperr);
    return -1;
  }

  /* fingerprint the certificate already installed */
  if ((thepid =  smime_invoke (NULL, NULL, NULL,
			       -1, fileno (fpout), fileno (fperr),
			       dest, NULL, NULL, NULL, NULL, NULL,
			       SmimeFingerprintCertCommand))== -1)
  {
    mutt_message (_("Error: unable to create OpenSSL subprocess!"));
    fclose (fperr);
    fclose (fpout);
    return -1;
  }

  mutt_wait_filter (thepid);
  
  fflush (fpout);
  rewind (fpout);
  fflush (fperr);
  rewind (fperr);

  if (!(fgets (md5Old, sizeof (md5Old), fpout)))
  {
    mutt_copy_stream (fperr, stdout);
    fclose (fpout);
    fclose (fperr);
    return -1;
  }

  fclose (fpout);
  fclose (fperr);

  return ((mutt_strcasecmp (md5Old, md5New) == 0));
}


/* Add a certificate and update index file. */

static int smime_add_certificate (char *certificate, char *mailbox)
{
  FILE *fpin = NULL, *fpout = NULL, *fperr = NULL;
  char tmpfname[_POSIX_PATH_MAX], dest[_POSIX_PATH_MAX];
  char buf[LONG_STRING], hashval[STRING], *tmpKey;
  struct stat info;
  int i = 0, certExists=0;
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
    fclose (fperr);
    mutt_perror (tmpfname);
    return 1;
  }
  mutt_unlink (tmpfname);

  /* 
     OpenSSl can create a hash value of the certificate's subject.
     This and a concatenated integer make up the certificate's
     'unique id' and also its filename.
  */

  mutt_endwin (NULL);
  
  if ((thepid =  smime_invoke (NULL, NULL, NULL,
			       -1, fileno (fpout), fileno (fperr),
			       certificate, NULL, NULL, NULL, NULL, NULL,
			       SmimeHashCertCommand))== -1)
  {
    mutt_message (_("Error: unable to create OpenSSL subprocess!"));
    fclose (fperr);
    fclose (fpout);
    return 1;
  }

  mutt_wait_filter (thepid);
  
  fflush (fpout);
  rewind (fpout);
  fflush (fperr);
  rewind (fperr);

  if (!(fgets (hashval, sizeof (hashval), fpout)))
  {
    mutt_copy_stream (fperr, stdout);
    fclose (fpout);
    fclose (fperr);
    return 1;
  }
  fclose (fpout);
  fclose (fperr);

  *(hashval+mutt_strlen(hashval)-1) = '\0';

  while (1)
  {
    snprintf (dest, sizeof (dest), "%s/%s.%d", NONULL(SmimeCertificates),
	      hashval, i);

    if (stat (dest, &info))
      break;
    else { /* check wether this certificate already exists. */
      if((certExists = smime_compare_fingerprint (certificate, hashval, dest)) == 1)
      {
	break;
      }
      else
      {
	if(!certExists)
	  i++;
	else  /* some error: abort. */
	  return 1;
      }
    }
  }
  
  if(!certExists)
  {
    if ((fpout = safe_fopen (dest, "w+")) == NULL)
    {
      mutt_perror (dest);
      return 1;
    }

    if ((fpin = safe_fopen (certificate, "r")) == NULL)
    {
      mutt_perror (certificate);
      fclose (fpout);
      mutt_unlink (dest);
      return 1;
    }
    
    mutt_copy_stream (fpin, fpout);
    fclose (fpout);
    fclose (fpin);
  }


  /*
    Now check if the mailbox is already found with the certificate's
    hash value and/or md5 fingerprint.
  */
  
  tmpKey = smime_get_field_from_db (mailbox, NULL, 1, 0); /* _always_ public! */

  /* check if hash is identical => same certificate! */
  /* perhaps we should ask for permission to overwrite ? */
  /* what about revoked certificates anyway ? */

  if (tmpKey && !mutt_strncmp (tmpKey, hashval, mutt_strlen (hashval)))
  {
    mutt_message (_("Certificate \"%s\" exists for \"%s\"."), hashval, mailbox);
    return 0;
  }
    
  /* doesn't exist or is a new one, so append to index. */
  snprintf (tmpfname, sizeof (tmpfname), "%s/.index",
	    NONULL(SmimeCertificates)); /* _always_ public: we don't add keys here */
  
  if (!stat (tmpfname, &info))
  {
    if ((fpout = safe_fopen (tmpfname, "a")) == NULL)
    {
      mutt_perror (tmpfname);
      mutt_unlink (dest);
      return 1;
    }
    /*
       ? = unknown issuer, - = unassigned label,
       v = undefined trust settings (else we wouldn't have got that far).
    */
    snprintf (buf, sizeof (buf), "%s %s.%d - ? u\n", mailbox, hashval, i);
    fputs (buf, fpout);
    fclose (fpout);
    
    mutt_message (_("Successfully added certificate \"%s\" for \"%s\". "), hashval, mailbox);
  }

  return 0;
}



void smime_invoke_import (char *infile, char *mailbox)
{
  char *certfile = NULL, **addrList=0;
  int i, addrCount=0, ret = 0;
  certfile = smime_extract_signer_certificate(infile);
  
  if (certfile)
  { 
    i = smime_handle_cert_email (certfile, mailbox, 1, &addrList, &addrCount);
  
    mutt_unlink(certfile);
  
    if (i)
    {
      mutt_message _("Certificate *NOT* added.");
      return;
    }
    if ((certfile = smime_extract_certificate(infile)))
    {
      for (i = 0; i < addrCount; i++)
      {
	/* perhaps we shouldn't abort completley ? */

	if(!ret)
	  ret = smime_add_certificate (certfile, addrList[i]);

	safe_free((void **)&(addrList[i]));
      }
      mutt_unlink (certfile);
      safe_free((void **)&certfile);
      safe_free((void **)&addrList);
    }
    if(!ret)
      return;
  }  

  if(isendwin())
  {
    mutt_any_key_to_continue(NULL);
    mutt_message _("Certificate *NOT* added.");
  }
  return;
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
		       CH_WEED|CH_NONEWLINE);
  else
    mutt_copy_message (fpout, Context, h, 0, 0);

  fflush(fpout);
  fclose (fpout);

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
	mutt_any_key_to_continue(NULL);
      else
	retval = 0;
      mutt_unlink(certfile);
      safe_free((void **)&certfile);
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
    fclose (fpout);
    mutt_unlink (tempfile);
    return NULL;
  }
  mutt_unlink (smimeerrfile);
  
  mutt_mktemp (smimeinfile);
  if ((fptmp = safe_fopen (smimeinfile, "w+")) == NULL)
  {
    mutt_perror (smimeinfile);
    mutt_unlink (tempfile);
    fclose (fpout);
    fclose (smimeerr);
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
  fclose (fptmp);

  if ((thepid =
       smime_invoke_encrypt (&smimein, NULL, NULL, -1,
			     fileno (fpout), fileno (smimeerr),
			     smimeinfile, certfile)) == -1)
  {
    fclose (smimeerr);
    mutt_unlink (smimeinfile);
    mutt_unlink (certfile);
    return (NULL);
  }

  fclose (smimein);
  
  mutt_wait_filter (thepid);
  mutt_unlink (smimeinfile);
  mutt_unlink (certfile);
  
  fflush (fpout);
  rewind (fpout);
  empty = (fgetc (fpout) == EOF);
  fclose (fpout);

  fflush (smimeerr);
  rewind (smimeerr);
  while (fgets (buf, sizeof (buf) - 1, smimeerr) != NULL)
  {
    err = 1;
    fputs (buf, stdout);
  }
  fclose (smimeerr);

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
  char *intermediates = smime_get_field_from_db(NULL, SmimeSignAs, 1, 1);

  if (!intermediates)
  {
    mutt_message(_("Warning: Intermediate certificate not found."));
    intermediates = SmimeSignAs; /* so openssl won't complain in any case */
  }
  else
      intermediates[mutt_strlen (intermediates)-1] = '\0';

  convert_to_7bit (a); /* Signed data _must_ be in 7-bit format. */

  mutt_mktemp (filetosign);
  if ((sfp = safe_fopen (filetosign, "w+")) == NULL)
  {
    mutt_perror (filetosign);
    return NULL;
  }

  mutt_mktemp (signedfile);
  if ((smimeout = safe_fopen (signedfile, "w+")) == NULL)
  {
    mutt_perror (signedfile);
    fclose (sfp);
    mutt_unlink (filetosign);
    return NULL;
  }
  
  mutt_write_mime_header (a, sfp);
  fputc ('\n', sfp);
  mutt_write_mime_body (a, sfp);
  fclose (sfp);

  

  snprintf (SmimeKeyToUse, sizeof (SmimeKeyToUse), "%s/%s", 
	   NONULL(SmimeKeys), SmimeSignAs);

  snprintf (SmimeCertToUse, sizeof (SmimeCertToUse), "%s/%s",
	   NONULL(SmimeCertificates), SmimeSignAs);
  
  snprintf (SmimeIntermediateToUse, sizeof (SmimeIntermediateToUse), "%s/%s",
	   NONULL(SmimeCertificates), intermediates);
  


  if ((thepid = smime_invoke_sign (&smimein, NULL, &smimeerr,
				 -1, fileno (smimeout), -1, filetosign)) == -1)
  {
    mutt_perror _("Can't open OpenSSL subprocess!");
    fclose (smimeout);
    mutt_unlink (signedfile);
    mutt_unlink (filetosign);
    return NULL;
  }
  fputs (SmimePass, smimein);
  fputc ('\n', smimein);
  fclose (smimein);
  

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
  fclose (smimeerr);


  fflush (smimeout);
  rewind (smimeout);
  empty = (fgetc (smimeout) == EOF);
  fclose (smimeout);

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

  sigbdy->length = ftell (s->fpout);
  sigbdy->offset = 0;
  fclose (s->fpout);

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
    fclose (smimeout);
      
    if (mutt_wait_filter (thepid))
      badsig = -1;
    else
    {
      char *line = NULL;
      int lineno = 0;
      size_t linelen;
      
      fflush (smimeerr);
      rewind (smimeerr);
      
      line = mutt_read_line (line, &linelen, smimeerr, &lineno);
      if (linelen && !mutt_strcasecmp (line, "verification successful"))
	badsig = 0;

      safe_free ((void **) &line);
    }
  }
  
  fflush (smimeerr);
  rewind (smimeerr);
  mutt_copy_stream (smimeerr, s->fpout);
  fclose (smimeerr);
    
  state_attach_puts (_("[-- End of OpenSSL output --]\n\n"), s);
  
  mutt_unlink (signedfile);
  mutt_unlink (smimeerrfile);

  sigbdy->length = tmplength;
  sigbdy->offset = tmpoffset;
  
  /* restore the original source stream */
  fclose (s->fpin);
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
    fclose (smimeout); smimeout = NULL;
    return NULL;
  }
  mutt_unlink (errfile);

  
  mutt_mktemp (tmpfname);
  if ((tmpfp = safe_fopen (tmpfname, "w+")) == NULL)
  {
    mutt_perror (tmpfname);
    fclose (smimeout); smimeout = NULL;
    fclose (smimeerr); smimeerr = NULL;
    return NULL;
  }

  fseek (s->fpin, m->offset, 0);
  last_pos = m->offset;

  mutt_copy_bytes (s->fpin, tmpfp,  m->length);

  fflush (tmpfp);
  fclose (tmpfp);

  if ((type & ENCRYPT) &&
      (thepid = smime_invoke_decrypt (&smimein, NULL, NULL, -1,
				      fileno (smimeout),  fileno (smimeerr), tmpfname)) == -1)
  {
    fclose (smimeout); smimeout = NULL;
    mutt_unlink (tmpfname);
    state_attach_puts (_("[-- Error: unable to create OpenSSL subprocess! --]\n"), s);
    return NULL;
  }
  else if ((type & SIGNOPAQUE) &&
	   (thepid = smime_invoke_verify (&smimein, NULL, NULL, -1,
					  fileno (smimeout), fileno (smimeerr), NULL,
					  tmpfname, SIGNOPAQUE)) == -1)
  {
    fclose (smimeout); smimeout = NULL;
    mutt_unlink (tmpfname);
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

  fclose (smimein);
	
  mutt_wait_filter (thepid);
  mutt_unlink (tmpfname);
  

  if (s->flags & M_DISPLAY)
  {
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
	fclose (smimeout); smimeout = NULL;
	return NULL;
      }
    }
    while (fgets (buf, sizeof (buf) - 1, smimeout) != NULL)
    {
      len = mutt_strlen (buf);
      if (len > 1 && buf[len - 2] == '\r')
	strcpy (buf + len - 2, "\n");
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
    fclose (smimeout);
    smimeout = NULL;
    mutt_unlink (outfile);

    if (!outFile)
    {
      fclose (fpout);
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
    
    line = mutt_read_line (line, &linelen, smimeerr, &lineno);
    if (linelen && !mutt_strcasecmp (line, "verification successful"))
      m->goodsig = 1;
    safe_free ((void **) &line);
  }
  else {
    m->goodsig = p->goodsig;
    m->badsig = p->badsig;
  }
  fclose (smimeerr);

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

  if (!mutt_is_application_smime (b))
    return -1;

  if (b->parts)
    return -1;
  
  memset (&s, 0, sizeof (s));
  s.fpin = fpin;
  fseek (s.fpin, b->offset, 0);

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
  b->length = ftell (s.fpout);
  b->offset = 0;
  rewind (tmpfp);
  s.fpin = tmpfp;
  s.fpout = 0;

  mutt_mktemp (tempfile);
  if ((*fpout = safe_fopen (tempfile, "w+")) == NULL)
  {
    mutt_perror (tempfile);
    return (-1);
  }
  mutt_unlink (tempfile);

  *cur = smime_handle_entity (b, &s, *fpout);
  (*cur)->goodsig = b->goodsig;
  (*cur)->badsig = b->badsig;
  b->type = origType;
  b->length = tmplength;
  b->offset = tmpoffset;
  fclose (tmpfp);

  rewind (*fpout);
  return (0);

}


void smime_application_smime_handler (BODY *m, STATE *s)
{
    
    smime_handle_entity (m, s, NULL);

}
#endif /* HAVE_SMIME */






