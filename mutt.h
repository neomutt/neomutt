/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <stdarg.h>

#ifndef _POSIX_PATH_MAX
#include <posix1_lim.h>
#endif

#include "rfc822.h"
#include "hash.h"

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(a) (gettext (a))
# ifdef gettext_noop
#  define N_(a) (gettext_noop (a))
# else
#  define N_(a) (a)
# endif
#else
# define _(a) (a)
# define N_(a) (a)
#endif

#ifdef SUBVERSION
# define MUTT_VERSION (VERSION SUBVERSION)
#else  
# define MUTT_VERSION (VERSION)
#endif

/* nifty trick I stole from ELM 2.5alpha. */
#ifdef MAIN_C
#define WHERE 
#define INITVAL(x) = x
#else
#define WHERE extern
#define INITVAL(x) 
#endif

#define TRUE 1
#define FALSE 0

#define HUGE_STRING	5120
#define LONG_STRING     1024
#define STRING          256
#define SHORT_STRING    128

/* flags for mutt_copy_header() */
#define CH_UPDATE	1      /* update the status and x-status fields? */
#define CH_WEED		(1<<1) /* weed the headers? */
#define CH_DECODE	(1<<2) /* do RFC1522 decoding? */
#define CH_XMIT		(1<<3) /* transmitting this message? */
#define CH_FROM		(1<<4) /* retain the "From " message separator? */
#define CH_PREFIX	(1<<5) /* use Prefix string? */
#define CH_NOSTATUS	(1<<6) /* supress the status and x-status fields */
#define CH_REORDER	(1<<7) /* Re-order output of headers */
#define CH_NONEWLINE	(1<<8) /* don't output terminating newline */
#define CH_MIME		(1<<9) /* ignore MIME fields */
#define CH_UPDATE_LEN	(1<<10) /* update Lines: and Content-Length: */
#define CH_TXTPLAIN	(1<<11) /* generate text/plain MIME headers */

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

typedef struct
{
  char *data;	/* pointer to data */
  char *dptr;	/* current read/write position */
  size_t dsize;	/* length of data */
  int destroy;	/* destroy `data' when done? */
} BUFFER;

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
  M_FORMAT_INDEX	= (1<<6)  /* this is a main index entry */
} format_flag;

/* types for mutt_add_hook() */
#define M_FOLDERHOOK	1
#define M_MBOXHOOK	(1<<1)
#define M_SENDHOOK	(1<<2)
#define M_FCCHOOK	(1<<3)
#define M_SAVEHOOK	(1<<4)
#define M_PGPHOOK	(1<<5)

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
#define M_TREE_MAX		10

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
  M_TO,
  M_CC,
  M_SUBJECT,
  M_FROM,
  M_DATE,
  M_DATE_RECEIVED,
  M_ID,
  M_BODY,
  M_HEADER,
  M_WHOLE_MSG,
  M_SENDER,
  M_MESSAGE,
  M_SCORE,
  M_SIZE,
  M_REFERENCE,
  M_RECIPIENT,
  M_LIST,
  M_PERSONAL_RECIP,
  M_PERSONAL_FROM,
  M_ADDRESS,
#ifdef _PGPPATH
  M_PGP_SIGN,
  M_PGP_ENCRYPT,
#endif
  
  /* Options for Mailcap lookup */
  M_EDIT,
  M_COMPOSE,
  M_PRINT,
  M_AUTOVIEW,

  /* options for socket code */
  M_NEW_SOCKET,

  /* Options for mutt_save_attachment */
  M_SAVE_APPEND
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

#ifdef _PGPPATH
  OPT_VERIFYSIG, /* verify PGP signatures */
#endif

  OPT_USEMAILCAP,
  OPT_PRINT,
  OPT_INCLUDE,
  OPT_DELETE,
  OPT_MIMEFWD,
  OPT_MOVE,
  OPT_COPY,
  OPT_POSTPONE,
  OPT_QUIT,
  OPT_REPLYTO,
  OPT_ABORT,
  OPT_RECALL,
  OPT_SUBJECT
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
#define SENDEDITMSG	(1<<8)

/* boolean vars */
enum
{
  OPTALLOW8BIT,
  OPTARROWCURSOR,
  OPTASCIICHARS,
  OPTASKBCC,
  OPTASKCC,
  OPTATTACHSPLIT,
  OPTAUTOEDIT,
  OPTAUTOTAG,
  OPTBEEP,
  OPTBEEPNEW,
  OPTCHECKNEW,
  OPTCOLLAPSEUNREAD,
  OPTCONFIRMAPPEND,
  OPTCONFIRMCREATE,
  OPTEDITHDRS,
  OPTFASTREPLY,
  OPTFCCATTACH,
  OPTFOLLOWUPTO,
  OPTFORCENAME,
  OPTFORWDECODE,
  OPTFORWQUOTE,
  OPTHDRS,
  OPTHEADER,
  OPTHELP,
  OPTHIDDENHOST,
  OPTIGNORELISTREPLYTO,
  OPTMARKERS,
  OPTMARKOLD,
  OPTMENUSCROLL,	/* scroll menu instead of implicit next-page */
  OPTMETAKEY,		/* interpret ALT-x as ESC-x */
  OPTMETOO,
  OPTMHPURGE,
  OPTMIMEFORWDECODE,
  OPTPAGERSTOP,
  OPTPIPEDECODE,
  OPTPIPESPLIT,
  OPTPOPDELETE,
  OPTPROMPTAFTER,
  OPTREADONLY,
  OPTRESOLVE,
  OPTREVALIAS,
  OPTREVNAME,
  OPTSAVEADDRESS,
  OPTSAVEEMPTY,
  OPTSAVENAME,
  OPTSIGDASHES,
  OPTSORTRE,
  OPTSTATUSONTOP,
  OPTSTRICTTHREADS,
  OPTSUSPEND,
  OPTTHOROUGHSRC,
  OPTTILDE,
  OPTUNCOLLAPSEJUMP,
  OPTUSE8BITMIME,
  OPTUSEDOMAIN,
  OPTUSEFROM,
  OPTWAITKEY,
  OPTWEED,
  OPTWRAP,
  OPTWRAPSEARCH,
  OPTWRITEBCC,		/* write out a bcc header? */

  /* PGP options */
  
#ifdef _PGPPATH
  OPTPGPAUTOSIGN,
  OPTPGPAUTOENCRYPT,
  OPTPGPLONGIDS,
  OPTPGPREPLYENCRYPT,
  OPTPGPREPLYSIGN,
  OPTPGPENCRYPTSELF,
  OPTPGPSTRICTENC,
  OPTFORWDECRYPT,
#endif

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
  OPTNEEDRESORT,	/* (pseudo) used to force a re-sort */
  OPTVIEWATTACH,	/* (pseudo) signals that we are viewing attachments */
  OPTFORCEREDRAWINDEX,	/* (pseudo) used to force a redraw in the main index */
  OPTFORCEREDRAWPAGER,	/* (pseudo) used to force a redraw in the pager */
  OPTSORTSUBTHREADS,	/* (pseudo) used when $sort_aux changes */
  OPTNEEDRESCORE,	/* (pseudo) set when the `score' command is used */
  OPTSORTCOLLAPSE,	/* (pseudo) used by mutt_sort_headers() */
  OPTUSEHEADERDATE,	/* (pseudo) used by edit-message */
  OPTATTACHMSG,		/* (pseudo) used by attach-message */
  
#ifdef _PGPPATH
  OPTPGPCHECKTRUST,	/* (pseudo) used by pgp_select_key () */
  OPTDONTHANDLEPGPKEYS,	/* (pseudo) used to extract PGP keys */
#endif




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

/* Bit fields for ``Signals'' */
#define S_INTERRUPT (1<<1)
#define S_SIGWINCH  (1<<2)
#define S_ALARM     (1<<3)

typedef struct list_t
{
  char *data;
  struct list_t *next;
} LIST;

#define mutt_new_list() safe_calloc (1, sizeof (LIST))
void mutt_add_to_list (LIST **, BUFFER *);
void mutt_free_list (LIST **);
int mutt_matches_ignore (const char *, LIST *);

/* add an element to a list */
LIST *mutt_add_list (LIST *, const char *);

void mutt_init (int, LIST *);

typedef struct alias
{
  char *name;
  ADDRESS *addr;
  struct alias *next;
  short tagged;
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
  char *subject;
  char *real_subj;		/* offset of the real subject */
  char *message_id;
  char *supersedes;
  char *date;
  LIST *references;		/* message references (in reverse order) */
  LIST *userhdrs;		/* user defined headers */
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
  long ascii;              /* number of ascii chars */
  long linemax;            /* length of the longest line in the file */
  unsigned int space : 1;  /* whitespace at the end of lines? */
  unsigned int binary : 1; /* long lines, or CR not in CRLF pair */
  unsigned int from : 1;   /* has a line beginning with "From "? */
  unsigned int dot : 1;    /* has a line consisting of a single dot? */
  unsigned int nonasc : 1; /* has unicode characters out of ASCII range */
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
  long offset;                  /* offset where the actual data begins */
  long length;                  /* length (in bytes) of attachment */
  char *filename;               /* when sending a message, this is the file
				 * to which this structure refers
				 */
  char *d_filename;		/* filename to be used for the 
				 * content-disposition header.
				 * If NULL, filename is used 
				 * instead.
				 */
  CONTENT *content;             /* structure used to store detailed info about
				 * the content of the attachment.  this is used
				 * to determine what content-transfer-encoding
				 * is required when sending mail.
				 */
  struct body *next;            /* next attachment in the list */
  struct body *parts;           /* parts of a multipart or message/rfc822 */
  struct header *hdr;		/* header information for message/rfc822 */

  time_t stamp;			/* time stamp of last
				 * encoding update.
				 */
  
  unsigned int type : 3;        /* content-type primary type */
  unsigned int encoding : 3;    /* content-transfer-encoding */
  unsigned int disposition : 2; /* content-disposition */
  unsigned int use_disp : 1;    /* Content-Disposition field printed? */
  unsigned int unlink : 1;      /* flag to indicate the the file named by
				 * "filename" should be unlink()ed before
				 * free()ing this structure
				 */
  unsigned int tagged : 1;
  unsigned int deleted : 1;	/* attachment marked for deletion */

} BODY;

typedef struct header
{
  #ifdef _PGPPATH
  unsigned int pgp : 3;
#endif

  unsigned int mime : 1;    /* has a Mime-Version header? */
  unsigned int mailcap : 1; /* requires mailcap to display? */
  unsigned int flagged : 1; /* marked important? */
  unsigned int tagged : 1;
  unsigned int deleted : 1;
  unsigned int changed : 1;
  unsigned int attach_del : 1; /* has an attachment marked for deletion */
  unsigned int old : 1;
  unsigned int read : 1;
  unsigned int expired : 1; /* already expired? */
  unsigned int superseded : 1; /* got superseded? */
  unsigned int replied : 1;
  unsigned int subject_changed : 1; /* used for threading */
  unsigned int display_subject : 1; /* used for threading */
  unsigned int fake_thread : 1;     /* no ref matched, but subject did */
  unsigned int threaded : 1;        /* message has been threaded */
  unsigned int recip_valid : 1;  /* is_recipient is valid */
  unsigned int active : 1;	    /* message is not to be removed */
  
  /* timezone of the sender of this message */
  unsigned int zhours : 5;
  unsigned int zminutes : 6;
  unsigned int zoccident : 1;

  /* bits used for caching when searching */
  unsigned int searched : 1;
  unsigned int matched : 1;

  /* the following are used to support collapsing threads  */
  unsigned int collapsed : 1; /* is this message part of a collapsed thread? */
  unsigned int limited : 1;   /* is this message in a limited view?  */
  size_t num_hidden;          /* number of hidden messages in this view */

  short recipient;	/* user_is_recipient()'s return value, cached */
  
  int pair; /* color-pair to use when displaying in the index */

  time_t date_sent;     /* time when the message was sent (UTC) */
  time_t received;      /* time when the message was placed in the mailbox */
  long offset;          /* where in the stream does this message begin? */
  int lines;		/* how many lines in the body of this message? */
  int index;		/* the absolute (unsorted) message number */
  int msgno;		/* number displayed to the user */
  int virtual;		/* virtual message number */
  int score;
  ENVELOPE *env;	/* envelope information */
  BODY *content;	/* list of MIME parts */
  char *path;
  
  /* the following are used for threading support */
  struct header *parent;
  struct header *child;  /* decendants of this message */
  struct header *next;   /* next message in this thread */
  struct header *prev;   /* previous message in thread */
  struct header *last_sort; /* last message in subthread, for secondary SORT_LAST */
  char *tree;            /* character string to print thread tree */

} HEADER;

#include "mutt_regex.h"

/* flag to mutt_pattern_comp() */
#define M_FULL_MSG	1	/* enable body and header matching */

typedef enum {
  M_MATCH_FULL_ADDRESS = 1
} pattern_exec_flag;

typedef struct pattern_t
{
  short op;
  short not;
  int min;
  int max;
  struct pattern_t *next;
  struct pattern_t *child;		/* arguments to logical op */
  regex_t *rx;
} pattern_t;

typedef struct
{
  char *path;
  FILE *fp;
  time_t mtime;
  time_t mtime_cur;		/* used with maildir folders */
  off_t size;
  off_t vsize;
  char *pattern;                /* limit pattern string */
  pattern_t *limit_pattern;     /* compiled limit pattern */
  HEADER **hdrs;
  HEADER *tree;			/* top of thread tree */
  HASH *id_hash;		/* hash table by msg id */
  HASH *subj_hash;		/* hash table by subject */
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
#ifdef USE_IMAP
  void *data;			/* driver specific data */
#endif /* USE_IMAP */

  short magic;			/* mailbox type */

  unsigned int locked : 1;	/* is the mailbox locked? */
  unsigned int changed : 1;	/* mailbox has been modified */
  unsigned int readonly : 1;    /* don't allow changes to the mailbox */
  unsigned int dontwrite : 1;   /* dont write the mailbox on close */
  unsigned int append : 1;	/* mailbox is opened in append mode */
  unsigned int setgid : 1;
  unsigned int quiet : 1;	/* inhibit status messages? */
  unsigned int revsort : 1;	/* mailbox sorted in reverse? */
  unsigned int collapsed : 1;   /* are all threads collapsed? */
} CONTEXT;

typedef struct attachptr
{
  BODY *content;
  char *tree;
  int level;
  int num;
} ATTACHPTR;

typedef struct
{
  FILE *fpin;
  FILE *fpout;
  char *prefix;
  int flags;
} STATE;

/* flags for the STATE struct */
#define M_DISPLAY	(1<<0) /* output is displayed to the user */



#ifdef _PGPPATH
#define M_VERIFY	(1<<1) /* perform signature verification */
#endif



#define M_PENDINGPREFIX (1<<2) /* prefix to write, but character must follow */

#define state_set_prefix(s) ((s)->flags |= M_PENDINGPREFIX)
#define state_reset_prefix(s) ((s)->flags &= ~M_PENDINGPREFIX)
#define state_puts(x,y) fputs(x,(y)->fpout)
#define state_putc(x,y) fputc(x,(y)->fpout)

void state_prefix_putc(char, STATE *);

#include "protos.h"
#include "globals.h"
