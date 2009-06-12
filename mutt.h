
/*
 * Copyright (C) 1996-2002 Michael R. Elkins <me@mutt.org>
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

#ifndef MUTT_H
#define MUTT_H 

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h> /* needed for SEEK_SET */
#endif
#ifdef HAVE_UNIX_H
# include <unix.h>   /* needed for snprintf on QNX. */
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <stdarg.h>
#include <signal.h>
/* On OS X 10.5.x, wide char functions are inlined by default breaking
 * --without-wc-funcs compilation
 */
#ifdef __APPLE_CC__
#define _DONT_USE_CTYPE_INLINE_
#endif
#ifdef HAVE_WCHAR_H
# include <wchar.h>
#endif
#if defined(HAVE_WCTYPE_H) && defined(HAVE_WC_FUNCS)
# include <wctype.h>
#endif

#ifndef _POSIX_PATH_MAX
#include <limits.h>
#endif

#include <pwd.h>
#include <grp.h>

#include "rfc822.h"
#include "hash.h"
#include "charset.h"

#ifndef HAVE_WC_FUNCS
# ifdef MB_LEN_MAX
#  undef MB_LEN_MAX
# endif
# define MB_LEN_MAX 16
#endif

#define MUTT_VERSION (VERSION)

/* nifty trick I stole from ELM 2.5alpha. */
#ifdef MAIN_C
#define WHERE 
#define INITVAL(x) = x
#else
#define WHERE extern
#define INITVAL(x) 
#endif

#define WHERE_DEFINED 1

#include "mutt_regex.h"

/* flags for mutt_enter_string() */
#define  M_ALIAS   1      /* do alias "completion" by calling up the alias-menu */
#define  M_FILE    (1<<1) /* do file completion */
#define  M_EFILE   (1<<2) /* do file completion, plus incoming folders */
#define  M_CMD     (1<<3) /* do completion on previous word */
#define  M_PASS    (1<<4) /* password mode (no echo) */
#define  M_CLEAR   (1<<5) /* clear input if printable character is pressed */
#define  M_COMMAND (1<<6) /* do command completion */
#define  M_PATTERN (1<<7) /* pattern mode - only used for history classes */

/* flags for mutt_get_token() */
#define M_TOKEN_EQUAL		1	/* treat '=' as a special */
#define M_TOKEN_CONDENSE	(1<<1)	/* ^(char) to control chars (macros) */
#define M_TOKEN_SPACE		(1<<2)  /* don't treat whitespace as a term */
#define M_TOKEN_QUOTE		(1<<3)	/* don't interpret quotes */
#define M_TOKEN_PATTERN		(1<<4)	/* !)|~ are terms (for patterns) */
#define M_TOKEN_COMMENT		(1<<5)	/* don't reap comments */
#define M_TOKEN_SEMICOLON	(1<<6)	/* don't treat ; as special */

/* flags for km_dokey() */
#define M_KM_UNBUFFERED 1 /* don't read from the key buffer */

typedef struct
{
  char *data;	/* pointer to data */
  char *dptr;	/* current read/write position */
  size_t dsize;	/* length of data */
  int destroy;	/* destroy `data' when done? */
} BUFFER;

typedef struct
{
  int ch; /* raw key pressed */
  int op; /* function op */
} event_t;

/* flags for _mutt_system() */
#define M_DETACH_PROCESS	1	/* detach subprocess from group */

/* flags for mutt_FormatString() */
typedef enum
{
  M_FORMAT_FORCESUBJ	= (1<<0), /* print the subject even if unchanged */
  M_FORMAT_TREE		= (1<<1), /* draw the thread tree */
  M_FORMAT_MAKEPRINT	= (1<<2), /* make sure that all chars are printable */
  M_FORMAT_OPTIONAL	= (1<<3),
  M_FORMAT_STAT_FILE	= (1<<4), /* used by mutt_attach_fmt */
  M_FORMAT_ARROWCURSOR	= (1<<5), /* reserve space for arrow_cursor */
  M_FORMAT_INDEX	= (1<<6), /* this is a main index entry */
  M_FORMAT_NOFILTER	= (1<<7)  /* do not allow filtering on this pass */
} format_flag;

/* types for mutt_add_hook() */
#define M_FOLDERHOOK	1
#define M_MBOXHOOK	(1<<1)
#define M_SENDHOOK	(1<<2)
#define M_FCCHOOK	(1<<3)
#define M_SAVEHOOK	(1<<4)
#define M_CHARSETHOOK	(1<<5)
#define M_ICONVHOOK 	(1<<6)
#define M_MESSAGEHOOK	(1<<7)
#define M_CRYPTHOOK	(1<<8)
#define M_ACCOUNTHOOK	(1<<9)
#define M_REPLYHOOK	(1<<10)
#define M_SEND2HOOK     (1<<11)

/* tree characters for linearize_tree and print_enriched_string */
#define M_TREE_LLCORNER		1
#define M_TREE_ULCORNER		2
#define M_TREE_LTEE		3
#define M_TREE_HLINE		4
#define M_TREE_VLINE		5
#define M_TREE_SPACE		6
#define M_TREE_RARROW		7
#define M_TREE_STAR		8
#define M_TREE_HIDDEN		9
#define M_TREE_EQUALS		10
#define M_TREE_TTEE		11
#define M_TREE_BTEE		12
#define M_TREE_MISSING		13
#define M_TREE_MAX		14

#define M_THREAD_COLLAPSE	(1<<0)
#define M_THREAD_UNCOLLAPSE	(1<<1)
#define M_THREAD_GET_HIDDEN	(1<<2)
#define M_THREAD_UNREAD		(1<<3)
#define M_THREAD_NEXT_UNREAD	(1<<4)

enum
{
  /* modes for mutt_view_attachment() */
  M_REGULAR = 1,
  M_MAILCAP,
  M_AS_TEXT,

  /* action codes used by mutt_set_flag() and mutt_pattern_function() */
  M_ALL,
  M_NONE,
  M_NEW,
  M_OLD,
  M_REPLIED,
  M_READ,
  M_UNREAD,
  M_DELETE,
  M_UNDELETE,
  M_DELETED,
  M_FLAG,
  M_TAG,
  M_UNTAG,
  M_LIMIT,
  M_EXPIRED,
  M_SUPERSEDED,

  /* actions for mutt_pattern_comp/mutt_pattern_exec */
  M_AND,
  M_OR,
  M_THREAD,
  M_TO,
  M_CC,
  M_COLLAPSED,
  M_SUBJECT,
  M_FROM,
  M_DATE,
  M_DATE_RECEIVED,
  M_DUPLICATED,
  M_UNREFERENCED,
  M_ID,
  M_BODY,
  M_HEADER,
  M_HORMEL,
  M_WHOLE_MSG,
  M_SENDER,
  M_MESSAGE,
  M_SCORE,
  M_SIZE,
  M_REFERENCE,
  M_RECIPIENT,
  M_LIST,
  M_SUBSCRIBED_LIST,
  M_PERSONAL_RECIP,
  M_PERSONAL_FROM,
  M_ADDRESS,
  M_CRYPT_SIGN,
  M_CRYPT_VERIFIED,
  M_CRYPT_ENCRYPT,
  M_PGP_KEY,
  M_XLABEL,
  M_MIMEATTACH,
  
  /* Options for Mailcap lookup */
  M_EDIT,
  M_COMPOSE,
  M_PRINT,
  M_AUTOVIEW,

  /* options for socket code */
  M_NEW_SOCKET,
#ifdef USE_SSL_OPENSSL
  M_NEW_SSL_SOCKET,
#endif

  /* Options for mutt_save_attachment */
  M_SAVE_APPEND,
  M_SAVE_OVERWRITE
};

/* possible arguments to set_quadoption() */
enum
{
  M_NO,
  M_YES,
  M_ASKNO,
  M_ASKYES
};

/* quad-option vars */
enum
{
  OPT_ABORT,
  OPT_BOUNCE,
  OPT_COPY,
  OPT_DELETE,
  OPT_FORWEDIT,
  OPT_FCCATTACH,
  OPT_INCLUDE,
  OPT_MFUPTO,
  OPT_MIMEFWD,
  OPT_MIMEFWDREST,
  OPT_MOVE,
  OPT_PGPMIMEAUTO,     /* ask to revert to PGP/MIME when inline fails */
#ifdef USE_POP
  OPT_POPDELETE,
  OPT_POPRECONNECT,
#endif
  OPT_POSTPONE,
  OPT_PRINT,
  OPT_QUIT,
  OPT_REPLYTO,
  OPT_RECALL,
#if defined(USE_SSL)
  OPT_SSLSTARTTLS,
#endif
  OPT_SUBJECT,
  OPT_VERIFYSIG,      /* verify PGP signatures */
    
  /* THIS MUST BE THE LAST VALUE. */
  OPT_MAX
};

/* flags to ci_send_message() */
#define SENDREPLY	(1<<0)
#define SENDGROUPREPLY	(1<<1)
#define SENDLISTREPLY	(1<<2)
#define SENDFORWARD	(1<<3)
#define SENDPOSTPONED	(1<<4)
#define SENDBATCH	(1<<5)
#define SENDMAILX	(1<<6)
#define SENDKEY		(1<<7)
#define SENDRESEND	(1<<8)

/* flags to _mutt_select_file() */
#define M_SEL_BUFFY	(1<<0)
#define M_SEL_MULTI	(1<<1)
#define M_SEL_FOLDER	(1<<2)

/* flags for parse_spam_list */
#define M_SPAM          1
#define M_NOSPAM        2

/* boolean vars */
enum
{
  OPTALLOW8BIT,
  OPTALLOWANSI,
  OPTARROWCURSOR,
  OPTASCIICHARS,
  OPTASKBCC,
  OPTASKCC,
  OPTATTACHSPLIT,
  OPTAUTOEDIT,
  OPTAUTOTAG,
  OPTBEEP,
  OPTBEEPNEW,
  OPTBOUNCEDELIVERED,
  OPTBRAILLEFRIENDLY,
  OPTCHECKMBOXSIZE,
  OPTCHECKNEW,
  OPTCOLLAPSEUNREAD,
  OPTCONFIRMAPPEND,
  OPTCONFIRMCREATE,
  OPTDELETEUNTAG,
  OPTDIGESTCOLLAPSE,
  OPTDUPTHREADS,
  OPTEDITHDRS,
  OPTENCODEFROM,
  OPTENVFROM,
  OPTFASTREPLY,
  OPTFCCCLEAR,
  OPTFOLLOWUPTO,
  OPTFORCENAME,
  OPTFORWDECODE,
  OPTFORWQUOTE,
#ifdef USE_HCACHE
  OPTHCACHEVERIFY,
#if defined(HAVE_QDBM) || defined(HAVE_TC)
  OPTHCACHECOMPRESS,
#endif /* HAVE_QDBM */
#endif
  OPTHDRS,
  OPTHEADER,
  OPTHELP,
  OPTHIDDENHOST,
  OPTHIDELIMITED,
  OPTHIDEMISSING,
  OPTHIDETHREADSUBJECT,
  OPTHIDETOPLIMITED,
  OPTHIDETOPMISSING,
  OPTHONORDISP,
  OPTIGNORELWS,
  OPTIGNORELISTREPLYTO,
#ifdef USE_IMAP
  OPTIMAPCHECKSUBSCRIBED,
  OPTIMAPIDLE,
  OPTIMAPLSUB,
  OPTIMAPPASSIVE,
  OPTIMAPPEEK,
  OPTIMAPSERVERNOISE,
#endif
#if defined(USE_SSL)
# ifndef USE_SSL_GNUTLS
  OPTSSLSYSTEMCERTS,
  OPTSSLV2,
# endif /* USE_SSL_GNUTLS */
  OPTSSLV3,
  OPTTLSV1,
  OPTSSLFORCETLS,
  OPTSSLVERIFYDATES,
  OPTSSLVERIFYHOST,
#endif /* defined(USE_SSL) */
  OPTIMPLICITAUTOVIEW,
  OPTINCLUDEONLYFIRST,
  OPTKEEPFLAGGED,
  OPTMAILCAPSANITIZE,
  OPTMAILDIRTRASH,
  OPTMARKERS,
  OPTMARKOLD,
  OPTMENUSCROLL,	/* scroll menu instead of implicit next-page */
  OPTMENUMOVEOFF,	/* allow menu to scroll past last entry */
#if defined(USE_IMAP) || defined(USE_POP)
  OPTMESSAGECACHECLEAN,
#endif
  OPTMETAKEY,		/* interpret ALT-x as ESC-x */
  OPTMETOO,
  OPTMHPURGE,
  OPTMIMEFORWDECODE,
  OPTNARROWTREE,
  OPTPAGERSTOP,
  OPTPIPEDECODE,
  OPTPIPESPLIT,
#ifdef USE_POP
  OPTPOPAUTHTRYALL,
  OPTPOPLAST,
#endif
  OPTPRINTDECODE,
  OPTPRINTSPLIT,
  OPTPROMPTAFTER,
  OPTREADONLY,
  OPTREPLYSELF,
  OPTRESOLVE,
  OPTREVALIAS,
  OPTREVNAME,
  OPTREVREAL,
  OPTRFC2047PARAMS,
  OPTSAVEADDRESS,
  OPTSAVEEMPTY,
  OPTSAVENAME,
  OPTSCORE,
  OPTSIGDASHES,
  OPTSIGONTOP,
  OPTSORTRE,
  OPTSPAMSEP,
  OPTSTATUSONTOP,
  OPTSTRICTTHREADS,
  OPTSUSPEND,
  OPTTEXTFLOWED,
  OPTTHOROUGHSRC,
  OPTTHREADRECEIVED,
  OPTTILDE,
  OPTUNCOLLAPSEJUMP,
  OPTUSE8BITMIME,
  OPTUSEDOMAIN,
  OPTUSEFROM,
  OPTUSEGPGAGENT,
#ifdef HAVE_LIBIDN
  OPTUSEIDN,
#endif
#ifdef HAVE_GETADDRINFO
  OPTUSEIPV6,
#endif
  OPTWAITKEY,
  OPTWEED,
  OPTWRAP,
  OPTWRAPSEARCH,
  OPTWRITEBCC,		/* write out a bcc header? */
  OPTXMAILER,

  OPTCRYPTUSEGPGME,
  OPTCRYPTUSEPKA,

  /* PGP options */
  
  OPTCRYPTAUTOSIGN,
  OPTCRYPTAUTOENCRYPT,
  OPTCRYPTAUTOPGP,
  OPTCRYPTAUTOSMIME,
  OPTCRYPTREPLYENCRYPT,
  OPTCRYPTREPLYSIGN,
  OPTCRYPTREPLYSIGNENCRYPTED,
  OPTCRYPTTIMESTAMP,
  OPTSMIMEISDEFAULT,
  OPTASKCERTLABEL,
  OPTSDEFAULTDECRYPTKEY,
  OPTPGPIGNORESUB,
  OPTPGPCHECKEXIT,
  OPTPGPLONGIDS,
  OPTPGPAUTODEC,
#if 0
  OPTPGPENCRYPTSELF,
#endif
  OPTPGPRETAINABLESIG,
  OPTPGPSTRICTENC,
  OPTFORWDECRYPT,
  OPTPGPSHOWUNUSABLE,
  OPTPGPAUTOINLINE,
  OPTPGPREPLYINLINE,

  /* pseudo options */

  OPTAUXSORT,		/* (pseudo) using auxillary sort function */
  OPTFORCEREFRESH,	/* (pseudo) refresh even during macros */
  OPTLOCALES,		/* (pseudo) set if user has valid locale definition */
  OPTNOCURSES,		/* (pseudo) when sending in batch mode */
  OPTNEEDREDRAW,	/* (pseudo) to notify caller of a submenu */
  OPTSEARCHREVERSE,	/* (pseudo) used by ci_search_command */
  OPTMSGERR,		/* (pseudo) used by mutt_error/mutt_message */
  OPTSEARCHINVALID,	/* (pseudo) used to invalidate the search pat */
  OPTSIGNALSBLOCKED,	/* (pseudo) using by mutt_block_signals () */
  OPTSYSSIGNALSBLOCKED,	/* (pseudo) using by mutt_block_signals_system () */
  OPTNEEDRESORT,	/* (pseudo) used to force a re-sort */
  OPTRESORTINIT,	/* (pseudo) used to force the next resort to be from scratch */
  OPTVIEWATTACH,	/* (pseudo) signals that we are viewing attachments */
  OPTFORCEREDRAWINDEX,	/* (pseudo) used to force a redraw in the main index */
  OPTFORCEREDRAWPAGER,	/* (pseudo) used to force a redraw in the pager */
  OPTSORTSUBTHREADS,	/* (pseudo) used when $sort_aux changes */
  OPTNEEDRESCORE,	/* (pseudo) set when the `score' command is used */
  OPTATTACHMSG,		/* (pseudo) used by attach-message */
  OPTKEEPQUIET,		/* (pseudo) shut up the message and refresh
			 * 	    functions while we are executing an
			 * 	    external program.
			 */
  OPTMENUCALLER,	/* (pseudo) tell menu to give caller a take */
  OPTREDRAWTREE,	/* (pseudo) redraw the thread tree */
  OPTPGPCHECKTRUST,	/* (pseudo) used by pgp_select_key () */
  OPTDONTHANDLEPGPKEYS,	/* (pseudo) used to extract PGP keys */
  OPTUNBUFFEREDINPUT,   /* (pseudo) don't use key buffer */

  OPTMAX
};

#define mutt_bit_alloc(n) calloc ((n + 7) / 8, sizeof (char))
#define mutt_bit_set(v,n) v[n/8] |= (1 << (n % 8))
#define mutt_bit_unset(v,n) v[n/8] &= ~(1 << (n % 8))
#define mutt_bit_toggle(v,n) v[n/8] ^= (1 << (n % 8))
#define mutt_bit_isset(v,n) (v[n/8] & (1 << (n % 8)))

#define set_option(x) mutt_bit_set(Options,x)
#define unset_option(x) mutt_bit_unset(Options,x)
#define toggle_option(x) mutt_bit_toggle(Options,x)
#define option(x) mutt_bit_isset(Options,x)

typedef struct list_t
{
  char *data;
  struct list_t *next;
} LIST;

typedef struct rx_list_t
{
  REGEXP *rx;
  struct rx_list_t *next;
} RX_LIST;

typedef struct spam_list_t
{
  REGEXP *rx;
  int     nmatch;
  char   *template;
  struct spam_list_t *next;
} SPAM_LIST;

#define mutt_new_list() safe_calloc (1, sizeof (LIST))
#define mutt_new_rx_list() safe_calloc (1, sizeof (RX_LIST))
#define mutt_new_spam_list() safe_calloc (1, sizeof (SPAM_LIST))
void mutt_free_list (LIST **);
void mutt_free_rx_list (RX_LIST **);
void mutt_free_spam_list (SPAM_LIST **);
LIST *mutt_copy_list (LIST *);
int mutt_matches_ignore (const char *, LIST *);

/* add an element to a list */
LIST *mutt_add_list (LIST *, const char *);
LIST *mutt_add_list_n (LIST*, const void *, size_t);
LIST *mutt_find_list (LIST *, const char *);

void mutt_init (int, LIST *);

typedef struct alias
{
  struct alias *self;		/* XXX - ugly hack */
  char *name;
  ADDRESS *addr;
  struct alias *next;
  short tagged;
  short del;
  short num;
} ALIAS;

typedef struct envelope
{
  ADDRESS *return_path;
  ADDRESS *from;
  ADDRESS *to;
  ADDRESS *cc;
  ADDRESS *bcc;
  ADDRESS *sender;
  ADDRESS *reply_to;
  ADDRESS *mail_followup_to;
  char *list_post;		/* this stores a mailto URL, or nothing */
  char *subject;
  char *real_subj;		/* offset of the real subject */
  char *message_id;
  char *supersedes;
  char *date;
  char *x_label;
  BUFFER *spam;
  LIST *references;		/* message references (in reverse order) */
  LIST *in_reply_to;		/* in-reply-to header content */
  LIST *userhdrs;		/* user defined headers */

  unsigned int irt_changed : 1; /* In-Reply-To changed to link/break threads */
  unsigned int refs_changed : 1; /* References changed to break thread */
} ENVELOPE;

typedef struct parameter
{
  char *attribute;
  char *value;
  struct parameter *next;
} PARAMETER;

/* Information that helps in determing the Content-* of an attachment */
typedef struct content
{
  long hibin;              /* 8-bit characters */
  long lobin;              /* unprintable 7-bit chars (eg., control chars) */
  long crlf;		   /* '\r' and '\n' characters */
  long ascii;              /* number of ascii chars */
  long linemax;            /* length of the longest line in the file */
  unsigned int space : 1;  /* whitespace at the end of lines? */
  unsigned int binary : 1; /* long lines, or CR not in CRLF pair */
  unsigned int from : 1;   /* has a line beginning with "From "? */
  unsigned int dot : 1;    /* has a line consisting of a single dot? */
  unsigned int cr : 1;     /* has CR, even when in a CRLF pair */
} CONTENT;

typedef struct body
{
  char *xtype;			/* content-type if x-unknown */
  char *subtype;                /* content-type subtype */
  PARAMETER *parameter;         /* parameters of the content-type */
  char *description;            /* content-description */
  char *form_name;		/* Content-Disposition form-data name param */
  long hdr_offset;              /* offset in stream where the headers begin.
				 * this info is used when invoking metamail,
				 * where we need to send the headers of the
				 * attachment
				 */
  LOFF_T offset;                /* offset where the actual data begins */
  LOFF_T length;                /* length (in bytes) of attachment */
  char *filename;               /* when sending a message, this is the file
				 * to which this structure refers
				 */
  char *d_filename;		/* filename to be used for the 
				 * content-disposition header.
				 * If NULL, filename is used 
				 * instead.
				 */
  char *charset;                /* charset of attached file */
  CONTENT *content;             /* structure used to store detailed info about
				 * the content of the attachment.  this is used
				 * to determine what content-transfer-encoding
				 * is required when sending mail.
				 */
  struct body *next;            /* next attachment in the list */
  struct body *parts;           /* parts of a multipart or message/rfc822 */
  struct header *hdr;		/* header information for message/rfc822 */

  struct attachptr *aptr;	/* Menu information, used in recvattach.c */

  signed short attach_count;

  time_t stamp;			/* time stamp of last
				 * encoding update.
				 */
  
  unsigned int type : 4;        /* content-type primary type */
  unsigned int encoding : 3;    /* content-transfer-encoding */
  unsigned int disposition : 2; /* content-disposition */
  unsigned int use_disp : 1;    /* Content-Disposition uses filename= ? */
  unsigned int unlink : 1;      /* flag to indicate the the file named by
				 * "filename" should be unlink()ed before
				 * free()ing this structure
				 */
  unsigned int tagged : 1;
  unsigned int deleted : 1;	/* attachment marked for deletion */

  unsigned int noconv : 1;	/* don't do character set conversion */
  unsigned int force_charset : 1; 
  				/* send mode: don't adjust the character
				 * set when in send-mode.
				 */
  unsigned int is_signed_data : 1; /* A lot of MUAs don't indicate
                                      S/MIME signed-data correctly,
                                      e.g. they use foo.p7m even for
                                      the name of signed data.  This
                                      flag is used to keep track of
                                      the actual message type.  It
                                      gets set during the verification
                                      (which is done if the encryption
                                      try failed) and check by the
                                      function to figure the type of
                                      the message. */

  unsigned int goodsig : 1;	/* good cryptographic signature */
  unsigned int warnsig : 1;     /* maybe good signature */
  unsigned int badsig : 1;	/* bad cryptographic signature (needed to check encrypted s/mime-signatures) */

  unsigned int collapsed : 1;	/* used by recvattach */
  unsigned int attach_qualifies : 1;

} BODY;

typedef struct header
{
  unsigned int security : 11;  /* bit 0-6: flags, bit 7,8: application.
				 see: crypt.h pgplib.h, smime.h */

  unsigned int mime : 1;    		/* has a MIME-Version header? */
  unsigned int flagged : 1; 		/* marked important? */
  unsigned int tagged : 1;
  unsigned int deleted : 1;
  unsigned int changed : 1;
  unsigned int attach_del : 1; 		/* has an attachment marked for deletion */
  unsigned int old : 1;
  unsigned int read : 1;
  unsigned int expired : 1; 		/* already expired? */
  unsigned int superseded : 1; 		/* got superseded? */
  unsigned int replied : 1;
  unsigned int subject_changed : 1; 	/* used for threading */
  unsigned int threaded : 1;	    	/* used for threading */
  unsigned int display_subject : 1; 	/* used for threading */
  unsigned int recip_valid : 1;  	/* is_recipient is valid */
  unsigned int active : 1;	    	/* message is not to be removed */
  unsigned int trash : 1;		/* message is marked as trashed on disk.
					 * This flag is used by the maildir_trash
					 * option.
					 */
  
  /* timezone of the sender of this message */
  unsigned int zhours : 5;
  unsigned int zminutes : 6;
  unsigned int zoccident : 1;

  /* bits used for caching when searching */
  unsigned int searched : 1;
  unsigned int matched : 1;

  /* tells whether the attachment count is valid */
  unsigned int attach_valid : 1;

  /* the following are used to support collapsing threads  */
  unsigned int collapsed : 1; 	/* is this message part of a collapsed thread? */
  unsigned int limited : 1;   	/* is this message in a limited view?  */
  size_t num_hidden;          	/* number of hidden messages in this view */

  short recipient;		/* user_is_recipient()'s return value, cached */
  
  int pair; 			/* color-pair to use when displaying in the index */

  time_t date_sent;     	/* time when the message was sent (UTC) */
  time_t received;      	/* time when the message was placed in the mailbox */
  LOFF_T offset;          	/* where in the stream does this message begin? */
  int lines;			/* how many lines in the body of this message? */
  int index;			/* the absolute (unsorted) message number */
  int msgno;			/* number displayed to the user */
  int virtual;			/* virtual message number */
  int score;
  ENVELOPE *env;		/* envelope information */
  BODY *content;		/* list of MIME parts */
  char *path;
  
  char *tree;           	/* character string to print thread tree */
  struct thread *thread;

  /* Number of qualifying attachments in message, if attach_valid */
  short attach_total;

#ifdef MIXMASTER
  LIST *chain;
#endif

#ifdef USE_POP
  int refno;			/* message number on server */
#endif

#if defined USE_POP || defined USE_IMAP
  void *data;            	/* driver-specific data */
#endif
  
  char *maildir_flags;		/* unknown maildir flags */
} HEADER;

typedef struct thread
{
  unsigned int fake_thread : 1;
  unsigned int duplicate_thread : 1;
  unsigned int sort_children : 1;
  unsigned int check_subject : 1;
  unsigned int visible : 1;
  unsigned int deep : 1;
  unsigned int subtree_visible : 2;
  unsigned int next_subtree_visible : 1;
  struct thread *parent;
  struct thread *child;
  struct thread *next;
  struct thread *prev;
  HEADER *message;
  HEADER *sort_key;
} THREAD;


/* flag to mutt_pattern_comp() */
#define M_FULL_MSG	(1<<0)	/* enable body and header matching */

typedef enum {
  M_MATCH_FULL_ADDRESS = 1
} pattern_exec_flag;

typedef struct group_t
{
  ADDRESS *as;
  RX_LIST *rs;
  char *name;
} group_t;

typedef struct group_context_t
{
  group_t *g;
  struct group_context_t *next;
} group_context_t;

typedef struct pattern_t
{
  short op;
  unsigned int not : 1;
  unsigned int alladdr : 1;
  unsigned int stringmatch : 1;
  unsigned int groupmatch : 1;
  unsigned int ign_case : 1;		/* ignore case for local stringmatch searches */
  int min;
  int max;
  struct pattern_t *next;
  struct pattern_t *child;		/* arguments to logical op */
  union 
  {
    regex_t *rx;
    group_t *g;
    char *str;
  } p;
} pattern_t;

/* ACL Rights */
enum
{
  M_ACL_LOOKUP = 0,
  M_ACL_READ,
  M_ACL_SEEN,
  M_ACL_WRITE,
  M_ACL_INSERT,
  M_ACL_POST,
  M_ACL_CREATE,
  M_ACL_DELMX,
  M_ACL_DELETE,
  M_ACL_EXPUNGE,
  M_ACL_ADMIN,

  RIGHTSMAX
};

typedef struct _context
{
  char *path;
  FILE *fp;
  time_t mtime;
  off_t size;
  off_t vsize;
  char *pattern;                /* limit pattern string */
  pattern_t *limit_pattern;     /* compiled limit pattern */
  HEADER **hdrs;
  HEADER *last_tag;		/* last tagged msg. used to link threads */
  THREAD *tree;			/* top of thread tree */
  HASH *id_hash;		/* hash table by msg id */
  HASH *subj_hash;		/* hash table by subject */
  HASH *thread_hash;		/* hash table for threading */
  int *v2r;			/* mapping from virtual to real msgno */
  int hdrmax;			/* number of pointers in hdrs */
  int msgcount;			/* number of messages in the mailbox */
  int vcount;			/* the number of virtual messages */
  int tagged;			/* how many messages are tagged? */
  int new;			/* how many new messages? */
  int unread;			/* how many unread messages? */
  int deleted;			/* how many deleted messages */
  int flagged;			/* how many flagged messages */
  int msgnotreadyet;		/* which msg "new" in pager, -1 if none */

  short magic;			/* mailbox type */

  unsigned char rights[(RIGHTSMAX + 7)/8];	/* ACL bits */

  unsigned int locked : 1;	/* is the mailbox locked? */
  unsigned int changed : 1;	/* mailbox has been modified */
  unsigned int readonly : 1;    /* don't allow changes to the mailbox */
  unsigned int dontwrite : 1;   /* dont write the mailbox on close */
  unsigned int append : 1;	/* mailbox is opened in append mode */
  unsigned int quiet : 1;	/* inhibit status messages? */
  unsigned int collapsed : 1;   /* are all threads collapsed? */
  unsigned int closing : 1;	/* mailbox is being closed */

  /* driver hooks */
  void *data;			/* driver specific data */
  int (*mx_close)(struct _context *);
} CONTEXT;

typedef struct
{
  FILE *fpin;
  FILE *fpout;
  char *prefix;
  int flags;
} STATE;

/* used by enter.c */

typedef struct
{
  wchar_t *wbuf;
  size_t wbuflen;
  size_t lastchar;
  size_t curpos;
  size_t begin;
  int	 tabs;
} ENTER_STATE;

/* flags for the STATE struct */
#define M_DISPLAY	(1<<0) /* output is displayed to the user */
#define M_VERIFY	(1<<1) /* perform signature verification */
#define M_PENDINGPREFIX (1<<2) /* prefix to write, but character must follow */
#define M_WEED          (1<<3) /* weed headers even when not in display mode */
#define M_CHARCONV	(1<<4) /* Do character set conversions */
#define M_PRINTING	(1<<5) /* are we printing? - M_DISPLAY "light" */
#define M_REPLYING	(1<<6) /* are we replying? */
#define M_FIRSTDONE	(1<<7) /* the first attachment has been done */

#define state_set_prefix(s) ((s)->flags |= M_PENDINGPREFIX)
#define state_reset_prefix(s) ((s)->flags &= ~M_PENDINGPREFIX)
#define state_puts(x,y) fputs(x,(y)->fpout)
#define state_putc(x,y) fputc(x,(y)->fpout)
#define state_putws(x,y) fputws(x,(y)->fpout)
#define state_putwc(x,y) fputwc(x,(y)->fpout)

void state_mark_attach (STATE *);
void state_attach_puts (const char *, STATE *);
void state_prefix_putc (char, STATE *);
int  state_printf(STATE *, const char *, ...);

/* for attachment counter */
typedef struct
{
  char   *major;
  int     major_int;
  char   *minor;
  regex_t minor_rx;
} ATTACH_MATCH;

#define M_PARTS_TOPLEVEL	(1<<0)	/* is the top-level part */

#include "ascii.h"
#include "protos.h"
#include "lib.h"
#include "globals.h"

#endif /*MUTT_H*/
