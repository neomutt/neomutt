/* $Id$ */
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

#define FOREVER while (1)

#ifdef DEBUG
#define dprint(N,X) if(debuglevel>=N) fprintf X
#else
#define dprint(N,X) 
#endif

#define NONULL(x) x?x:""

#define MoreArgs(p) (*p->dptr && *p->dptr != ';' && *p->dptr != '#')

#define mutt_make_string(A,B,C,D,E) _mutt_make_string(A,B,C,D,E,0)
void _mutt_make_string (char *, size_t, const char *, CONTEXT *,
	HEADER *, format_flag);

int mutt_extract_token (BUFFER *, BUFFER *, int);

int mutt_add_string (BUFFER *, const char *);
int mutt_add_char (BUFFER *, char);

#define mutt_system(x) _mutt_system(x,0)
int _mutt_system (const char *, int);

#define mutt_next_thread(x) _mutt_aside_thread(x,1,0)
#define mutt_previous_thread(x) _mutt_aside_thread(x,0,0)
#define mutt_next_subthread(x) _mutt_aside_thread(x,1,1)
#define mutt_previous_subthread(x) _mutt_aside_thread(x,0,1)
int _mutt_aside_thread (HEADER *, short, short);

#define mutt_collapse_thread(x,y) _mutt_traverse_thread (x,y,M_THREAD_COLLAPSE)
#define mutt_uncollapse_thread(x,y) _mutt_traverse_thread (x,y,M_THREAD_UNCOLLAPSE)
#define mutt_get_hidden(x,y)_mutt_traverse_thread (x,y,M_THREAD_GET_HIDDEN) 
#define mutt_thread_contains_unread(x,y) _mutt_traverse_thread (x,y,M_THREAD_UNREAD)
#define mutt_thread_next_unread(x,y) _mutt_traverse_thread(x,y,M_THREAD_NEXT_UNREAD)
int _mutt_traverse_thread (CONTEXT *ctx, HEADER *hdr, int flag);

#define ISSPACE(c) isspace((unsigned char)c)

#define mutt_new_parameter() safe_calloc (1, sizeof (PARAMETER))
#define mutt_new_header() safe_calloc (1, sizeof (HEADER))
#define mutt_new_envelope() safe_calloc (1, sizeof (ENVELOPE))

typedef const char * format_t (char *, size_t, char, const char *, const char *, const char *, const char *, unsigned long, format_flag);

void mutt_FormatString (char *, size_t, const char *, format_t *, unsigned long, format_flag);

void mutt_update_encoding (BODY *a);

FILE *mutt_open_read (const char *, pid_t *);

void set_quadoption (int, int);
int query_quadoption (int, const char *);
int quadoption (int);

ADDRESS *mutt_lookup_alias (const char *s);
ADDRESS *mutt_remove_duplicates (ADDRESS *);
ADDRESS *mutt_expand_aliases (ADDRESS *);
ADDRESS *mutt_parse_adrlist (ADDRESS *, const char *);

BODY *mutt_dup_body (BODY *);
BODY *mutt_make_file_attach (const char *);
BODY *mutt_make_message_attach (CONTEXT *, HEADER *, int);
BODY *mutt_remove_multipart (BODY *);
BODY *mutt_make_multipart (BODY *);
BODY *mutt_new_body (void);
BODY *mutt_parse_multipart (FILE *, const char *, long, int);
BODY *mutt_parse_messageRFC822 (FILE *, BODY *);
BODY *mutt_read_mime_header (FILE *, int);

ENVELOPE *mutt_read_rfc822_header (FILE *, HEADER *, short);
HEADER *mutt_dup_header (HEADER *);

ATTACHPTR **mutt_gen_attach_list (BODY *, int, ATTACHPTR **, short *, short *, int, int);

time_t mutt_local_tz (void);
time_t mutt_mktime (struct tm *, int);
time_t is_from (const char *, char *, size_t);
time_t mutt_parse_date (char *, HEADER *);

const char *mutt_attach_fmt (
	char *dest,
	size_t destlen,
	char op,
	const char *src,
	const char *prefix,
	const char *ifstring,
	const char *elsestring,
	unsigned long data,
	format_flag flags);

char *mutt_expand_path (char *, size_t);
char *mutt_find_hook (int, const char *);
char *mutt_generate_boundary (void);
char *mutt_gen_msgid (void);
char *mutt_get_name (ADDRESS *);
char *mutt_get_parameter (const char *, PARAMETER *);
#ifdef _PGPPATH
char *mutt_pgp_hook (ADDRESS *);
#endif /* _PGPPATH */
char *mutt_make_date (char *, size_t);
char *mutt_quote_filename(const char *);
char *mutt_read_line (char *, size_t *, FILE *, int *);
char *mutt_strlower (char *);
char *mutt_skip_whitespace (char *);
char *mutt_substrcpy (char *, const char *, const char *, size_t);
char *mutt_substrdup (const char *, const char *);

const char *mutt_fqdn(short);

void mutt_add_child_pid (pid_t);
void mutt_alias_menu (char *, size_t, ALIAS *);
void mutt_block_signals (void);
void mutt_block_signals_system (void);
void mutt_body_handler (BODY *, STATE *);
void mutt_bounce_message (HEADER *, ADDRESS *);
void mutt_buffy (char *);
void mutt_check_rescore (CONTEXT *);
void mutt_clear_error (void);
void mutt_create_alias (ENVELOPE *, ADDRESS *);
void mutt_decode_attachment (BODY *, STATE *);
void mutt_default_save (char *, size_t, HEADER *);
void mutt_display_address (ADDRESS *);
void mutt_edit_file (const char *, const char *);
void mutt_edit_headers (const char *, const char *, HEADER *, char *, size_t);
void mutt_curses_error (const char *, ...);
void mutt_enter_command (void);
void mutt_exit (int);
void mutt_expand_file_fmt (char *, size_t, const char *, const char *);
void mutt_expand_fmt (char *, size_t, const char *, const char *);
void mutt_expand_link (char *, const char *, const char *);
void mutt_fetchPopMail (void);
void mutt_folder_hook (char *);
void mutt_free_alias (ALIAS **);
void mutt_free_body (BODY **);
void mutt_free_color (int fg, int bg);
void mutt_free_envelope (ENVELOPE **);
void mutt_free_header (HEADER **);
void mutt_free_parameter (PARAMETER **);
void mutt_generate_header (char *, size_t, HEADER *, int);
void mutt_help (int);
void mutt_linearize_tree (CONTEXT *, int);
void mutt_make_help (char *, size_t, char *, int, int);
void mutt_message (const char *, ...);
void mutt_message_to_7bit (BODY *, FILE *);
void mutt_mktemp (char *);
void mutt_nocurses_error (const char *, ...);
void mutt_normalize_time (struct tm *);
void mutt_parse_mime_message (CONTEXT *ctx, HEADER *);
void mutt_parse_part (FILE *, BODY *);
void mutt_pipe_message_to_state (HEADER *, STATE *);
void mutt_perror (const char *);
void mutt_prepare_envelope (ENVELOPE *);
void mutt_pretty_mailbox (char *);
void mutt_pretty_size (char *, size_t, long);
void mutt_print_message (HEADER *);
void mutt_remove_trailing_ws (char *);
void mutt_query_exit (void);
void mutt_query_menu (char *, size_t);
void mutt_safe_path (char *s, size_t l, ADDRESS *a);
void mutt_sanitize_filename (char *);
void mutt_save_path (char *s, size_t l, ADDRESS *a);
void mutt_score_message (HEADER *);
void mutt_select_fcc (char *, size_t, HEADER *);
void mutt_select_file (char *, size_t, int);
void mutt_send_hook (HEADER *);
void mutt_set_flag (CONTEXT *, HEADER *, int, int);
void mutt_set_followup_to (ENVELOPE *);
void mutt_shell_escape (void);
void mutt_show_error (void);
void mutt_signal_init (void);
void mutt_stamp_attachment (BODY *a);
void mutt_tabs_to_spaces (char *);
void mutt_tag_set_flag (int, int);
void mutt_unblock_signals (void);
void mutt_unblock_signals_system (int);
void mutt_unlink (const char *);
void mutt_update_encoding (BODY *a);
void mutt_update_tree (ATTACHPTR **, short);
void mutt_version (void);
void mutt_view_attachments (HEADER *);
void mutt_set_virtual (CONTEXT *);

int mutt_addr_is_user (ADDRESS *);
int mutt_alias_complete (char *, size_t);
int mutt_alloc_color (int fg, int bg);
int mutt_any_key_to_continue (const char *);
int mutt_buffy_check (int);
int mutt_buffy_notify (void);
int mutt_builtin_editor (const char *, HEADER *, HEADER *);
int mutt_can_decode (BODY *);
int mutt_change_flag (HEADER *, int);
int mutt_check_encoding (const char *);
int mutt_check_key (const char *);
int mutt_check_menu (const char *);
int mutt_check_mime_type (const char *);
int mutt_check_month (const char *);
int mutt_check_overwrite (const char *, const char *, char *, size_t, int);
int mutt_command_complete (char *, size_t, int, int);
int mutt_var_value_complete (char *, size_t, int);
int mutt_complete (char *);
int mutt_compose_attachment (BODY *a);
int mutt_copy_bytes (FILE *, FILE *, size_t);
int mutt_copy_stream (FILE *, FILE *);
int mutt_decode_save_attachment (FILE *, BODY *, char *, int, int);
int mutt_display_message (HEADER *h);
int mutt_edit_attachment(BODY *);
int mutt_prepare_edit_message(CONTEXT *, HEADER *, HEADER *);
int mutt_enter_fname (const char *, char *, size_t, int *, int);
int mutt_enter_string (unsigned char *, size_t, int, int, int);
int mutt_get_field (char *, char *, size_t, int);
int mutt_get_password (char *, char *, size_t);
int mutt_get_postponed (CONTEXT *, HEADER *, HEADER **, char *, size_t);
int mutt_get_tmp_attachment (BODY *);
int mutt_index_menu (void);
int mutt_invoke_sendmail (ADDRESS *, ADDRESS *, ADDRESS *, const char *, int);
int mutt_is_autoview (char *);
int mutt_is_mail_list (ADDRESS *);
int mutt_is_message_type(int, const char *);
int mutt_is_list_recipient (ADDRESS *a);
int mutt_is_text_type (int, char *);
int mutt_is_valid_mailbox (const char *);
int mutt_multi_choice (char *prompt, char *letters);
int mutt_needs_mailcap (BODY *);
int mutt_num_postponed (void);
int mutt_parse_bind (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_exec (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_color (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_uncolor (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_hook (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_macro (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_mailboxes (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_mono (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_unmono (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_push (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_rc_line (/* const */ char *, BUFFER *, BUFFER *);
int mutt_parse_score (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_parse_unscore (BUFFER *, BUFFER *, unsigned long, BUFFER *);
int mutt_pattern_func (int, char *);
int mutt_pipe_attachment (FILE *, BODY *, const char *, char *); 
int mutt_pipe_message (HEADER *);
int mutt_print_attachment (FILE *, BODY *);
int mutt_query_complete (char *, size_t);
int mutt_save_attachment (FILE *, BODY *, char *, int, HEADER *);
int mutt_save_message (HEADER *, int, int, int, int *);
int mutt_search_command (int, int);
int mutt_compose_menu (HEADER *, char *, size_t, HEADER *);
int mutt_strcmp (const char *, const char *);
int mutt_strcasecmp (const char *, const char *);
int mutt_strncmp (const char *, const char *, size_t);
int mutt_strncasecmp (const char *, const char *, size_t);
size_t mutt_strlen (const char *);
int mutt_thread_set_flag (HEADER *, int, int, int);
int mutt_user_is_recipient (HEADER *);
int mutt_view_attachment (FILE*, BODY *, int);
int mutt_wait_filter (pid_t);
int mutt_which_case (const char *);
int mutt_write_fcc (const char *path, HEADER *hdr, const char *msgid, int, char *);
int mutt_write_mime_body (BODY *, FILE *);
int mutt_write_mime_header (BODY *, FILE *);
int mutt_write_rfc822_header (FILE *, ENVELOPE *, BODY *, int);
int mutt_yesorno (const char *, int);
void mutt_set_header_color(CONTEXT *, HEADER *);
int mutt_save_confirm (const char  *, struct stat *);

int mh_valid_message (const char *);

pid_t mutt_create_filter (const char *, FILE **, FILE **, FILE **);
pid_t mutt_create_filter_fd (const char *, FILE **, FILE **, FILE **, int, int, int);

#define FREE(x) safe_free((void **)x)

char *safe_strdup (const char *);
void *safe_calloc (size_t, size_t);
void *safe_malloc (unsigned int);
void safe_realloc (void **, size_t);
void safe_free (void **);

int safe_open (const char *, int);
int safe_symlink (const char *, const char *);
FILE *safe_fopen (const char *, const char *);

ADDRESS *alias_reverse_lookup (ADDRESS *);

#define strfcpy(A,B,C) strncpy(A,B,C), *(A+(C)-1)=0

/* this macro must check for *c == 0 since isspace(0) has unreliable behavior
   on some systems */
#define SKIPWS(c) while (*(c) && isspace ((unsigned char) *(c))) c++;

#ifdef LOCALES_HACK
#define IsPrint(c) (isprint((unsigned char)(c)) || \
	((unsigned char)(c) >= 0xa0))
#else
#define IsPrint(c) (isprint((unsigned char)(c)) || \
	(option (OPTLOCALES) ? 0 : \
	((unsigned char)(c) >= 0xa0)))
#endif

#define new_pattern() safe_calloc(1, sizeof (pattern_t))

int mutt_pattern_exec (struct pattern_t *pat, pattern_exec_flag flags, CONTEXT *ctx, HEADER *h);
pattern_t *mutt_pattern_comp (/* const */ char *s, int flags, BUFFER *err);
void mutt_check_simple (char *s, size_t len, const char *simple);
void mutt_pattern_free (pattern_t **pat);

/* ----------------------------------------------------------------------------
 * Prototypes for broken systems
 */

#ifdef HAVE_SRAND48
#define LRAND lrand48
#define SRAND srand48
#define DRAND drand48
#else
#define LRAND rand
#define SRAND srand
#define DRAND (double)rand
#endif /* HAVE_SRAND48 */

/* HP-UX, ConvexOS and UNIXware don't have this macro */
#ifndef S_ISLNK
#define S_ISLNK(x) (((x) & S_IFMT) == S_IFLNK ? 1 : 0)
#endif

int getdnsdomainname (char *, size_t);

/* According to SCO support, this is how to detect SCO */
#if defined (_M_UNIX) || defined (M_OS)
#define SCO
#endif

/* SCO Unix uses chsize() instead of ftruncate() */
#ifndef HAVE_FTRUNCATE
#define ftruncate chsize
#endif

#ifndef HAVE_SNPRINTF
extern int snprintf (char *, size_t, const char *, ...);
#endif

#ifndef HAVE_VSNPRINTF
extern int vsnprintf (char *, size_t, const char *, va_list);
#endif

#ifndef HAVE_STRERROR
#ifndef STDC_HEADERS
extern int sys_nerr;
extern char *sys_errlist[];
#endif

#define strerror(x) ((x) > 0 && (x) < sys_nerr) ? sys_errlist[(x)] : 0
#endif /* !HAVE_STRERROR */

/* AIX doesn't define these in any headers (sigh) */
int strcasecmp (const char *, const char *);
int strncasecmp (const char *, const char *, size_t);

#ifdef _AIX
int setegid (gid_t);
#endif /* _AIX */

#ifndef STDC_HEADERS
extern FILE *fdopen ();
extern int system ();
extern int puts ();
extern int fputs ();
extern int fputc ();
extern int fseek ();
extern char *strchr ();
extern int getopt ();
extern int fputs ();
extern int fputc ();
extern int fclose ();
extern int fprintf();
extern int printf ();
extern int fgetc ();
extern int tolower ();
extern int toupper ();
extern int sscanf ();
extern size_t fread ();
extern size_t fwrite ();
extern int system ();
extern int rename ();
extern time_t time ();
extern struct tm *localtime ();
extern char *asctime ();
extern char *strpbrk ();
extern int fflush ();
extern long lrand48 ();
extern void srand48 ();
extern time_t mktime ();
extern int vsprintf ();
extern int ungetc ();
extern char *mktemp ();
extern int ftruncate ();
extern void *memset ();
extern int pclose ();
extern int socket ();
extern int connect ();
extern size_t strftime ();
extern int lstat ();
extern void rewind ();
extern int readlink ();

/* IRIX barfs on empty var decls because the system include file uses elipsis
   in the declaration.  So declare all the args to avoid compiler errors.  This
   should be harmless on other systems.  */
int ioctl (int, int, ...);

#endif

/* unsorted */
void ci_bounce_message (HEADER *, int *);
void ci_send_message (int, HEADER *, char *, CONTEXT *, HEADER *);
void ci_attach (BODY *);
