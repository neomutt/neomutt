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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

#ifdef _MAKEDOC
# include "config.h"
#else
# include "sort.h"
#endif

#include "buffy.h"

#ifndef _MAKEDOC
#define DT_MASK		0x0f
#define DT_BOOL		1 /* boolean option */
#define DT_NUM		2 /* a number */
#define DT_STR		3 /* a string */
#define DT_PATH		4 /* a pathname */
#define DT_QUAD		5 /* quad-option (yes/no/ask-yes/ask-no) */
#define DT_SORT		6 /* sorting methods */
#define DT_RX		7 /* regular expressions */
#define DT_MAGIC	8 /* mailbox type */
#define DT_SYN		9 /* synonym for another variable */
#define DT_ADDR	       10 /* e-mail address */

#define DTYPE(x) ((x) & DT_MASK)

/* subtypes */
#define DT_SUBTYPE_MASK	0xf0
#define DT_SORT_ALIAS	0x10
#define DT_SORT_BROWSER 0x20
#define DT_SORT_KEYS	0x40
#define DT_SORT_AUX	0x80

/* flags to parse_set() */
#define M_SET_INV	(1<<0)	/* default is to invert all vars */
#define M_SET_UNSET	(1<<1)	/* default is to unset all vars */
#define M_SET_RESET	(1<<2)	/* default is to reset all vars to default */

/* forced redraw/resort types */
#define R_NONE		0
#define R_INDEX		(1<<0)
#define R_PAGER		(1<<1)
#define R_RESORT	(1<<2)	/* resort the mailbox */
#define R_RESORT_SUB	(1<<3)	/* resort subthreads */
#define R_RESORT_INIT	(1<<4)  /* resort from scratch */
#define R_TREE		(1<<5)  /* redraw the thread tree */
#define R_BOTH		(R_INDEX | R_PAGER)
#define R_RESORT_BOTH	(R_RESORT | R_RESORT_SUB)

struct option_t
{
  char *option;
  short type;
  short flags;
  unsigned long data;
  unsigned long init; /* initial value */
};

#define UL (unsigned long)

#endif /* _MAKEDOC */

#ifndef ISPELL
#define ISPELL "ispell"
#endif

/* build complete documentation */

#ifdef _MAKEDOC
# ifndef USE_IMAP
#  define USE_IMAP
# endif
# ifndef MIXMASTER
#  define MIXMASTER "mixmaster"
# endif
# ifndef USE_POP
#  define USE_POP
# endif
# ifndef USE_SSL
#  define USE_SSL
# endif
# ifndef USE_SOCKET
#  define USE_SOCKET
# endif
#endif

struct option_t MuttVars[] = {
  /*++*/
  { "abort_nosubject",	DT_QUAD, R_NONE, OPT_SUBJECT, M_ASKYES },
  /*
  ** .pp
  ** If set to \fIyes\fP, when composing messages and no subject is given
  ** at the subject prompt, composition will be aborted.  If set to
  ** \fIno\fP, composing messages with no subject given at the subject
  ** prompt will never be aborted.
  */
  { "abort_unmodified",	DT_QUAD, R_NONE, OPT_ABORT, M_YES },
  /*
  ** .pp
  ** If set to \fIyes\fP, composition will automatically abort after
  ** editing the message body if no changes are made to the file (this
  ** check only happens after the \fIfirst\fP edit of the file).  When set
  ** to \fIno\fP, composition will never be aborted.
  */
  { "alias_file",	DT_PATH, R_NONE, UL &AliasFile, UL "~/.muttrc" },
  /*
  ** .pp
  ** The default file in which to save aliases created by the 
  ** ``$create-alias'' function.
  ** .pp
  ** \fBNote:\fP Mutt will not automatically source this file; you must
  ** explicitly use the ``$source'' command for it to be executed.
  */
  { "alias_format",	DT_STR,  R_NONE, UL &AliasFmt, UL "%4n %2f %t %-10a   %r" },
  /*
  ** .pp
  ** Specifies the format of the data displayed for the `alias' menu.  The
  ** following printf(3)-style sequences are available:
  ** .pp
  ** .dl
  ** .dt %a .dd alias name
  ** .dt %f .dd flags - currently, a "d" for an alias marked for deletion
  ** .dt %n .dd index number
  ** .dt %r .dd address which alias expands to
  ** .dt %t .dd character which indicates if the alias is tagged for inclusion
  ** .de
  */
  { "allow_8bit",	DT_BOOL, R_NONE, OPTALLOW8BIT, 1 },
  /*
  ** .pp
  ** Controls whether 8-bit data is converted to 7-bit using either Quoted-
  ** Printable or Base64 encoding when sending mail.
  */
  { "allow_ansi",      DT_BOOL, R_NONE, OPTALLOWANSI, 0 },
  /*
  ** .pp
  ** Controls whether ANSI color codes in messages (and color tags in 
  ** rich text messages) are to be interpreted.
  ** Messages containing these codes are rare, but if this option is set,
  ** their text will be colored accordingly. Note that this may override
  ** your color choices, and even present a security problem, since a
  ** message could include a line like "[-- PGP output follows ..." and
  ** give it the same color as your attachment color.
  */
  { "arrow_cursor",	DT_BOOL, R_BOTH, OPTARROWCURSOR, 0 },
  /*
  ** .pp
  ** When set, an arrow (``->'') will be used to indicate the current entry
  ** in menus instead of highlighting the whole line.  On slow network or modem
  ** links this will make response faster because there is less that has to
  ** be redrawn on the screen when moving to the next or previous entries
  ** in the menu.
  */
  { "ascii_chars",	DT_BOOL, R_BOTH, OPTASCIICHARS, 0 },
  /*
  ** .pp
  ** If set, Mutt will use plain ASCII characters when displaying thread
  ** and attachment trees, instead of the default \fIACS\fP characters.
  */
  { "askbcc",		DT_BOOL, R_NONE, OPTASKBCC, 0 },
  /*
  ** .pp
  ** If set, Mutt will prompt you for blind-carbon-copy (Bcc) recipients
  ** before editing an outgoing message.
  */
  { "askcc",		DT_BOOL, R_NONE, OPTASKCC, 0 },
  /*
  ** .pp
  ** If set, Mutt will prompt you for carbon-copy (Cc) recipients before
  ** editing the body of an outgoing message.
  */  
  { "attach_format",	DT_STR,  R_NONE, UL &AttachFormat, UL "%u%D%I %t%4n %T%.40d%> [%.7m/%.10M, %.6e%?C?, %C?, %s] " },
  /*
  ** .pp
  ** This variable describes the format of the `attachment' menu.  The
  ** following printf-style sequences are understood:
  ** .pp
  ** .dl
  ** .dt %C  .dd charset
  ** .dt %c  .dd requires charset conversion (n or c)
  ** .dt %D  .dd deleted flag
  ** .dt %d  .dd description
  ** .dt %e  .dd MIME content-transfer-encoding
  ** .dt %f  .dd filename
  ** .dt %I  .dd disposition (I=inline, A=attachment)
  ** .dt %m  .dd major MIME type
  ** .dt %M  .dd MIME subtype
  ** .dt %n  .dd attachment number
  ** .dt %s  .dd size
  ** .dt %t  .dd tagged flag
  ** .dt %T  .dd graphic tree characters
  ** .dt %u  .dd unlink (=to delete) flag
  ** .dt %>X .dd right justify the rest of the string and pad with character "X"
  ** .dt %|X .dd pad to the end of the line with character "X"
  ** .de
  */
  { "attach_sep",	DT_STR,	 R_NONE, UL &AttachSep, UL "\n" },
  /*
  ** .pp
  ** The separator to add between attachments when operating (saving,
  ** printing, piping, etc) on a list of tagged attachments.
  */
  { "attach_split",	DT_BOOL, R_NONE, OPTATTACHSPLIT, 1 },
  /*
  ** .pp
  ** If this variable is unset, when operating (saving, printing, piping,
  ** etc) on a list of tagged attachments, Mutt will concatenate the
  ** attachments and will operate on them as a single attachment. The
  ** ``$$attach_sep'' separator is added after each attachment. When set,
  ** Mutt will operate on the attachments one by one.
  */
  { "attribution",	DT_STR,	 R_NONE, UL &Attribution, UL "On %d, %n wrote:" },
  /*
  ** .pp
  ** This is the string that will precede a message which has been included
  ** in a reply.  For a full listing of defined printf()-like sequences see
  ** the section on ``$$index_format''.
  */
  { "autoedit",		DT_BOOL, R_NONE, OPTAUTOEDIT, 0 },
  /*
  ** .pp
  ** When set along with ``$$edit_headers'', Mutt will skip the initial
  ** send-menu and allow you to immediately begin editing the body of your
  ** message.  The send-menu may still be accessed once you have finished
  ** editing the body of your message.
  ** .pp
  ** Also see ``$$fast_reply''.
  */
  { "auto_tag",		DT_BOOL, R_NONE, OPTAUTOTAG, 0 },
  /*
  ** .pp
  ** When set, functions in the \fIindex\fP menu which affect a message
  ** will be applied to all tagged messages (if there are any).  When
  ** unset, you must first use the tag-prefix function (default: ";") to
  ** make the next function apply to all tagged messages.
  */
  { "beep",		DT_BOOL, R_NONE, OPTBEEP, 1 },
  /*
  ** .pp
  ** When this variable is set, mutt will beep when an error occurs.
  */
  { "beep_new",		DT_BOOL, R_NONE, OPTBEEPNEW, 0 },
  /*
  ** .pp
  ** When this variable is set, mutt will beep whenever it prints a message
  ** notifying you of new mail.  This is independent of the setting of the
  ** ``$$beep'' variable.
  */
  { "bounce",	DT_QUAD, R_NONE, OPT_BOUNCE, M_ASKYES },
  /*
  ** .pp
  ** Controls whether you will be asked to confirm bouncing messages.
  ** If set to \fIyes\fP you don't get asked if you want to bounce a
  ** message. Setting this variable to \fIno\fP is not generally useful,
  ** and thus not recommended, because you are unable to bounce messages.
  */
  { "bounce_delivered", DT_BOOL, R_NONE, OPTBOUNCEDELIVERED, 1 },
  /*
  ** .pp
  ** When this variable is set, mutt will include Delivered-To headers when
  ** bouncing messages.  Postfix users may wish to unset this variable.
  */
  { "charset",		DT_STR,	 R_NONE, UL &Charset, UL 0 },
  /*
  ** .pp
  ** Character set your terminal uses to display and enter textual data.
  */
  { "check_new",	DT_BOOL, R_NONE, OPTCHECKNEW, 1 },
  /*
  ** .pp
  ** \fBNote:\fP this option only affects \fImaildir\fP and \fIMH\fP style
  ** mailboxes.
  ** .pp
  ** When \fIset\fP, Mutt will check for new mail delivered while the
  ** mailbox is open.  Especially with MH mailboxes, this operation can
  ** take quite some time since it involves scanning the directory and
  ** checking each file to see if it has already been looked at.  If
  ** \fIcheck_new\fP is \fIunset\fP, no check for new mail is performed
  ** while the mailbox is open.
  */
  { "collapse_unread",	DT_BOOL, R_NONE, OPTCOLLAPSEUNREAD, 1 },
  /*
  ** .pp
  ** When \fIunset\fP, Mutt will not collapse a thread if it contains any
  ** unread messages.
  */
  { "uncollapse_jump", 	DT_BOOL, R_NONE, OPTUNCOLLAPSEJUMP, 0 },
  /*
  ** .pp
  ** When \fIset\fP, Mutt will jump to the next unread message, if any,
  ** when the current thread is \fIun\fPcollapsed.
  */
  { "compose_format",	DT_STR,	 R_BOTH, UL &ComposeFormat, UL "-- Mutt: Compose  [Approx. msg size: %l   Atts: %a]%>-" },
  /*
  ** .pp
  ** Controls the format of the status line displayed in the \fICompose\fP
  ** menu.  This string is similar to ``$$status_format'', but has its own
  ** set of printf()-like sequences:
  ** .pp
  ** .dl
  ** .dt %a .dd total number of attachments 
  ** .dt %h .dd local hostname
  ** .dt %l .dd approximate size (in bytes) of the current message
  ** .dt %v .dd Mutt version string
  ** .de
  ** .pp
  ** See the text describing the ``$$status_format'' option for more 
  ** information on how to set ``$$compose_format''.
  */
  { "config_charset",	DT_STR,  R_NONE, UL &ConfigCharset, UL 0 },
  /*
  ** .pp
  ** When defined, Mutt will recode commands in rc files from this
  ** encoding.
  */
  { "confirmappend",	DT_BOOL, R_NONE, OPTCONFIRMAPPEND, 1 },
  /*
  ** .pp
  ** When set, Mutt will prompt for confirmation when appending messages to
  ** an existing mailbox.
  */
  { "confirmcreate",	DT_BOOL, R_NONE, OPTCONFIRMCREATE, 1 },
  /*
  ** .pp
  ** When set, Mutt will prompt for confirmation when saving messages to a
  ** mailbox which does not yet exist before creating it.
  */
  { "connect_timeout",	DT_NUM,	R_NONE, UL &ConnectTimeout, 30 },
  /*
  ** .pp
  ** Causes Mutt to timeout a network connection (for IMAP or POP) after this
  ** many seconds if the connection is not able to be established.  A negative
  ** value causes Mutt to wait indefinitely for the connection to succeed.
  */
  { "content_type",	DT_STR, R_NONE, UL &ContentType, UL "text/plain" },
  /*
  ** .pp
  ** Sets the default Content-Type for the body of newly composed messages.
  */
  { "copy",		DT_QUAD, R_NONE, OPT_COPY, M_YES },
  /*
  ** .pp
  ** This variable controls whether or not copies of your outgoing messages
  ** will be saved for later references.  Also see ``$$record'',
  ** ``$$save_name'', ``$$force_name'' and ``$fcc-hook''.
  */
  
  { "crypt_use_gpgme",  DT_BOOL, R_NONE, OPTCRYPTUSEGPGME, 0 },
  /*
  ** .pp
  ** This variable controls the use the GPGME enabled crypto backends.
  ** If it is set and Mutt was build with gpgme support, the gpgme code for
  ** S/MIME and PGP will be used instead of the classic code.  Note, that
  ** you need to use this option in .muttrc as it won't have any effect when 
  ** used interactively.
  */
  
  { "crypt_autopgp",	DT_BOOL, R_NONE, OPTCRYPTAUTOPGP, 1 },
  /*
  ** .pp
  ** This variable controls whether or not mutt may automatically enable
  ** PGP encryption/signing for messages.  See also ``$$crypt_autoencrypt'',
  ** ``$$crypt_replyencrypt'',
  ** ``$$crypt_autosign'', ``$$crypt_replysign'' and ``$$smime_is_default''.
  */
  { "crypt_autosmime",	DT_BOOL, R_NONE, OPTCRYPTAUTOSMIME, 1 },
  /*
  ** .pp
  ** This variable controls whether or not mutt may automatically enable
  ** S/MIME encryption/signing for messages. See also ``$$crypt_autoencrypt'',
  ** ``$$crypt_replyencrypt'',
  ** ``$$crypt_autosign'', ``$$crypt_replysign'' and ``$$smime_is_default''.
  */
  { "date_format",	DT_STR,	 R_BOTH, UL &DateFmt, UL "!%a, %b %d, %Y at %I:%M:%S%p %Z" },
  /*
  ** .pp
  ** This variable controls the format of the date printed by the ``%d''
  ** sequence in ``$$index_format''.  This is passed to the \fIstrftime\fP
  ** call to process the date. See the man page for \fIstrftime(3)\fP for
  ** the proper syntax.
  ** .pp
  ** Unless the first character in the string is a bang (``!''), the month
  ** and week day names are expanded according to the locale specified in
  ** the variable ``$$locale''. If the first character in the string is a
  ** bang, the bang is discarded, and the month and week day names in the
  ** rest of the string are expanded in the \fIC\fP locale (that is in US
  ** English).
  */  
  { "default_hook",	DT_STR,	 R_NONE, UL &DefaultHook, UL "~f %s !~P | (~P ~C %s)" },
  /*
  ** .pp
  ** This variable controls how send-hooks, message-hooks, save-hooks,
  ** and fcc-hooks will
  ** be interpreted if they are specified with only a simple regexp,
  ** instead of a matching pattern.  The hooks are expanded when they are
  ** declared, so a hook will be interpreted according to the value of this
  ** variable at the time the hook is declared.  The default value matches
  ** if the message is either from a user matching the regular expression
  ** given, or if it is from you (if the from address matches
  ** ``alternates'') and is to or cc'ed to a user matching the given
  ** regular expression.
  */
  { "delete",		DT_QUAD, R_NONE, OPT_DELETE, M_ASKYES },
  /*
  ** .pp
  ** Controls whether or not messages are really deleted when closing or
  ** synchronizing a mailbox.  If set to \fIyes\fP, messages marked for
  ** deleting will automatically be purged without prompting.  If set to
  ** \fIno\fP, messages marked for deletion will be kept in the mailbox.
  */
  { "delete_untag",	DT_BOOL, R_NONE, OPTDELETEUNTAG, 1 },
  /*
  ** .pp
  ** If this option is \fIset\fP, mutt will untag messages when marking them
  ** for deletion.  This applies when you either explicitly delete a message,
  ** or when you save it to another folder.
  */
  { "digest_collapse",	DT_BOOL, R_NONE, OPTDIGESTCOLLAPSE, 1},
  /*
  ** .pp
  ** If this option is \fIset\fP, mutt's received-attachments menu will not show the subparts of
  ** individual messages in a multipart/digest.  To see these subparts, press 'v' on that menu.
  */
  { "display_filter",	DT_PATH, R_PAGER, UL &DisplayFilter, UL "" },
  /*
  ** .pp
  ** When set, specifies a command used to filter messages.  When a message
  ** is viewed it is passed as standard input to $$display_filter, and the
  ** filtered message is read from the standard output.
  */
#if defined(DL_STANDALONE) && defined(USE_DOTLOCK)
  { "dotlock_program",  DT_PATH, R_NONE, UL &MuttDotlock, UL BINDIR "/mutt_dotlock" },
  /*
  ** .pp
  ** Contains the path of the mutt_dotlock (8) binary to be used by
  ** mutt.
  */
#endif
  { "dsn_notify",	DT_STR,	 R_NONE, UL &DsnNotify, UL "" },
  /*
  ** .pp
  ** \fBNote:\fP you should not enable this unless you are using Sendmail
  ** 8.8.x or greater.
  ** .pp
  ** This variable sets the request for when notification is returned.  The
  ** string consists of a comma separated list (no spaces!) of one or more
  ** of the following: \fInever\fP, to never request notification,
  ** \fIfailure\fP, to request notification on transmission failure,
  ** \fIdelay\fP, to be notified of message delays, \fIsuccess\fP, to be
  ** notified of successful transmission.
  ** .pp
  ** Example: set dsn_notify="failure,delay"
  */
  { "dsn_return",	DT_STR,	 R_NONE, UL &DsnReturn, UL "" },
  /*
  ** .pp
  ** \fBNote:\fP you should not enable this unless you are using Sendmail
  ** 8.8.x or greater.
  ** .pp
  ** This variable controls how much of your message is returned in DSN
  ** messages.  It may be set to either \fIhdrs\fP to return just the
  ** message header, or \fIfull\fP to return the full message.
  ** .pp
  ** Example: set dsn_return=hdrs
  */
  { "duplicate_threads",	DT_BOOL, R_RESORT|R_RESORT_INIT|R_INDEX, OPTDUPTHREADS, 1 },
  /*
  ** .pp
  ** This variable controls whether mutt, when sorting by threads, threads
  ** messages with the same message-id together.  If it is set, it will indicate
  ** that it thinks they are duplicates of each other with an equals sign
  ** in the thread diagram.
  */
  { "edit_headers",	DT_BOOL, R_NONE, OPTEDITHDRS, 0 },
  /*
  ** .pp
  ** This option allows you to edit the header of your outgoing messages
  ** along with the body of your message.
  */
  { "edit_hdrs",	DT_SYN,  R_NONE, UL "edit_headers", 0 },
  /*
  */  
  { "editor",		DT_PATH, R_NONE, UL &Editor, 0 },
  /*
  ** .pp
  ** This variable specifies which editor is used by mutt.
  ** It defaults to the value of the VISUAL, or EDITOR, environment
  ** variable, or to the string "vi" if neither of those are set.
  */
  { "encode_from",	DT_BOOL, R_NONE, OPTENCODEFROM, 0 },
  /*
  ** .pp
  ** When \fIset\fP, mutt will quoted-printable encode messages when
  ** they contain the string "From " in the beginning of a line.
  ** Useful to avoid the tampering certain mail delivery and transport
  ** agents tend to do with messages.
  */
  { "envelope_from", 	DT_BOOL, R_NONE, OPTENVFROM, 0 },
  /*
  ** .pp
  ** When \fIset\fP, mutt will try to derive the message's \fIenvelope\fP
  ** sender from the "From:" header.  Note that this information is passed 
  ** to sendmail command using the "-f" command line switch, so don't set this
  ** option if you are using that switch in $$sendmail yourself,
  ** or if the sendmail on your machine doesn't support that command
  ** line switch.
  */
  { "escape",		DT_STR,	 R_NONE, UL &EscChar, UL "~" },
  /*
  ** .pp
  ** Escape character to use for functions in the builtin editor.
  */
  { "fast_reply",	DT_BOOL, R_NONE, OPTFASTREPLY, 0 },
  /*
  ** .pp
  ** When set, the initial prompt for recipients and subject are skipped
  ** when replying to messages, and the initial prompt for subject is
  ** skipped when forwarding messages.
  ** .pp
  ** \fBNote:\fP this variable has no effect when the ``$$autoedit''
  ** variable is set.
  */
  { "fcc_attach",	DT_BOOL, R_NONE, OPTFCCATTACH, 1 },
  /*
  ** .pp
  ** This variable controls whether or not attachments on outgoing messages
  ** are saved along with the main body of your message.
  */
  { "fcc_clear",	DT_BOOL, R_NONE, OPTFCCCLEAR, 0 },
  /*
  ** .pp
  ** When this variable is set, FCCs will be stored unencrypted and
  ** unsigned, even when the actual message is encrypted and/or
  ** signed.
  ** (PGP only)
  */
  { "folder",		DT_PATH, R_NONE, UL &Maildir, UL "~/Mail" },
  /*
  ** .pp
  ** Specifies the default location of your mailboxes.  A `+' or `=' at the
  ** beginning of a pathname will be expanded to the value of this
  ** variable.  Note that if you change this variable from the default
  ** value you need to make sure that the assignment occurs \fIbefore\fP
  ** you use `+' or `=' for any other variables since expansion takes place
  ** during the `set' command.
  */
  { "folder_format",	DT_STR,	 R_INDEX, UL &FolderFormat, UL "%2C %t %N %F %2l %-8.8u %-8.8g %8s %d %f" },
  /*
  ** .pp
  ** This variable allows you to customize the file browser display to your
  ** personal taste.  This string is similar to ``$$index_format'', but has
  ** its own set of printf()-like sequences:
  ** .pp
  ** .dl
  ** .dt %C  .dd current file number
  ** .dt %d  .dd date/time folder was last modified
  ** .dt %f  .dd filename
  ** .dt %F  .dd file permissions
  ** .dt %g  .dd group name (or numeric gid, if missing)
  ** .dt %l  .dd number of hard links
  ** .dt %N  .dd N if folder has new mail, blank otherwise
  ** .dt %s  .dd size in bytes
  ** .dt %t  .dd * if the file is tagged, blank otherwise
  ** .dt %u  .dd owner name (or numeric uid, if missing)
  ** .dt %>X .dd right justify the rest of the string and pad with character "X"
  ** .dt %|X .dd pad to the end of the line with character "X"
  ** .de
  */
  { "followup_to",	DT_BOOL, R_NONE, OPTFOLLOWUPTO, 1 },
  /*
  ** .pp
  ** Controls whether or not the \fIMail-Followup-To\fP header field is
  ** generated when sending mail.  When \fIset\fP, Mutt will generate this
  ** field when you are replying to a known mailing list, specified with
  ** the ``subscribe'' or ``$lists'' commands.
  ** .pp
  ** This field has two purposes.  First, preventing you from
  ** receiving duplicate copies of replies to messages which you send
  ** to mailing lists, and second, ensuring that you do get a reply
  ** separately for any messages sent to known lists to which you are
  ** not subscribed.  The header will contain only the list's address
  ** for subscribed lists, and both the list address and your own
  ** email address for unsubscribed lists.  Without this header, a
  ** group reply to your message sent to a subscribed list will be
  ** sent to both the list and your address, resulting in two copies
  ** of the same email for you.
  */
  { "force_name",	DT_BOOL, R_NONE, OPTFORCENAME, 0 },
  /*
  ** .pp
  ** This variable is similar to ``$$save_name'', except that Mutt will
  ** store a copy of your outgoing message by the username of the address
  ** you are sending to even if that mailbox does not exist.
  ** .pp
  ** Also see the ``$$record'' variable.
  */
  { "forward_decode",	DT_BOOL, R_NONE, OPTFORWDECODE, 1 },
  /*
  ** .pp
  ** Controls the decoding of complex MIME messages into text/plain when
  ** forwarding a message.  The message header is also RFC2047 decoded.
  ** This variable is only used, if ``$$mime_forward'' is \fIunset\fP,
  ** otherwise ``$$mime_forward_decode'' is used instead.
  */
  { "forw_decode",	DT_SYN,  R_NONE, UL "forward_decode", 0 },
  /*
  */
  { "forward_edit",	DT_QUAD, R_NONE, OPT_FORWEDIT, M_YES },
  /*
  ** .pp
  ** This quadoption controls whether or not the user is automatically
  ** placed in the editor when forwarding messages.  For those who always want
  ** to forward with no modification, use a setting of ``no''.
  */
  { "forward_format",	DT_STR,	 R_NONE, UL &ForwFmt, UL "[%a: %s]" },
  /*
  ** .pp
  ** This variable controls the default subject when forwarding a message.
  ** It uses the same format sequences as the ``$$index_format'' variable.
  */
  { "forw_format",	DT_SYN,  R_NONE, UL "forward_format", 0 },  
  /*
  */
  { "forward_quote",	DT_BOOL, R_NONE, OPTFORWQUOTE, 0 },
  /*
  ** .pp
  ** When \fIset\fP forwarded messages included in the main body of the
  ** message (when ``$$mime_forward'' is \fIunset\fP) will be quoted using
  ** ``$$indent_string''.
  */
  { "forw_quote",	DT_SYN,  R_NONE, UL "forward_quote", 0 },
  /*
  */
  { "from",		DT_ADDR, R_NONE, UL &From, UL 0 },
  /*
  ** .pp
  ** When set, this variable contains a default from address.  It
  ** can be overridden using my_hdr (including from send-hooks) and
  ** ``$$reverse_name''.  This variable is ignored if ``$$use_from''
  ** is unset.
  ** .pp
  ** Defaults to the contents of the environment variable EMAIL.
  */
  { "gecos_mask",	DT_RX,	 R_NONE, UL &GecosMask, UL "^[^,]*" },
  /*
  ** .pp
  ** A regular expression used by mutt to parse the GECOS field of a password
  ** entry when expanding the alias.  By default the regular expression is set
  ** to "^[^,]*" which will return the string up to the first "," encountered.
  ** If the GECOS field contains a string like "lastname, firstname" then you
  ** should set the gecos_mask=".*".
  ** .pp
  ** This can be useful if you see the following behavior: you address a e-mail
  ** to user ID stevef whose full name is Steve Franklin.  If mutt expands 
  ** stevef to "Franklin" stevef@foo.bar then you should set the gecos_mask to
  ** a regular expression that will match the whole name so mutt will expand
  ** "Franklin" to "Franklin, Steve".
  */
  { "hdr_format",	DT_SYN,  R_NONE, UL "index_format", 0 },
  /*
  */
  { "hdrs",		DT_BOOL, R_NONE, OPTHDRS, 1 },
  /*
  ** .pp
  ** When unset, the header fields normally added by the ``$my_hdr''
  ** command are not created.  This variable \fImust\fP be unset before
  ** composing a new message or replying in order to take effect.  If set,
  ** the user defined header fields are added to every new message.
  */
  { "header",		DT_BOOL, R_NONE, OPTHEADER, 0 },
  /*
  ** .pp
  ** When set, this variable causes Mutt to include the header
  ** of the message you are replying to into the edit buffer.
  ** The ``$$weed'' setting applies.
  */  
  { "help",		DT_BOOL, R_BOTH, OPTHELP, 1 },
  /*
  ** .pp
  ** When set, help lines describing the bindings for the major functions
  ** provided by each menu are displayed on the first line of the screen.
  ** .pp
  ** \fBNote:\fP The binding will not be displayed correctly if the
  ** function is bound to a sequence rather than a single keystroke.  Also,
  ** the help line may not be updated if a binding is changed while Mutt is
  ** running.  Since this variable is primarily aimed at new users, neither
  ** of these should present a major problem.
  */
  { "hidden_host",	DT_BOOL, R_NONE, OPTHIDDENHOST, 0 },
  /*
  ** .pp
  ** When set, mutt will skip the host name part of ``$$hostname'' variable
  ** when adding the domain part to addresses.  This variable does not
  ** affect the generation of Message-IDs, and it will not lead to the 
  ** cut-off of first-level domains.
  */
  { "hide_limited",	DT_BOOL, R_TREE|R_INDEX, OPTHIDELIMITED, 0 },
  /*
  ** .pp
  ** When set, mutt will not show the presence of messages that are hidden
  ** by limiting, in the thread tree.
  */
  { "hide_missing",	DT_BOOL, R_TREE|R_INDEX, OPTHIDEMISSING, 1 },
  /*
  ** .pp
  ** When set, mutt will not show the presence of missing messages in the
  ** thread tree.
  */
  { "hide_thread_subject", DT_BOOL, R_TREE|R_INDEX, OPTHIDETHREADSUBJECT, 1 },
  /*
  ** .pp
  ** When set, mutt will not show the subject of messages in the thread
  ** tree that have the same subject as their parent or closest previously
  ** displayed sibling.
  */
  { "hide_top_limited",	DT_BOOL, R_TREE|R_INDEX, OPTHIDETOPLIMITED, 0 },
  /*
  ** .pp
  ** When set, mutt will not show the presence of messages that are hidden
  ** by limiting, at the top of threads in the thread tree.  Note that when
  ** $$hide_missing is set, this option will have no effect.
  */
  { "hide_top_missing",	DT_BOOL, R_TREE|R_INDEX, OPTHIDETOPMISSING, 1 },
  /*
  ** .pp
  ** When set, mutt will not show the presence of missing messages at the
  ** top of threads in the thread tree.  Note that when $$hide_limited is
  ** set, this option will have no effect.
  */
  { "history",		DT_NUM,	 R_NONE, UL &HistSize, 10 },
  /*
  ** .pp
  ** This variable controls the size (in number of strings remembered) of
  ** the string history buffer. The buffer is cleared each time the
  ** variable is set.
  */
  { "honor_followup_to", DT_QUAD, R_NONE, OPT_MFUPTO, M_YES },
  /*
  ** .pp
  ** This variable controls whether or not a Mail-Followup-To header is
  ** honored when group-replying to a message.
  */
  { "hostname",		DT_STR,	 R_NONE, UL &Fqdn, 0 },
  /*
  ** .pp
  ** Specifies the hostname to use after the ``@'' in local e-mail
  ** addresses.  This overrides the compile time definition obtained from
  ** /etc/resolv.conf.
  */
  { "ignore_list_reply_to", DT_BOOL, R_NONE, OPTIGNORELISTREPLYTO, 0 },
  /*
  ** .pp
  ** Affects the behaviour of the \fIreply\fP function when replying to
  ** messages from mailing lists.  When set, if the ``Reply-To:'' field is
  ** set to the same value as the ``To:'' field, Mutt assumes that the
  ** ``Reply-To:'' field was set by the mailing list to automate responses
  ** to the list, and will ignore this field.  To direct a response to the
  ** mailing list when this option is set, use the \fIlist-reply\fP
  ** function; \fIgroup-reply\fP will reply to both the sender and the
  ** list.
  */
#ifdef USE_IMAP
  { "imap_authenticators", DT_STR, R_NONE, UL &ImapAuthenticators, UL 0 },
  /*
  ** .pp
  ** This is a colon-delimited list of authentication methods mutt may
  ** attempt to use to log in to an IMAP server, in the order mutt should
  ** try them.  Authentication methods are either 'login' or the right
  ** side of an IMAP 'AUTH=xxx' capability string, eg 'digest-md5', 'gssapi'
  ** or 'cram-md5'. This parameter is case-insensitive. If this
  ** parameter is unset (the default) mutt will try all available methods,
  ** in order from most-secure to least-secure.
  ** .pp
  ** Example: set imap_authenticators="gssapi:cram-md5:login"
  ** .pp
  ** \fBNote:\fP Mutt will only fall back to other authentication methods if
  ** the previous methods are unavailable. If a method is available but
  ** authentication fails, mutt will not connect to the IMAP server.
  */
  { "imap_delim_chars",		DT_STR, R_NONE, UL &ImapDelimChars, UL "/." },
  /*
  ** .pp
  ** This contains the list of characters which you would like to treat
  ** as folder separators for displaying IMAP paths. In particular it
  ** helps in using the '=' shortcut for your \fIfolder\fP variable.
  */
# if defined(USE_SSL) || defined(USE_GNUTLS)
  { "imap_force_ssl",		DT_BOOL, R_NONE, OPTIMAPFORCESSL, 0 },
  /*
  ** .pp
  ** If this variable is set, Mutt will always use SSL when
  ** connecting to IMAP servers.
  */
# endif
  { "imap_headers",	DT_STR, R_INDEX, UL &ImapHeaders, UL 0},
  /*
  ** .pp
  ** Mutt requests these header fields in addition to the default headers
  ** ("DATE FROM SUBJECT TO CC MESSAGE-ID REFERENCES CONTENT-TYPE
  ** CONTENT-DESCRIPTION IN-REPLY-TO REPLY-TO LINES X-LABEL") from IMAP
  ** servers before displaying the index menu. You may want to add more
  ** headers for spam detection. \fBNote:\fP This is a space separated list.
  */
  { "imap_home_namespace",	DT_STR, R_NONE, UL &ImapHomeNamespace, UL 0},
  /*
  ** .pp
  ** You normally want to see your personal folders alongside
  ** your INBOX in the IMAP browser. If you see something else, you may set
  ** this variable to the IMAP path to your folders.
  */
  { "imap_keepalive",           DT_NUM,  R_NONE, UL &ImapKeepalive, 900 },
  /*
  ** .pp
  ** This variable specifies the maximum amount of time in seconds that mutt
  ** will wait before polling open IMAP connections, to prevent the server
  ** from closing them before mutt has finished with them. The default is
  ** well within the RFC-specified minimum amount of time (30 minutes) before
  ** a server is allowed to do this, but in practice the RFC does get
  ** violated every now and then. Reduce this number if you find yourself
  ** getting disconnected from your IMAP server due to inactivity.
  */
  { "imap_list_subscribed",	DT_BOOL, R_NONE, OPTIMAPLSUB, 0 },
  /*
  ** .pp
  ** This variable configures whether IMAP folder browsing will look for
  ** only subscribed folders or all folders.  This can be toggled in the
  ** IMAP browser with the \fItoggle-subscribed\fP function.
  */
  { "imap_pass", 	DT_STR,  R_NONE, UL &ImapPass, UL 0 },
  /*
  ** .pp
  ** Specifies the password for your IMAP account.  If unset, Mutt will
  ** prompt you for your password when you invoke the fetch-mail function.
  ** \fBWarning\fP: you should only use this option when you are on a
  ** fairly secure machine, because the superuser can read your muttrc even
  ** if you are the only one who can read the file.
  */
  { "imap_passive",		DT_BOOL, R_NONE, OPTIMAPPASSIVE, 1 },
  /*
  ** .pp
  ** When set, mutt will not open new IMAP connections to check for new
  ** mail.  Mutt will only check for new mail over existing IMAP
  ** connections.  This is useful if you don't want to be prompted to
  ** user/password pairs on mutt invocation, or if opening the connection
  ** is slow.
  */
  { "imap_peek", DT_BOOL, R_NONE, OPTIMAPPEEK, 1 },
  /*
  ** .pp
  ** If set, mutt will avoid implicitly marking your mail as read whenever
  ** you fetch a message from the server. This is generally a good thing,
  ** but can make closing an IMAP folder somewhat slower. This option
  ** exists to appease speed freaks.
  */
  { "imap_servernoise",		DT_BOOL, R_NONE, OPTIMAPSERVERNOISE, 1 },
  /*
  ** .pp
  ** When set, mutt will display warning messages from the IMAP
  ** server as error messages. Since these messages are often
  ** harmless, or generated due to configuration problems on the
  ** server which are out of the users' hands, you may wish to suppress
  ** them at some point.
  */
  { "imap_user",	DT_STR,  R_NONE, UL &ImapUser, UL 0 },
  /*
  ** .pp
  ** Your login name on the IMAP server.
  ** .pp
  ** This variable defaults to your user name on the local machine.
  */
#endif
  { "implicit_autoview", DT_BOOL,R_NONE, OPTIMPLICITAUTOVIEW, 0},
  /*
  ** .pp
  ** If set to ``yes'', mutt will look for a mailcap entry with the
  ** copiousoutput flag set for \fIevery\fP MIME attachment it doesn't have
  ** an internal viewer defined for.  If such an entry is found, mutt will
  ** use the viewer defined in that entry to convert the body part to text
  ** form.
  */
  { "include",		DT_QUAD, R_NONE, OPT_INCLUDE, M_ASKYES },
  /*
  ** .pp
  ** Controls whether or not a copy of the message(s) you are replying to
  ** is included in your reply.
  */
  { "include_onlyfirst",	DT_BOOL, R_NONE, OPTINCLUDEONLYFIRST, 0},
  /*
  ** .pp
  ** Controls whether or not Mutt includes only the first attachment
  ** of the message you are replying.
  */
  { "indent_string",	DT_STR,	 R_NONE, UL &Prefix, UL "> " },
  /*
  ** .pp
  ** Specifies the string to prepend to each line of text quoted in a
  ** message to which you are replying.  You are strongly encouraged not to
  ** change this value, as it tends to agitate the more fanatical netizens.
  */
  { "indent_str",	DT_SYN,  R_NONE, UL "indent_string", 0 },
  /*
  */
  { "index_format",	DT_STR,	 R_BOTH, UL &HdrFmt, UL "%4C %Z %{%b %d} %-15.15L (%?l?%4l&%4c?) %s" },
  /*
  ** .pp
  ** This variable allows you to customize the message index display to
  ** your personal taste.
  ** .pp
  ** ``Format strings'' are similar to the strings used in the ``C''
  ** function printf to format output (see the man page for more detail).
  ** The following sequences are defined in Mutt:
  ** .pp
  ** .dl
  ** .dt %a .dd address of the author
  ** .dt %A .dd reply-to address (if present; otherwise: address of author)
  ** .dt %b .dd filename of the original message folder (think mailBox)
  ** .dt %B .dd the list to which the letter was sent, or else the folder name (%b).
  ** .dt %c .dd number of characters (bytes) in the message
  ** .dt %C .dd current message number
  ** .dt %d .dd date and time of the message in the format specified by
  **            ``date_format'' converted to sender's time zone
  ** .dt %D .dd date and time of the message in the format specified by
  **            ``date_format'' converted to the local time zone
  ** .dt %e .dd current message number in thread
  ** .dt %E .dd number of messages in current thread
  ** .dt %f .dd entire From: line (address + real name)
  ** .dt %F .dd author name, or recipient name if the message is from you
  ** .dt %H .dd spam attribute(s) of this message
  ** .dt %i .dd message-id of the current message
  ** .dt %l .dd number of lines in the message (does not work with maildir,
  **            mh, and possibly IMAP folders)
  ** .dt %L .dd If an address in the To or CC header field matches an address
  **            defined by the users ``subscribe'' command, this displays
  **            "To <list-name>", otherwise the same as %F.
  ** .dt %m .dd total number of message in the mailbox
  ** .dt %M .dd number of hidden messages if the thread is collapsed.
  ** .dt %N .dd message score
  ** .dt %n .dd author's real name (or address if missing)
  ** .dt %O .dd (_O_riginal save folder)  Where mutt would formerly have
  **            stashed the message: list name or recipient name if no list
  ** .dt %s .dd subject of the message
  ** .dt %S .dd status of the message (N/D/d/!/r/\(as)
  ** .dt %t .dd `to:' field (recipients)
  ** .dt %T .dd the appropriate character from the $$to_chars string
  ** .dt %u .dd user (login) name of the author
  ** .dt %v .dd first name of the author, or the recipient if the message is from you
  ** .dt %y .dd `x-label:' field, if present
  ** .dt %Y .dd `x-label' field, if present, and (1) not at part of a thread tree,
  **            (2) at the top of a thread, or (3) `x-label' is different from
  **            preceding message's `x-label'.
  ** .dt %Z .dd message status flags
  ** .dt %{fmt} .dd the date and time of the message is converted to sender's
  **                time zone, and ``fmt'' is expanded by the library function
  **                ``strftime''; a leading bang disables locales
  ** .dt %[fmt] .dd the date and time of the message is converted to the local
  **                time zone, and ``fmt'' is expanded by the library function
  **                ``strftime''; a leading bang disables locales
  ** .dt %(fmt) .dd the local date and time when the message was received.
  **                ``fmt'' is expanded by the library function ``strftime'';
  **                a leading bang disables locales
  ** .dt %<fmt> .dd the current local time. ``fmt'' is expanded by the library
  **                function ``strftime''; a leading bang disables locales.
  ** .dt %>X    .dd right justify the rest of the string and pad with character "X"
  ** .dt %|X    .dd pad to the end of the line with character "X"
  ** .de
  ** .pp
  ** See also: ``$$to_chars''.
  */
  { "ispell",		DT_PATH, R_NONE, UL &Ispell, UL ISPELL },
  /*
  ** .pp
  ** How to invoke ispell (GNU's spell-checking software).
  */
  { "keep_flagged", DT_BOOL, R_NONE, OPTKEEPFLAGGED, 0 },
  /*
  ** .pp
  ** If set, read messages marked as flagged will not be moved
  ** from your spool mailbox to your ``$$mbox'' mailbox, or as a result of
  ** a ``$mbox-hook'' command.
  */
  { "locale",		DT_STR,  R_BOTH, UL &Locale, UL "C" },
  /*
  ** .pp
  ** The locale used by \fIstrftime(3)\fP to format dates. Legal values are
  ** the strings your system accepts for the locale variable \fILC_TIME\fP.
  */
  { "mail_check",	DT_NUM,  R_NONE, UL &BuffyTimeout, 5 },
  /*
  ** .pp
  ** This variable configures how often (in seconds) mutt should look for
  ** new mail.
  */
  { "mailcap_path",	DT_STR,	 R_NONE, UL &MailcapPath, 0 },
  /*
  ** .pp
  ** This variable specifies which files to consult when attempting to
  ** display MIME bodies not directly supported by Mutt.
  */
  { "mailcap_sanitize",	DT_BOOL, R_NONE, OPTMAILCAPSANITIZE, 1 },
  /*
  ** .pp
  ** If set, mutt will restrict possible characters in mailcap % expandos
  ** to a well-defined set of safe characters.  This is the safe setting,
  ** but we are not sure it doesn't break some more advanced MIME stuff.
  ** .pp
  ** \fBDON'T CHANGE THIS SETTING UNLESS YOU ARE REALLY SURE WHAT YOU ARE
  ** DOING!\fP
  */
#if USE_HCACHE
  { "header_cache", DT_PATH, R_NONE, UL &HeaderCache, 0 },
  /*
  ** .pp
  ** The header_cache variable points to the header cache database. If
  ** header_cache points to a directory there will be created one header cache
  ** database per folder within this directory. If it doesn't point to a directory a
  ** global header cache for all folders is used. Per default it is unset and so
  ** no header caching will be used.
  */
  { "header_cache_pagesize", DT_STR, R_NONE, UL &HeaderCachePageSize, UL "16384" },
  /*
  ** .pp
  ** Change the header cache database page size.  Too large or to small values
  ** can waste space, memory, or CPU time. The default should be more or
  ** less the best you can get. For details google after mutt header
  ** cache (first hit).
  */
#endif /* USE_HCACHE */
  { "maildir_trash", DT_BOOL, R_NONE, OPTMAILDIRTRASH, 0 },
  /*
  ** .pp
  ** If set, messages marked as deleted will be saved with the maildir
  ** (T)rashed flag instead of unlinked.  \fBNOTE:\fP this only applies
  ** to maildir-style mailboxes.  Setting it will have no effect on other
  ** mailbox types.
  */
  { "mark_old",		DT_BOOL, R_BOTH, OPTMARKOLD, 1 },
  /*
  ** .pp
  ** Controls whether or not mutt marks \fInew\fP \fBunread\fP
  ** messages as \fIold\fP if you exit a mailbox without reading them.
  ** With this option set, the next time you start mutt, the messages
  ** will show up with an "O" next to them in the index menu,
  ** indicating that they are old.
  */
  { "markers",		DT_BOOL, R_PAGER, OPTMARKERS, 1 },
  /*
  ** .pp
  ** Controls the display of wrapped lines in the internal pager. If set, a
  ** ``+'' marker is displayed at the beginning of wrapped lines. Also see
  ** the ``$$smart_wrap'' variable.
  */
  { "mask",		DT_RX,	 R_NONE, UL &Mask, UL "!^\\.[^.]" },
  /*
  ** .pp
  ** A regular expression used in the file browser, optionally preceded by
  ** the \fInot\fP operator ``!''.  Only files whose names match this mask
  ** will be shown. The match is always case-sensitive.
  */
  { "mbox",		DT_PATH, R_BOTH, UL &Inbox, UL "~/mbox" },
  /*
  ** .pp
  ** This specifies the folder into which read mail in your ``$$spoolfile''
  ** folder will be appended.
  */
  { "mbox_type",	DT_MAGIC,R_NONE, UL &DefaultMagic, M_MBOX },
  /*
  ** .pp
  ** The default mailbox type used when creating new folders. May be any of
  ** mbox, MMDF, MH and Maildir.
  */
  { "metoo",		DT_BOOL, R_NONE, OPTMETOO, 0 },
  /*
  ** .pp
  ** If unset, Mutt will remove your address (see the ``alternates''
  ** command) from the list of recipients when replying to a message.
  */
  { "menu_scroll",	DT_BOOL, R_NONE, OPTMENUSCROLL, 0 },
  /*
  ** .pp
  ** When \fIset\fP, menus will be scrolled up or down one line when you
  ** attempt to move across a screen boundary.  If \fIunset\fP, the screen
  ** is cleared and the next or previous page of the menu is displayed
  ** (useful for slow links to avoid many redraws).
  */
  { "meta_key",		DT_BOOL, R_NONE, OPTMETAKEY, 0 },
  /*
  ** .pp
  ** If set, forces Mutt to interpret keystrokes with the high bit (bit 8)
  ** set as if the user had pressed the ESC key and whatever key remains
  ** after having the high bit removed.  For example, if the key pressed
  ** has an ASCII value of 0xf4, then this is treated as if the user had
  ** pressed ESC then ``x''.  This is because the result of removing the
  ** high bit from ``0xf4'' is ``0x74'', which is the ASCII character
  ** ``x''.
  */
  { "mh_purge",		DT_BOOL, R_NONE, OPTMHPURGE, 0 },
  /*
  ** .pp
  ** When unset, mutt will mimic mh's behaviour and rename deleted messages
  ** to \fI,<old file name>\fP in mh folders instead of really deleting
  ** them.  If the variable is set, the message files will simply be
  ** deleted.
  */
  { "mh_seq_flagged",	DT_STR, R_NONE, UL &MhFlagged, UL "flagged" },
  /*
  ** .pp
  ** The name of the MH sequence used for flagged messages.
  */
  { "mh_seq_replied",	DT_STR, R_NONE, UL &MhReplied, UL "replied" },
  /*
  ** .pp
  ** The name of the MH sequence used to tag replied messages.
  */
  { "mh_seq_unseen",	DT_STR, R_NONE, UL &MhUnseen, UL "unseen" },
  /*
  ** .pp
  ** The name of the MH sequence used for unseen messages.
  */
  { "mime_forward",	DT_QUAD, R_NONE, OPT_MIMEFWD, M_NO },
  /*
  ** .pp
  ** When set, the message you are forwarding will be attached as a
  ** separate MIME part instead of included in the main body of the
  ** message.  This is useful for forwarding MIME messages so the receiver
  ** can properly view the message as it was delivered to you. If you like
  ** to switch between MIME and not MIME from mail to mail, set this
  ** variable to ask-no or ask-yes.
  ** .pp
  ** Also see ``$$forward_decode'' and ``$$mime_forward_decode''.
  */
  { "mime_forward_decode", DT_BOOL, R_NONE, OPTMIMEFORWDECODE, 0 },
  /*
  ** .pp
  ** Controls the decoding of complex MIME messages into text/plain when
  ** forwarding a message while ``$$mime_forward'' is \fIset\fP. Otherwise
  ** ``$$forward_decode'' is used instead.
  */
  { "mime_fwd",		DT_SYN,  R_NONE, UL "mime_forward", 0 },
  /*
  */

  { "mime_forward_rest", DT_QUAD, R_NONE, OPT_MIMEFWDREST, M_YES },
  /*
  ** .pp
  ** When forwarding multiple attachments of a MIME message from the recvattach
  ** menu, attachments which cannot be decoded in a reasonable manner will
  ** be attached to the newly composed message if this option is set.
  */

#ifdef MIXMASTER
  { "mix_entry_format", DT_STR,  R_NONE, UL &MixEntryFormat, UL "%4n %c %-16s %a" },
  /*
  ** .pp
  ** This variable describes the format of a remailer line on the mixmaster
  ** chain selection screen.  The following printf-like sequences are 
  ** supported:
  ** .pp
  ** .dl
  ** .dt %n .dd The running number on the menu.
  ** .dt %c .dd Remailer capabilities.
  ** .dt %s .dd The remailer's short name.
  ** .dt %a .dd The remailer's e-mail address.
  ** .de
  */
  { "mixmaster",	DT_PATH, R_NONE, UL &Mixmaster, UL MIXMASTER },
  /*
  ** .pp
  ** This variable contains the path to the Mixmaster binary on your
  ** system.  It is used with various sets of parameters to gather the
  ** list of known remailers, and to finally send a message through the
  ** mixmaster chain.
  */
#endif


  { "move",		DT_QUAD, R_NONE, OPT_MOVE, M_ASKNO },
  /*
  ** .pp
  ** Controls whether you will be asked to confirm moving read messages
  ** from your spool mailbox to your ``$$mbox'' mailbox, or as a result of
  ** a ``$mbox-hook'' command.
  */
  { "message_format",	DT_STR,	 R_NONE, UL &MsgFmt, UL "%s" },
  /*
  ** .pp
  ** This is the string displayed in the ``attachment'' menu for
  ** attachments of type message/rfc822.  For a full listing of defined
  ** printf()-like sequences see the section on ``$$index_format''.
  */
  { "msg_format",	DT_SYN,  R_NONE, UL "message_format", 0 },
  /*
  */
  { "narrow_tree",	DT_BOOL, R_TREE|R_INDEX, OPTNARROWTREE, 0 },
  /*
  ** .pp
  ** This variable, when set, makes the thread tree narrower, allowing
  ** deeper threads to fit on the screen.
  */
  { "pager",		DT_PATH, R_NONE, UL &Pager, UL "builtin" },
  /*
  ** .pp
  ** This variable specifies which pager you would like to use to view
  ** messages.  builtin means to use the builtin pager, otherwise this
  ** variable should specify the pathname of the external pager you would
  ** like to use.
  ** .pp
  ** Using an external pager may have some disadvantages: Additional
  ** keystrokes are necessary because you can't call mutt functions
  ** directly from the pager, and screen resizes cause lines longer than
  ** the screen width to be badly formatted in the help menu.
  */
  { "pager_context",	DT_NUM,	 R_NONE, UL &PagerContext, 0 },
  /*
  ** .pp
  ** This variable controls the number of lines of context that are given
  ** when displaying the next or previous page in the internal pager.  By
  ** default, Mutt will display the line after the last one on the screen
  ** at the top of the next page (0 lines of context).
  */
  { "pager_format",	DT_STR,	 R_PAGER, UL &PagerFmt, UL "-%Z- %C/%m: %-20.20n   %s" },
  /*
  ** .pp
  ** This variable controls the format of the one-line message ``status''
  ** displayed before each message in either the internal or an external
  ** pager.  The valid sequences are listed in the ``$$index_format''
  ** section.
  */
  { "pager_index_lines",DT_NUM,	 R_PAGER, UL &PagerIndexLines, 0 },
  /*
  ** .pp
  ** Determines the number of lines of a mini-index which is shown when in
  ** the pager.  The current message, unless near the top or bottom of the
  ** folder, will be roughly one third of the way down this mini-index,
  ** giving the reader the context of a few messages before and after the
  ** message.  This is useful, for example, to determine how many messages
  ** remain to be read in the current thread.  One of the lines is reserved
  ** for the status bar from the index, so a \fIpager_index_lines\fP of 6
  ** will only show 5 lines of the actual index.  A value of 0 results in
  ** no index being shown.  If the number of messages in the current folder
  ** is less than \fIpager_index_lines\fP, then the index will only use as
  ** many lines as it needs.
  */
  { "pager_stop",	DT_BOOL, R_NONE, OPTPAGERSTOP, 0 },
  /*
  ** .pp
  ** When set, the internal-pager will \fBnot\fP move to the next message
  ** when you are at the end of a message and invoke the \fInext-page\fP
  ** function.
  */
  { "pgp_autosign", 	DT_SYN,  R_NONE, UL "crypt_autosign", 0 },
  { "crypt_autosign",	DT_BOOL, R_NONE, OPTCRYPTAUTOSIGN, 0 },
  /*
  ** .pp
  ** Setting this variable will cause Mutt to always attempt to
  ** cryptographically sign outgoing messages.  This can be overridden
  ** by use of the \fIpgp-menu\fP, when signing is not required or
  ** encryption is requested as well. If ``$$smime_is_default'' is set,
  ** then OpenSSL is used instead to create S/MIME messages and settings can
  ** be overridden by use of the \fIsmime-menu\fP.
  ** (Crypto only)
  */
  { "pgp_autoencrypt",		DT_SYN,  R_NONE, UL "crypt_autoencrypt", 0 },
  { "crypt_autoencrypt",	DT_BOOL, R_NONE, OPTCRYPTAUTOENCRYPT, 0 },
  /*
  ** .pp
  ** Setting this variable will cause Mutt to always attempt to PGP
  ** encrypt outgoing messages.  This is probably only useful in
  ** connection to the \fIsend-hook\fP command.  It can be overridden
  ** by use of the \fIpgp-menu\fP, when encryption is not required or
  ** signing is requested as well.  IF ``$$smime_is_default'' is set,
  ** then OpenSSL is used instead to create S/MIME messages and
  ** settings can be overridden by use of the \fIsmime-menu\fP.
  ** (Crypto only)
  */
  { "pgp_ignore_subkeys", DT_BOOL, R_NONE, OPTPGPIGNORESUB, 1},
  /*
  ** .pp
  ** Setting this variable will cause Mutt to ignore OpenPGP subkeys. Instead,
  ** the principal key will inherit the subkeys' capabilities.  Unset this
  ** if you want to play interesting key selection games.
  ** (PGP only)
  */
  { "pgp_replyencrypt",		DT_SYN,  R_NONE, UL "crypt_replyencrypt", 1  },
  { "crypt_replyencrypt",	DT_BOOL, R_NONE, OPTCRYPTREPLYENCRYPT, 1 },
  /*
  ** .pp
  ** If set, automatically PGP or OpenSSL encrypt replies to messages which are
  ** encrypted.
  ** (Crypto only)
  */
  { "pgp_replysign",	DT_SYN, R_NONE, UL "crypt_replysign", 0 },
  { "crypt_replysign",	DT_BOOL, R_NONE, OPTCRYPTREPLYSIGN, 0 },
  /*
  ** .pp
  ** If set, automatically PGP or OpenSSL sign replies to messages which are
  ** signed.
  ** .pp
  ** \fBNote:\fP this does not work on messages that are encrypted
  ** \fBand\fP signed!
  ** (Crypto only)
  */
  { "pgp_replysignencrypted",   DT_SYN,  R_NONE, UL "crypt_replysignencrypted", 0},
  { "crypt_replysignencrypted", DT_BOOL, R_NONE, OPTCRYPTREPLYSIGNENCRYPTED, 0 },
  /*
  ** .pp
  ** If set, automatically PGP or OpenSSL sign replies to messages
  ** which are encrypted. This makes sense in combination with
  ** ``$$crypt_replyencrypt'', because it allows you to sign all
  ** messages which are automatically encrypted.  This works around
  ** the problem noted in ``$$crypt_replysign'', that mutt is not able
  ** to find out whether an encrypted message is also signed.
  ** (Crypto only)
  */
  { "crypt_timestamp", DT_BOOL, R_NONE, OPTCRYPTTIMESTAMP, 1 },
  /*
  ** .pp
  ** If set, mutt will include a time stamp in the lines surrounding
  ** PGP or S/MIME output, so spoofing such lines is more difficult.
  ** If you are using colors to mark these lines, and rely on these,
  ** you may unset this setting.
  ** (Crypto only)
  */
  { "pgp_use_gpg_agent", DT_BOOL, R_NONE, OPTUSEGPGAGENT, 0},
  /*
  ** .pp
  ** If set, mutt will use a possibly-running gpg-agent process.
  ** (PGP only)
  */
  { "pgp_verify_sig",   DT_SYN,  R_NONE, UL "crypt_verify_sig", 0},
  { "crypt_verify_sig",	DT_QUAD, R_NONE, OPT_VERIFYSIG, M_YES },
  /*
  ** .pp
  ** If ``yes'', always attempt to verify PGP or S/MIME signatures.
  ** If ``ask'', ask whether or not to verify the signature. 
  ** If ``no'', never attempt to verify cryptographic signatures.
  ** (Crypto only)
  */
  { "smime_is_default", DT_BOOL,  R_NONE, OPTSMIMEISDEFAULT, 0},
  /*
  ** .pp
  ** The default behaviour of mutt is to use PGP on all auto-sign/encryption
  ** operations. To override and to use OpenSSL instead this must be set.
  ** However, this has no effect while replying, since mutt will automatically 
  ** select the same application that was used to sign/encrypt the original
  ** message.  (Note that this variable can be overridden by unsetting $$crypt_autosmime.)
  ** (S/MIME only)
  */
  { "smime_ask_cert_label",	DT_BOOL, R_NONE, OPTASKCERTLABEL, 1 },
  /*
  ** .pp
  ** This flag controls whether you want to be asked to enter a label
  ** for a certificate about to be added to the database or not. It is
  ** set by default.
  ** (S/MIME only)
  */
  { "smime_decrypt_use_default_key",	DT_BOOL, R_NONE, OPTSDEFAULTDECRYPTKEY, 1 },
  /*
  ** .pp
  ** If set (default) this tells mutt to use the default key for decryption. Otherwise,
  ** if manage multiple certificate-key-pairs, mutt will try to use the mailbox-address
  ** to determine the key to use. It will ask you to supply a key, if it can't find one.
  ** (S/MIME only)
  */
  { "pgp_entry_format", DT_STR,  R_NONE, UL &PgpEntryFormat, UL "%4n %t%f %4l/0x%k %-4a %2c %u" },
  /*
  ** .pp
  ** This variable allows you to customize the PGP key selection menu to
  ** your personal taste. This string is similar to ``$$index_format'', but
  ** has its own set of printf()-like sequences:
  ** .pp
  ** .dl
  ** .dt %n     .dd number
  ** .dt %k     .dd key id
  ** .dt %u     .dd user id
  ** .dt %a     .dd algorithm
  ** .dt %l     .dd key length
  ** .dt %f     .dd flags
  ** .dt %c     .dd capabilities
  ** .dt %t     .dd trust/validity of the key-uid association
  ** .dt %[<s>] .dd date of the key where <s> is an strftime(3) expression
  ** .de
  ** .pp
  ** (PGP only)
  */
  { "pgp_good_sign",	DT_RX,  R_NONE, UL &PgpGoodSign, 0 },
  /*
  ** .pp
  ** If you assign a text to this variable, then a PGP signature is only
  ** considered verified if the output from $$pgp_verify_command contains
  ** the text. Use this variable if the exit code from the command is 0
  ** even for bad signatures.
  ** (PGP only)
  */ 
  { "pgp_check_exit",	DT_BOOL, R_NONE, OPTPGPCHECKEXIT, 1 },
  /*
  ** .pp
  ** If set, mutt will check the exit code of the PGP subprocess when
  ** signing or encrypting.  A non-zero exit code means that the
  ** subprocess failed.
  ** (PGP only)
  */
  { "pgp_long_ids",	DT_BOOL, R_NONE, OPTPGPLONGIDS, 0 },
  /*
  ** .pp
  ** If set, use 64 bit PGP key IDs. Unset uses the normal 32 bit Key IDs.
  ** (PGP only)
  */
  { "pgp_retainable_sigs", DT_BOOL, R_NONE, OPTPGPRETAINABLESIG, 0 },
  /*
  ** .pp
  ** If set, signed and encrypted messages will consist of nested
  ** multipart/signed and multipart/encrypted body parts.
  ** .pp
  ** This is useful for applications like encrypted and signed mailing
  ** lists, where the outer layer (multipart/encrypted) can be easily
  ** removed, while the inner multipart/signed part is retained.
  ** (PGP only)
  */
  { "pgp_create_traditional",	DT_SYN, R_NONE, UL "pgp_autoinline", 0 },
  { "pgp_autoinline",		DT_BOOL, R_NONE, OPTPGPAUTOINLINE, 0 },
  /*
  ** .pp
  ** This option controls whether Mutt generates old-style inline
  ** (traditional) PGP encrypted or signed messages under certain
  ** circumstances.  This can be overridden by use of the \fIpgp-menu\fP,
  ** when inline is not required.
  ** .pp
  ** Note that Mutt might automatically use PGP/MIME for messages
  ** which consist of more than a single MIME part.  Mutt can be
  ** configured to ask before sending PGP/MIME messages when inline
  ** (traditional) would not work.
  ** See also: ``$$pgp_mime_auto''.
  ** .pp
  ** Also note that using the old-style PGP message format is \fBstrongly\fP
  ** \fBdeprecated\fP.
  ** (PGP only)
  */
  { "pgp_auto_traditional",	DT_SYN, R_NONE, UL "pgp_replyinline", 0 },
  { "pgp_replyinline",		DT_BOOL, R_NONE, OPTPGPREPLYINLINE, 0 },
  /*
  ** .pp
  ** Setting this variable will cause Mutt to always attempt to
  ** create an inline (traditional) message when replying to a
  ** message which is PGP encrypted/signed inline.  This can be
  ** overridden by use of the \fIpgp-menu\fP, when inline is not
  ** required.  This option does not automatically detect if the
  ** (replied-to) message is inline; instead it relies on Mutt
  ** internals for previously checked/flagged messages.
  ** .pp
  ** Note that Mutt might automatically use PGP/MIME for messages
  ** which consist of more than a single MIME part.  Mutt can be
  ** configured to ask before sending PGP/MIME messages when inline
  ** (traditional) would not work.
  ** See also: ``$$pgp_mime_auto''.
  ** .pp
  ** Also note that using the old-style PGP message format is \fBstrongly\fP
  ** \fBdeprecated\fP.
  ** (PGP only)
  ** 
  */
  { "pgp_show_unusable", DT_BOOL, R_NONE, OPTPGPSHOWUNUSABLE, 1 },
  /*
  ** .pp
  ** If set, mutt will display non-usable keys on the PGP key selection
  ** menu.  This includes keys which have been revoked, have expired, or
  ** have been marked as ``disabled'' by the user.
  ** (PGP only)
  */
  { "pgp_sign_as",	DT_STR,	 R_NONE, UL &PgpSignAs, 0 },
  /*
  ** .pp
  ** If you have more than one key pair, this option allows you to specify
  ** which of your private keys to use.  It is recommended that you use the
  ** keyid form to specify your key (e.g., ``0x00112233'').
  ** (PGP only)
  */
  { "pgp_strict_enc",	DT_BOOL, R_NONE, OPTPGPSTRICTENC, 1 },
  /*
  ** .pp
  ** If set, Mutt will automatically encode PGP/MIME signed messages as
  ** \fIquoted-printable\fP.  Please note that unsetting this variable may
  ** lead to problems with non-verifyable PGP signatures, so only change
  ** this if you know what you are doing.
  ** (PGP only)
  */
  { "pgp_timeout",	DT_NUM,	 R_NONE, UL &PgpTimeout, 300 },
  /*
  ** .pp
  ** The number of seconds after which a cached passphrase will expire if
  ** not used.
  ** (PGP only)
  */
  { "pgp_sort_keys",	DT_SORT|DT_SORT_KEYS, R_NONE, UL &PgpSortKeys, SORT_ADDRESS },
  /*
  ** .pp
  ** Specifies how the entries in the `pgp keys' menu are sorted. The
  ** following are legal values:
  ** .pp
  ** .dl
  ** .dt address .dd sort alphabetically by user id
  ** .dt keyid   .dd sort alphabetically by key id
  ** .dt date    .dd sort by key creation date
  ** .dt trust   .dd sort by the trust of the key
  ** .de
  ** .pp
  ** If you prefer reverse order of the above values, prefix it with
  ** `reverse-'.
  ** (PGP only)
  */
  { "pgp_mime_auto", DT_QUAD, R_NONE, OPT_PGPMIMEAUTO, M_ASKYES },
  /*
  ** .pp
  ** This option controls whether Mutt will prompt you for
  ** automatically sending a (signed/encrypted) message using
  ** PGP/MIME when inline (traditional) fails (for any reason).
  ** .pp
  ** Also note that using the old-style PGP message format is \fBstrongly\fP
  ** \fBdeprecated\fP.
  ** (PGP only)
  */

  /* XXX Default values! */
  
  { "pgp_decode_command", 	DT_STR, R_NONE, UL &PgpDecodeCommand, 0},
  /*
  ** .pp
  ** This format strings specifies a command which is used to decode 
  ** application/pgp attachments.
  ** .pp
  ** The PGP command formats have their own set of printf-like sequences:
  ** .pp
  ** .dl
  ** .dt %p .dd Expands to PGPPASSFD=0 when a pass phrase is needed, to an empty
  **            string otherwise. Note: This may be used with a %? construct.
  ** .dt %f .dd Expands to the name of a file containing a message.
  ** .dt %s .dd Expands to the name of a file containing the signature part
  ** .          of a multipart/signed attachment when verifying it.
  ** .dt %a .dd The value of $$pgp_sign_as.
  ** .dt %r .dd One or more key IDs.
  ** .de
  ** .pp
  ** For examples on how to configure these formats for the various versions
  ** of PGP which are floating around, see the pgp*.rc and gpg.rc files in
  ** the samples/ subdirectory which has been installed on your system
  ** alongside the documentation.
  ** (PGP only)
  */
  { "pgp_getkeys_command",	DT_STR, R_NONE, UL &PgpGetkeysCommand, 0},
  /*
  ** .pp
  ** This command is invoked whenever mutt will need public key information.
  ** %r is the only printf-like sequence used with this format.
  ** (PGP only)
  */
  { "pgp_verify_command", 	DT_STR, R_NONE, UL &PgpVerifyCommand, 0},
  /*
  ** .pp
  ** This command is used to verify PGP signatures.
  ** (PGP only)
  */
  { "pgp_decrypt_command", 	DT_STR, R_NONE, UL &PgpDecryptCommand, 0},
  /*
  ** .pp
  ** This command is used to decrypt a PGP encrypted message.
  ** (PGP only)
  */  
  { "pgp_clearsign_command",	DT_STR,	R_NONE, UL &PgpClearSignCommand, 0 },
  /*
  ** .pp
  ** This format is used to create a old-style "clearsigned" PGP
  ** message.  Note that the use of this format is \fBstrongly\fP
  ** \fBdeprecated\fP.
  ** (PGP only)
  */
  { "pgp_sign_command",		DT_STR, R_NONE, UL &PgpSignCommand, 0},
  /*
  ** .pp
  ** This command is used to create the detached PGP signature for a 
  ** multipart/signed PGP/MIME body part.
  ** (PGP only)
  */  
  { "pgp_encrypt_sign_command",	DT_STR, R_NONE, UL &PgpEncryptSignCommand, 0},
  /*
  ** .pp
  ** This command is used to both sign and encrypt a body part.
  ** (PGP only)
  */  
  { "pgp_encrypt_only_command", DT_STR, R_NONE, UL &PgpEncryptOnlyCommand, 0},
  /*
  ** .pp
  ** This command is used to encrypt a body part without signing it.
  ** (PGP only)
  */  
  { "pgp_import_command",	DT_STR, R_NONE, UL &PgpImportCommand, 0},
  /*
  ** .pp
  ** This command is used to import a key from a message into 
  ** the user's public key ring.
  ** (PGP only)
  */  
  { "pgp_export_command", 	DT_STR, R_NONE, UL &PgpExportCommand, 0},
  /*
  ** .pp
  ** This command is used to export a public key from the user's
  ** key ring.
  ** (PGP only)
  */  
  { "pgp_verify_key_command",	DT_STR, R_NONE, UL &PgpVerifyKeyCommand, 0},
  /*
  ** .pp
  ** This command is used to verify key information from the key selection
  ** menu.
  ** (PGP only)
  */  
  { "pgp_list_secring_command",	DT_STR, R_NONE, UL &PgpListSecringCommand, 0},
  /*
  ** .pp
  ** This command is used to list the secret key ring's contents.  The
  ** output format must be analogous to the one used by 
  ** gpg --list-keys --with-colons.
  ** .pp
  ** This format is also generated by the pgpring utility which comes 
  ** with mutt.
  ** (PGP only)
  */  
  { "pgp_list_pubring_command", DT_STR, R_NONE, UL &PgpListPubringCommand, 0},
  /*
  ** .pp
  ** This command is used to list the public key ring's contents.  The
  ** output format must be analogous to the one used by 
  ** gpg --list-keys --with-colons.
  ** .pp
  ** This format is also generated by the pgpring utility which comes 
  ** with mutt.
  ** (PGP only)
  */  
  { "forward_decrypt",	DT_BOOL, R_NONE, OPTFORWDECRYPT, 1 },
  /*
  ** .pp
  ** Controls the handling of encrypted messages when forwarding a message.
  ** When set, the outer layer of encryption is stripped off.  This
  ** variable is only used if ``$$mime_forward'' is \fIset\fP and
  ** ``$$mime_forward_decode'' is \fIunset\fP.
  ** (PGP only)
  */
  { "forw_decrypt",	DT_SYN,  R_NONE, UL "forward_decrypt", 0 },
  /*
  */
  
  { "smime_timeout",		DT_NUM,	 R_NONE, UL &SmimeTimeout, 300 },
  /*
  ** .pp
  ** The number of seconds after which a cached passphrase will expire if
  ** not used.
  ** (S/MIME only)
  */
  { "smime_encrypt_with",	DT_STR,	 R_NONE, UL &SmimeCryptAlg, 0 },
  /*
  ** .pp
  ** This sets the algorithm that should be used for encryption.
  ** Valid choices are "des", "des3", "rc2-40", "rc2-64", "rc2-128".
  ** If unset "3des" (TripleDES) is used.
  ** (S/MIME only)
  */
  { "smime_keys",		DT_PATH, R_NONE, UL &SmimeKeys, 0 },
  /*
  ** .pp
  ** Since there is no pubring/secring as with PGP, mutt has to handle
  ** storage ad retrieval of keys/certs by itself. This is very basic right now,
  ** and stores keys and certificates in two different directories, both
  ** named as the hash-value retrieved from OpenSSL. There is an index file
  ** which contains mailbox-address keyid pair, and which can be manually
  ** edited. This one points to the location of the private keys.
  ** (S/MIME only)
  */
  { "smime_ca_location",	DT_PATH, R_NONE, UL &SmimeCALocation, 0 },
  /*
  ** .pp
  ** This variable contains the name of either a directory, or a file which
  ** contains trusted certificates for use with OpenSSL.
  ** (S/MIME only)
  */
  { "smime_certificates",	DT_PATH, R_NONE, UL &SmimeCertificates, 0 },
  /*
  ** .pp
  ** Since there is no pubring/secring as with PGP, mutt has to handle
  ** storage and retrieval of keys by itself. This is very basic right
  ** now, and keys and certificates are stored in two different
  ** directories, both named as the hash-value retrieved from
  ** OpenSSL. There is an index file which contains mailbox-address
  ** keyid pairs, and which can be manually edited. This one points to
  ** the location of the certificates.
  ** (S/MIME only)
  */
  { "smime_decrypt_command", 	DT_STR, R_NONE, UL &SmimeDecryptCommand, 0},
  /*
  ** .pp
  ** This format string specifies a command which is used to decrypt
  ** application/x-pkcs7-mime attachments.
  ** .pp
  ** The OpenSSL command formats have their own set of printf-like sequences
  ** similar to PGP's:
  ** .pp
  ** .dl
  ** .dt %f .dd Expands to the name of a file containing a message.
  ** .dt %s .dd Expands to the name of a file containing the signature part
  ** .          of a multipart/signed attachment when verifying it.
  ** .dt %k .dd The key-pair specified with $$smime_default_key
  ** .dt %c .dd One or more certificate IDs.
  ** .dt %a .dd The algorithm used for encryption.
  ** .dt %C .dd CA location:  Depending on whether $$smime_ca_location
  ** .          points to a directory or file, this expands to 
  ** .          "-CApath $$smime_ca_location" or "-CAfile $$smime_ca_location".
  ** .de
  ** .pp
  ** For examples on how to configure these formats, see the smime.rc in
  ** the samples/ subdirectory which has been installed on your system
  ** alongside the documentation.
  ** (S/MIME only)
  */
  { "smime_verify_command", 	DT_STR, R_NONE, UL &SmimeVerifyCommand, 0},
  /*
  ** .pp
  ** This command is used to verify S/MIME signatures of type multipart/signed.
  ** (S/MIME only)
  */
  { "smime_verify_opaque_command", 	DT_STR, R_NONE, UL &SmimeVerifyOpaqueCommand, 0},
  /*
  ** .pp
  ** This command is used to verify S/MIME signatures of type
  ** application/x-pkcs7-mime.
  ** (S/MIME only)
  */
  { "smime_sign_command", 	DT_STR, R_NONE, UL &SmimeSignCommand, 0},
  /*
  ** .pp
  ** This command is used to created S/MIME signatures of type
  ** multipart/signed, which can be read by all mail clients.
  ** (S/MIME only)
  */
  { "smime_sign_opaque_command", 	DT_STR, R_NONE, UL &SmimeSignOpaqueCommand, 0},
  /*
  ** .pp
  ** This command is used to created S/MIME signatures of type
  ** application/x-pkcs7-signature, which can only be handled by mail
  ** clients supporting the S/MIME extension.
  ** (S/MIME only)
  */
  { "smime_encrypt_command", 	DT_STR, R_NONE, UL &SmimeEncryptCommand, 0},
  /*
  ** .pp
  ** This command is used to create encrypted S/MIME messages.
  ** (S/MIME only)
  */
  { "smime_pk7out_command", 	DT_STR, R_NONE, UL &SmimePk7outCommand, 0},
  /*
  ** .pp
  ** This command is used to extract PKCS7 structures of S/MIME signatures,
  ** in order to extract the public X509 certificate(s).
  ** (S/MIME only)
  */
  { "smime_get_cert_command", 	DT_STR, R_NONE, UL &SmimeGetCertCommand, 0},
  /*
  ** .pp
  ** This command is used to extract X509 certificates from a PKCS7 structure.
  ** (S/MIME only)
  */
  { "smime_get_signer_cert_command", 	DT_STR, R_NONE, UL &SmimeGetSignerCertCommand, 0},
  /*
  ** .pp
  ** This command is used to extract only the signers X509 certificate from a S/MIME
  ** signature, so that the certificate's owner may get compared to the
  ** email's 'From'-field.
  ** (S/MIME only)
  */
  { "smime_import_cert_command", 	DT_STR, R_NONE, UL &SmimeImportCertCommand, 0},
  /*
  ** .pp
  ** This command is used to import a certificate via smime_keys.
  ** (S/MIME only)
  */
  { "smime_get_cert_email_command", 	DT_STR, R_NONE, UL &SmimeGetCertEmailCommand, 0},
  /*
  ** .pp
  ** This command is used to extract the mail address(es) used for storing
  ** X509 certificates, and for verification purposes (to check whether the
  ** certificate was issued for the sender's mailbox).
  ** (S/MIME only)
  */
  { "smime_sign_as",			DT_SYN,  R_NONE, UL "smime_default_key", 0 },
  { "smime_default_key",		DT_STR,	 R_NONE, UL &SmimeDefaultKey, 0 },
  /*
  ** .pp
  ** This is the default key-pair to use for signing. This must be set to the
  ** keyid (the hash-value that OpenSSL generates) to work properly
  ** (S/MIME only)
  */
  
#if defined(USE_SSL)||defined(USE_NSS)||defined(USE_GNUTLS)
# if defined(USE_SSL)||defined(USE_GNUTLS)
  { "ssl_starttls", DT_QUAD, R_NONE, OPT_SSLSTARTTLS, M_YES },
  /*
  ** .pp
  ** If set (the default), mutt will attempt to use STARTTLS on servers
  ** advertising the capability. When unset, mutt will not attempt to
  ** use STARTTLS regardless of the server's capabilities.
  */
# endif  
  { "certificate_file",	DT_PATH, R_NONE, UL &SslCertFile, UL "~/.mutt_certificates" },
  /*
  ** .pp
  ** This variable specifies the file where the certificates you trust
  ** are saved. When an unknown certificate is encountered, you are asked
  ** if you accept it or not. If you accept it, the certificate can also 
  ** be saved in this file and further connections are automatically 
  ** accepted.
  ** .pp
  ** You can also manually add CA certificates in this file. Any server
  ** certificate that is signed with one of these CA certificates are 
  ** also automatically accepted.
  ** .pp
  ** Example: set certificate_file=~/.mutt/certificates
  */
# ifndef USE_GNUTLS
  { "ssl_usesystemcerts", DT_BOOL, R_NONE, OPTSSLSYSTEMCERTS, 1 },
  /*
  ** .pp
  ** If set to \fIyes\fP, mutt will use CA certificates in the
  ** system-wide certificate store when checking if server certificate 
  ** is signed by a trusted CA.
  */
  { "entropy_file",	DT_PATH, R_NONE, UL &SslEntropyFile, 0 },
  /*
  ** .pp
  ** The file which includes random data that is used to initialize SSL
  ** library functions.
   */
  { "ssl_use_sslv2", DT_BOOL, R_NONE, OPTSSLV2, 1 },
  /*
  ** .pp
  ** This variables specifies whether to attempt to use SSLv2 in the
  ** SSL authentication process.
  */
# endif
  { "ssl_use_sslv3", DT_BOOL, R_NONE, OPTSSLV3, 1 },
  /*
  ** .pp
  ** This variables specifies whether to attempt to use SSLv3 in the
  ** SSL authentication process.
  */
  { "ssl_use_tlsv1", DT_BOOL, R_NONE, OPTTLSV1, 1 },
  /*
  ** .pp
  ** This variables specifies whether to attempt to use TLSv1 in the
  ** SSL authentication process.
  */
  { "ssl_client_cert", DT_PATH, R_NONE, UL &SslClientCert, 0 },
  /*
  ** .pp
  ** The file containing a client certificate and its associated private
  ** key.
  */
#ifdef USE_GNUTLS
  { "ssl_min_dh_prime_bits", DT_NUM, R_NONE, UL &SslDHPrimeBits, 0 },
  /*
  ** .pp
  ** This variable specifies the minimum acceptable prime size (in bits)
  ** for use in any Diffie-Hellman key exchange. A value of 0 will use
  ** the default from the GNUTLS library.
  */
  { "ssl_ca_certificates_file", DT_PATH, R_NONE, UL &SslCACertFile, 0 },
  /*
  ** .pp
  ** This variable specifies a file containing trusted CA certificates.
  ** Any server certificate that is signed with one of these CA
  ** certificates are also automatically accepted.
  ** .pp
  ** Example: set ssl_ca_certificates_file=/etc/ssl/certs/ca-certificates.crt
  */
#endif
#endif

  { "pipe_split",	DT_BOOL, R_NONE, OPTPIPESPLIT, 0 },
  /*
  ** .pp
  ** Used in connection with the \fIpipe-message\fP command and the ``tag-
  ** prefix'' operator.  If this variable is unset, when piping a list of
  ** tagged messages Mutt will concatenate the messages and will pipe them
  ** as a single folder.  When set, Mutt will pipe the messages one by one.
  ** In both cases the messages are piped in the current sorted order,
  ** and the ``$$pipe_sep'' separator is added after each message.
  */
  { "pipe_decode",	DT_BOOL, R_NONE, OPTPIPEDECODE, 0 },
  /*
  ** .pp
  ** Used in connection with the \fIpipe-message\fP command.  When unset,
  ** Mutt will pipe the messages without any preprocessing. When set, Mutt
  ** will weed headers and will attempt to PGP/MIME decode the messages
  ** first.
  */
  { "pipe_sep",		DT_STR,	 R_NONE, UL &PipeSep, UL "\n" },
  /*
  ** .pp
  ** The separator to add between messages when piping a list of tagged
  ** messages to an external Unix command.
  */
#ifdef USE_POP
  { "pop_authenticators", DT_STR, R_NONE, UL &PopAuthenticators, UL 0 },
  /*
  ** .pp
  ** This is a colon-delimited list of authentication methods mutt may
  ** attempt to use to log in to an POP server, in the order mutt should
  ** try them.  Authentication methods are either 'user', 'apop' or any
  ** SASL mechanism, eg 'digest-md5', 'gssapi' or 'cram-md5'.
  ** This parameter is case-insensitive. If this parameter is unset
  ** (the default) mutt will try all available methods, in order from
  ** most-secure to least-secure.
  ** .pp
  ** Example: set pop_authenticators="digest-md5:apop:user"
  */
  { "pop_auth_try_all",	DT_BOOL, R_NONE, OPTPOPAUTHTRYALL, 1 },
  /*
  ** .pp
  ** If set, Mutt will try all available methods. When unset, Mutt will
  ** only fall back to other authentication methods if the previous
  ** methods are unavailable. If a method is available but authentication
  ** fails, Mutt will not connect to the POP server.
  */
  { "pop_checkinterval", DT_NUM, R_NONE, UL &PopCheckTimeout, 60 },
  /*
  ** .pp
  ** This variable configures how often (in seconds) POP should look for
  ** new mail.
  */
  { "pop_delete",	DT_QUAD, R_NONE, OPT_POPDELETE, M_ASKNO },
  /*
  ** .pp
  ** If set, Mutt will delete successfully downloaded messages from the POP
  ** server when using the fetch-mail function.  When unset, Mutt will
  ** download messages but also leave them on the POP server.
  */
  { "pop_host",		DT_STR,	 R_NONE, UL &PopHost, UL "" },
  /*
  ** .pp
  ** The name of your POP server for the fetch-mail function.  You
  ** can also specify an alternative port, username and password, ie:
  ** .pp
  ** [pop[s]://][username[:password]@]popserver[:port]
  */
  { "pop_last",		DT_BOOL, R_NONE, OPTPOPLAST, 0 },
  /*
  ** .pp
  ** If this variable is set, mutt will try to use the "LAST" POP command
  ** for retrieving only unread messages from the POP server when using
  ** the fetch-mail function.
  */
  { "pop_reconnect",	DT_QUAD, R_NONE, OPT_POPRECONNECT, M_ASKYES },
  /*
  ** .pp
  ** Controls whether or not Mutt will try to reconnect to POP server when
  ** connection lost.
  */
  { "pop_user",		DT_STR,	 R_NONE, UL &PopUser, 0 },
  /*
  ** .pp
  ** Your login name on the POP server.
  ** .pp
  ** This variable defaults to your user name on the local machine.
  */
  { "pop_pass",		DT_STR,	 R_NONE, UL &PopPass, UL "" },
  /*
  ** .pp
  ** Specifies the password for your POP account.  If unset, Mutt will
  ** prompt you for your password when you open POP mailbox.
  ** \fBWarning\fP: you should only use this option when you are on a
  ** fairly secure machine, because the superuser can read your muttrc
  ** even if you are the only one who can read the file.
  */
#endif /* USE_POP */
  { "post_indent_string",DT_STR, R_NONE, UL &PostIndentString, UL "" },
  /*
  ** .pp
  ** Similar to the ``$$attribution'' variable, Mutt will append this
  ** string after the inclusion of a message which is being replied to.
  */
  { "post_indent_str",  DT_SYN,  R_NONE, UL "post_indent_string", 0 },
  /*
  */
  { "postpone",		DT_QUAD, R_NONE, OPT_POSTPONE, M_ASKYES },
  /*
  ** .pp
  ** Controls whether or not messages are saved in the ``$$postponed''
  ** mailbox when you elect not to send immediately.
  */
  { "postponed",	DT_PATH, R_NONE, UL &Postponed, UL "~/postponed" },
  /*
  ** .pp
  ** Mutt allows you to indefinitely ``$postpone sending a message'' which
  ** you are editing.  When you choose to postpone a message, Mutt saves it
  ** in the mailbox specified by this variable.  Also see the ``$$postpone''
  ** variable.
  */
#ifdef USE_SOCKET
  { "preconnect",	DT_STR, R_NONE, UL &Preconnect, UL 0},
  /*
  ** .pp
  ** If set, a shell command to be executed if mutt fails to establish
  ** a connection to the server. This is useful for setting up secure
  ** connections, e.g. with ssh(1). If the command returns a  nonzero
  ** status, mutt gives up opening the server. Example:
  ** .pp
  ** preconnect="ssh -f -q -L 1234:mailhost.net:143 mailhost.net
  **                   sleep 20 < /dev/null > /dev/null"
  ** .pp
  ** Mailbox 'foo' on mailhost.net can now be reached
  ** as '{localhost:1234}foo'.
  ** .pp
  ** NOTE: For this example to work, you must be able to log in to the
  ** remote machine without having to enter a password.
  */
#endif /* USE_SOCKET */
  { "print",		DT_QUAD, R_NONE, OPT_PRINT, M_ASKNO },
  /*
  ** .pp
  ** Controls whether or not Mutt asks for confirmation before printing.
  ** This is useful for people (like me) who accidentally hit ``p'' often.
  */
  { "print_command",	DT_PATH, R_NONE, UL &PrintCmd, UL "lpr" },
  /*
  ** .pp
  ** This specifies the command pipe that should be used to print messages.
  */
  { "print_cmd",	DT_SYN,  R_NONE, UL "print_command", 0 },
  /*
  */
  { "print_decode",	DT_BOOL, R_NONE, OPTPRINTDECODE, 1 },
  /*
  ** .pp
  ** Used in connection with the print-message command.  If this
  ** option is set, the message is decoded before it is passed to the
  ** external command specified by $$print_command.  If this option
  ** is unset, no processing will be applied to the message when
  ** printing it.  The latter setting may be useful if you are using
  ** some advanced printer filter which is able to properly format
  ** e-mail messages for printing.
  */
  { "print_split",	DT_BOOL, R_NONE, OPTPRINTSPLIT,  0 },
  /*
  ** .pp
  ** Used in connection with the print-message command.  If this option
  ** is set, the command specified by $$print_command is executed once for
  ** each message which is to be printed.  If this option is unset, 
  ** the command specified by $$print_command is executed only once, and
  ** all the messages are concatenated, with a form feed as the message
  ** separator.
  ** .pp
  ** Those who use the \fBenscript\fP(1) program's mail-printing mode will
  ** most likely want to set this option.
  */
  { "prompt_after",	DT_BOOL, R_NONE, OPTPROMPTAFTER, 1 },
  /*
  ** .pp
  ** If you use an \fIexternal\fP ``$$pager'', setting this variable will
  ** cause Mutt to prompt you for a command when the pager exits rather
  ** than returning to the index menu.  If unset, Mutt will return to the
  ** index menu when the external pager exits.
  */
  { "query_command",	DT_PATH, R_NONE, UL &QueryCmd, UL "" },
  /*
  ** .pp
  ** This specifies the command that mutt will use to make external address
  ** queries.  The string should contain a %s, which will be substituted
  ** with the query string the user types.  See ``$query'' for more
  ** information.
  */
  { "quit",		DT_QUAD, R_NONE, OPT_QUIT, M_YES },
  /*
  ** .pp
  ** This variable controls whether ``quit'' and ``exit'' actually quit
  ** from mutt.  If it set to yes, they do quit, if it is set to no, they
  ** have no effect, and if it is set to ask-yes or ask-no, you are
  ** prompted for confirmation when you try to quit.
  */
  { "quote_regexp",	DT_RX,	 R_PAGER, UL &QuoteRegexp, UL "^([ \t]*[|>:}#])+" },
  /*
  ** .pp
  ** A regular expression used in the internal-pager to determine quoted
  ** sections of text in the body of a message.
  ** .pp
  ** \fBNote:\fP In order to use the \fIquoted\fP\fBx\fP patterns in the
  ** internal pager, you need to set this to a regular expression that
  ** matches \fIexactly\fP the quote characters at the beginning of quoted
  ** lines.
  */
  { "read_inc",		DT_NUM,	 R_NONE, UL &ReadInc, 10 },
  /*
  ** .pp
  ** If set to a value greater than 0, Mutt will display which message it
  ** is currently on when reading a mailbox.  The message is printed after
  ** \fIread_inc\fP messages have been read (e.g., if set to 25, Mutt will
  ** print a message when it reads message 25, and then again when it gets
  ** to message 50).  This variable is meant to indicate progress when
  ** reading large mailboxes which may take some time.
  ** When set to 0, only a single message will appear before the reading
  ** the mailbox.
  ** .pp
  ** Also see the ``$$write_inc'' variable.
  */
  { "read_only",	DT_BOOL, R_NONE, OPTREADONLY, 0 },
  /*
  ** .pp
  ** If set, all folders are opened in read-only mode.
  */
  { "realname",		DT_STR,	 R_BOTH, UL &Realname, 0 },
  /*
  ** .pp
  ** This variable specifies what "real" or "personal" name should be used
  ** when sending messages.
  ** .pp
  ** By default, this is the GECOS field from /etc/passwd.  Note that this
  ** variable will \fInot\fP be used when the user has set a real name
  ** in the $$from variable.
  */
  { "recall",		DT_QUAD, R_NONE, OPT_RECALL, M_ASKYES },
  /*
  ** .pp
  ** Controls whether or not you are prompted to recall postponed messages
  ** when composing a new message.  Also see ``$$postponed''.
  ** .pp
  ** Setting this variable to ``yes'' is not generally useful, and thus not
  ** recommended.
  */
  { "record",		DT_PATH, R_NONE, UL &Outbox, UL "" },
  /*
  ** .pp
  ** This specifies the file into which your outgoing messages should be
  ** appended.  (This is meant as the primary method for saving a copy of
  ** your messages, but another way to do this is using the ``$my_hdr''
  ** command to create a \fIBcc:\fP field with your email address in it.)
  ** .pp
  ** The value of \fI$$record\fP is overridden by the ``$$force_name'' and
  ** ``$$save_name'' variables, and the ``$fcc-hook'' command.
  */
  { "reply_regexp",	DT_RX,	 R_INDEX|R_RESORT, UL &ReplyRegexp, UL "^(re([\\[0-9\\]+])*|aw):[ \t]*" },
  /*
  ** .pp
  ** A regular expression used to recognize reply messages when threading
  ** and replying. The default value corresponds to the English "Re:" and
  ** the German "Aw:".
  */
  { "reply_self",	DT_BOOL, R_NONE, OPTREPLYSELF, 0 },
  /*
  ** .pp
  ** If unset and you are replying to a message sent by you, Mutt will
  ** assume that you want to reply to the recipients of that message rather
  ** than to yourself.
  */
  { "reply_to",		DT_QUAD, R_NONE, OPT_REPLYTO, M_ASKYES },
  /*
  ** .pp
  ** If set, Mutt will ask you if you want to use the address listed in the
  ** Reply-To: header field when replying to a message.  If you answer no,
  ** it will use the address in the From: header field instead.  This
  ** option is useful for reading a mailing list that sets the Reply-To:
  ** header field to the list address and you want to send a private
  ** message to the author of a message.
  */
  { "resolve",		DT_BOOL, R_NONE, OPTRESOLVE, 1 },
  /*
  ** .pp
  ** When set, the cursor will be automatically advanced to the next
  ** (possibly undeleted) message whenever a command that modifies the
  ** current message is executed.
  */
  { "reverse_alias",	DT_BOOL, R_BOTH, OPTREVALIAS, 0 },
  /*
  ** .pp
  ** This variable controls whether or not Mutt will display the "personal"
  ** name from your aliases in the index menu if it finds an alias that
  ** matches the message's sender.  For example, if you have the following
  ** alias:
  ** .pp
  ** .ts
  **  alias juser abd30425@somewhere.net (Joe User)
  ** .te
  ** .pp
  ** and then you receive mail which contains the following header:
  ** .pp
  ** .ts
  **  From: abd30425@somewhere.net
  ** .te
  ** .pp
  ** It would be displayed in the index menu as ``Joe User'' instead of
  ** ``abd30425@somewhere.net.''  This is useful when the person's e-mail
  ** address is not human friendly (like CompuServe addresses).
  */
  { "reverse_name",	DT_BOOL, R_BOTH, OPTREVNAME, 0 },
  /*
  ** .pp
  ** It may sometimes arrive that you receive mail to a certain machine,
  ** move the messages to another machine, and reply to some the messages
  ** from there.  If this variable is set, the default \fIFrom:\fP line of
  ** the reply messages is built using the address where you received the
  ** messages you are replying to \fBif\fP that address matches your
  ** alternates.  If the variable is unset, or the address that would be
  ** used doesn't match your alternates, the \fIFrom:\fP line will use
  ** your address on the current machine.
  */
  { "reverse_realname",	DT_BOOL, R_BOTH, OPTREVREAL, 1 },
  /*
  ** .pp
  ** This variable fine-tunes the behaviour of the $reverse_name feature.
  ** When it is set, mutt will use the address from incoming messages as-is,
  ** possibly including eventual real names.  When it is unset, mutt will
  ** override any such real names with the setting of the $realname variable.
  */
  { "rfc2047_parameters", DT_BOOL, R_NONE, OPTRFC2047PARAMS, 0 },
  /*
  ** .pp
  ** When this variable is set, Mutt will decode RFC-2047-encoded MIME 
  ** parameters. You want to set this variable when mutt suggests you
  ** to save attachments to files named like this: 
  ** =?iso-8859-1?Q?file=5F=E4=5F991116=2Ezip?=
  ** .pp
  ** When this variable is set interactively, the change doesn't have
  ** the desired effect before you have changed folders.
  ** .pp
  ** Note that this use of RFC 2047's encoding is explicitly,
  ** prohibited by the standard, but nevertheless encountered in the
  ** wild.
  ** Also note that setting this parameter will \fInot\fP have the effect 
  ** that mutt \fIgenerates\fP this kind of encoding.  Instead, mutt will
  ** unconditionally use the encoding specified in RFC 2231.
  */
  { "save_address",	DT_BOOL, R_NONE, OPTSAVEADDRESS, 0 },
  /*
  ** .pp
  ** If set, mutt will take the sender's full address when choosing a
  ** default folder for saving a mail. If ``$$save_name'' or ``$$force_name''
  ** is set too, the selection of the fcc folder will be changed as well.
  */
  { "save_empty",	DT_BOOL, R_NONE, OPTSAVEEMPTY, 1 },
  /*
  ** .pp
  ** When unset, mailboxes which contain no saved messages will be removed
  ** when closed (the exception is ``$$spoolfile'' which is never removed).
  ** If set, mailboxes are never removed.
  ** .pp
  ** \fBNote:\fP This only applies to mbox and MMDF folders, Mutt does not
  ** delete MH and Maildir directories.
  */
  { "save_name",	DT_BOOL, R_NONE, OPTSAVENAME, 0 },
  /*
  ** .pp
  ** This variable controls how copies of outgoing messages are saved.
  ** When set, a check is made to see if a mailbox specified by the
  ** recipient address exists (this is done by searching for a mailbox in
  ** the ``$$folder'' directory with the \fIusername\fP part of the
  ** recipient address).  If the mailbox exists, the outgoing message will
  ** be saved to that mailbox, otherwise the message is saved to the
  ** ``$$record'' mailbox.
  ** .pp
  ** Also see the ``$$force_name'' variable.
  */
  { "score", 		DT_BOOL, R_NONE, OPTSCORE, 1 },
  /*
  ** .pp
  ** When this variable is \fIunset\fP, scoring is turned off.  This can
  ** be useful to selectively disable scoring for certain folders when the
  ** ``$$score_threshold_delete'' variable and friends are used.
  **
  */
  { "score_threshold_delete", DT_NUM, R_NONE, UL &ScoreThresholdDelete, UL -1 },
  /*
  ** .pp
  ** Messages which have been assigned a score equal to or lower than the value
  ** of this variable are automatically marked for deletion by mutt.  Since
  ** mutt scores are always greater than or equal to zero, the default setting
  ** of this variable will never mark a message for deletion.
  */
  { "score_threshold_flag", DT_NUM, R_NONE, UL &ScoreThresholdFlag, 9999 },
  /* 
  ** .pp
  ** Messages which have been assigned a score greater than or equal to this 
  ** variable's value are automatically marked "flagged".
  */
  { "score_threshold_read", DT_NUM, R_NONE, UL &ScoreThresholdRead, UL -1 },
  /*
  ** .pp
  ** Messages which have been assigned a score equal to or lower than the value
  ** of this variable are automatically marked as read by mutt.  Since
  ** mutt scores are always greater than or equal to zero, the default setting
  ** of this variable will never mark a message read.
  */
  { "send_charset",	DT_STR,  R_NONE, UL &SendCharset, UL "us-ascii:iso-8859-1:utf-8" },
  /*
  ** .pp
  ** A list of character sets for outgoing messages. Mutt will use the
  ** first character set into which the text can be converted exactly.
  ** If your ``$$charset'' is not iso-8859-1 and recipients may not
  ** understand UTF-8, it is advisable to include in the list an
  ** appropriate widely used standard character set (such as
  ** iso-8859-2, koi8-r or iso-2022-jp) either instead of or after
  ** "iso-8859-1".
  */
  { "sendmail",		DT_PATH, R_NONE, UL &Sendmail, UL SENDMAIL " -oem -oi" },
  /*
  ** .pp
  ** Specifies the program and arguments used to deliver mail sent by Mutt.
  ** Mutt expects that the specified program interprets additional
  ** arguments as recipient addresses.
  */
  { "sendmail_wait",	DT_NUM,  R_NONE, UL &SendmailWait, 0 },
  /*
  ** .pp
  ** Specifies the number of seconds to wait for the ``$$sendmail'' process
  ** to finish before giving up and putting delivery in the background.
  ** .pp
  ** Mutt interprets the value of this variable as follows:
  ** .dl
  ** .dt >0 .dd number of seconds to wait for sendmail to finish before continuing
  ** .dt 0  .dd wait forever for sendmail to finish
  ** .dt <0 .dd always put sendmail in the background without waiting
  ** .de
  ** .pp
  ** Note that if you specify a value other than 0, the output of the child
  ** process will be put in a temporary file.  If there is some error, you
  ** will be informed as to where to find the output.
  */
  { "shell",		DT_PATH, R_NONE, UL &Shell, 0 },
  /*
  ** .pp
  ** Command to use when spawning a subshell.  By default, the user's login
  ** shell from /etc/passwd is used.
  */
  { "sig_dashes",	DT_BOOL, R_NONE, OPTSIGDASHES, 1 },
  /*
  ** .pp
  ** If set, a line containing ``-- '' will be inserted before your
  ** ``$$signature''.  It is \fBstrongly\fP recommended that you not unset
  ** this variable unless your ``signature'' contains just your name.  The
  ** reason for this is because many software packages use ``-- \n'' to
  ** detect your signature.  For example, Mutt has the ability to highlight
  ** the signature in a different color in the builtin pager.
  */
  { "sig_on_top",	DT_BOOL, R_NONE, OPTSIGONTOP, 0},
  /*
  ** .pp
  ** If set, the signature will be included before any quoted or forwarded
  ** text.  It is \fBstrongly\fP recommended that you do not set this variable
  ** unless you really know what you are doing, and are prepared to take
  ** some heat from netiquette guardians.
  */
  { "signature",	DT_PATH, R_NONE, UL &Signature, UL "~/.signature" },
  /*
  ** .pp
  ** Specifies the filename of your signature, which is appended to all
  ** outgoing messages.   If the filename ends with a pipe (``|''), it is
  ** assumed that filename is a shell command and input should be read from
  ** its stdout.
  */
  { "simple_search",	DT_STR,	 R_NONE, UL &SimpleSearch, UL "~f %s | ~s %s" },
  /*
  ** .pp
  ** Specifies how Mutt should expand a simple search into a real search
  ** pattern.  A simple search is one that does not contain any of the ~
  ** operators.  See ``$patterns'' for more information on search patterns.
  ** .pp
  ** For example, if you simply type joe at a search or limit prompt, Mutt
  ** will automatically expand it to the value specified by this variable.
  ** For the default value it would be:
  ** .pp
  ** ~f joe | ~s joe
  */
  { "smart_wrap",	DT_BOOL, R_PAGER, OPTWRAP, 1 },
  /*
  ** .pp
  ** Controls the display of lines longer than the screen width in the
  ** internal pager. If set, long lines are wrapped at a word boundary.  If
  ** unset, lines are simply wrapped at the screen edge. Also see the
  ** ``$$markers'' variable.
  */
  { "smileys",		DT_RX,	 R_PAGER, UL &Smileys, UL "(>From )|(:[-^]?[][)(><}{|/DP])" },
  /*
  ** .pp
  ** The \fIpager\fP uses this variable to catch some common false
  ** positives of ``$$quote_regexp'', most notably smileys in the beginning
  ** of a line
  */
  { "sleep_time",	DT_NUM, R_NONE, UL &SleepTime, 1 },
  /*
  ** .pp
  ** Specifies time, in seconds, to pause while displaying certain informational
  ** messages, while moving from folder to folder and after expunging
  ** messages from the current folder.  The default is to pause one second, so 
  ** a value of zero for this option suppresses the pause.
  */
  { "sort",		DT_SORT, R_INDEX|R_RESORT, UL &Sort, SORT_DATE },
  /*
  ** .pp
  ** Specifies how to sort messages in the \fIindex\fP menu.  Valid values
  ** are:
  ** .pp
  ** .ts
  ** .  date or date-sent
  ** .  date-received
  ** .  from
  ** .  mailbox-order (unsorted)
  ** .  score
  ** .  size
  ** .  spam
  ** .  subject
  ** .  threads
  ** .  to
  ** .te
  ** .pp
  ** You may optionally use the reverse- prefix to specify reverse sorting
  ** order (example: set sort=reverse-date-sent).
  */
  { "sort_alias",	DT_SORT|DT_SORT_ALIAS,	R_NONE,	UL &SortAlias, SORT_ALIAS },
  /*
  ** .pp
  ** Specifies how the entries in the `alias' menu are sorted.  The
  ** following are legal values:
  ** .pp
  ** .ts
  ** .  address (sort alphabetically by email address)
  ** .  alias (sort alphabetically by alias name)
  ** .  unsorted (leave in order specified in .muttrc)
  ** .te
  */
  { "sort_aux",		DT_SORT|DT_SORT_AUX, R_INDEX|R_RESORT_BOTH, UL &SortAux, SORT_DATE },
  /*
  ** .pp
  ** When sorting by threads, this variable controls how threads are sorted
  ** in relation to other threads, and how the branches of the thread trees
  ** are sorted.  This can be set to any value that ``$$sort'' can, except
  ** threads (in that case, mutt will just use date-sent).  You can also
  ** specify the last- prefix in addition to the reverse- prefix, but last-
  ** must come after reverse-.  The last- prefix causes messages to be
  ** sorted against its siblings by which has the last descendant, using
  ** the rest of sort_aux as an ordering.  For instance, set sort_aux=last-
  ** date-received would mean that if a new message is received in a
  ** thread, that thread becomes the last one displayed (or the first, if
  ** you have set sort=reverse-threads.) Note: For reversed ``$$sort''
  ** order $$sort_aux is reversed again (which is not the right thing to do,
  ** but kept to not break any existing configuration setting).
  */
  { "sort_browser",	DT_SORT|DT_SORT_BROWSER, R_NONE, UL &BrowserSort, SORT_ALPHA },
  /*
  ** .pp
  ** Specifies how to sort entries in the file browser.  By default, the
  ** entries are sorted alphabetically.  Valid values:
  ** .pp
  ** .ts
  ** .  alpha (alphabetically)
  ** .  date
  ** .  size
  ** .  unsorted
  ** .te
  ** .pp
  ** You may optionally use the reverse- prefix to specify reverse sorting
  ** order (example: set sort_browser=reverse-date).
  */
  { "sort_re",		DT_BOOL, R_INDEX|R_RESORT|R_RESORT_INIT, OPTSORTRE, 1 },
  /*
  ** .pp
  ** This variable is only useful when sorting by threads with
  ** ``$$strict_threads'' unset.  In that case, it changes the heuristic
  ** mutt uses to thread messages by subject.  With sort_re set, mutt will
  ** only attach a message as the child of another message by subject if
  ** the subject of the child message starts with a substring matching the
  ** setting of ``$$reply_regexp''.  With sort_re unset, mutt will attach
  ** the message whether or not this is the case, as long as the
  ** non-``$$reply_regexp'' parts of both messages are identical.
  */
  { "spam_separator",   DT_STR, R_NONE, UL &SpamSep, UL "," },
  /*
  ** .pp
  ** ``$spam_separator'' controls what happens when multiple spam headers
  ** are matched: if unset, each successive header will overwrite any
  ** previous matches value for the spam label. If set, each successive
  ** match will append to the previous, using ``$spam_separator'' as a
  ** separator.
  */
  { "spoolfile",	DT_PATH, R_NONE, UL &Spoolfile, 0 },
  /*
  ** .pp
  ** If your spool mailbox is in a non-default place where Mutt cannot find
  ** it, you can specify its location with this variable.  Mutt will
  ** automatically set this variable to the value of the environment
  ** variable $$$MAIL if it is not set.
  */
  { "status_chars",	DT_STR,	 R_BOTH, UL &StChars, UL "-*%A" },
  /*
  ** .pp
  ** Controls the characters used by the "%r" indicator in
  ** ``$$status_format''. The first character is used when the mailbox is
  ** unchanged. The second is used when the mailbox has been changed, and
  ** it needs to be resynchronized. The third is used if the mailbox is in
  ** read-only mode, or if the mailbox will not be written when exiting
  ** that mailbox (You can toggle whether to write changes to a mailbox
  ** with the toggle-write operation, bound by default to "%"). The fourth
  ** is used to indicate that the current folder has been opened in attach-
  ** message mode (Certain operations like composing a new mail, replying,
  ** forwarding, etc. are not permitted in this mode).
  */
  { "status_format",	DT_STR,	 R_BOTH, UL &Status, UL "-%r-Mutt: %f [Msgs:%?M?%M/?%m%?n? New:%n?%?o? Old:%o?%?d? Del:%d?%?F? Flag:%F?%?t? Tag:%t?%?p? Post:%p?%?b? Inc:%b?%?l? %l?]---(%s/%S)-%>-(%P)---" },
  /*
  ** .pp
  ** Controls the format of the status line displayed in the \fIindex\fP
  ** menu.  This string is similar to ``$$index_format'', but has its own
  ** set of printf()-like sequences:
  ** .pp
  ** .dl
  ** .dt %b  .dd number of mailboxes with new mail *
  ** .dt %d  .dd number of deleted messages *
  ** .dt %f  .dd the full pathname of the current mailbox
  ** .dt %F  .dd number of flagged messages *
  ** .dt %h  .dd local hostname
  ** .dt %l  .dd size (in bytes) of the current mailbox *
  ** .dt %L  .dd size (in bytes) of the messages shown 
  **             (i.e., which match the current limit) *
  ** .dt %m  .dd the number of messages in the mailbox *
  ** .dt %M  .dd the number of messages shown (i.e., which match the current limit) *
  ** .dt %n  .dd number of new messages in the mailbox *
  ** .dt %o  .dd number of old unread messages
  ** .dt %p  .dd number of postponed messages *
  ** .dt %P  .dd percentage of the way through the index
  ** .dt %r  .dd modified/read-only/won't-write/attach-message indicator,
  **             according to $$status_chars
  ** .dt %s  .dd current sorting mode ($$sort)
  ** .dt %S  .dd current aux sorting method ($$sort_aux)
  ** .dt %t  .dd number of tagged messages *
  ** .dt %u  .dd number of unread messages *
  ** .dt %v  .dd Mutt version string
  ** .dt %V  .dd currently active limit pattern, if any *
  ** .dt %>X .dd right justify the rest of the string and pad with "X"
  ** .dt %|X .dd pad to the end of the line with "X"
  ** .de
  ** .pp
  ** * = can be optionally printed if nonzero
  ** .pp
  ** Some of the above sequences can be used to optionally print a string
  ** if their value is nonzero.  For example, you may only want to see the
  ** number of flagged messages if such messages exist, since zero is not
  ** particularly meaningful.  To optionally print a string based upon one
  ** of the above sequences, the following construct is used
  ** .pp
  **  %?<sequence_char>?<optional_string>?
  ** .pp
  ** where \fIsequence_char\fP is a character from the table above, and
  ** \fIoptional_string\fP is the string you would like printed if
  ** \fIstatus_char\fP is nonzero.  \fIoptional_string\fP \fBmay\fP contain
  ** other sequence as well as normal text, but you may \fBnot\fP nest
  ** optional strings.
  ** .pp
  ** Here is an example illustrating how to optionally print the number of
  ** new messages in a mailbox:
  ** %?n?%n new messages.?
  ** .pp
  ** Additionally you can switch between two strings, the first one, if a
  ** value is zero, the second one, if the value is nonzero, by using the
  ** following construct:
  ** %?<sequence_char>?<if_string>&<else_string>?
  ** .pp
  ** You can additionally force the result of any printf-like sequence to
  ** be lowercase by prefixing the sequence character with an underscore
  ** (_) sign.  For example, if you want to display the local hostname in
  ** lowercase, you would use:
  ** %_h
  ** .pp
  ** If you prefix the sequence character with a colon (:) character, mutt
  ** will replace any dots in the expansion by underscores. This might be helpful 
  ** with IMAP folders that don't like dots in folder names.
  */
  { "status_on_top",	DT_BOOL, R_BOTH, OPTSTATUSONTOP, 0 },
  /*
  ** .pp
  ** Setting this variable causes the ``status bar'' to be displayed on
  ** the first line of the screen rather than near the bottom.
  */
  { "strict_threads",	DT_BOOL, R_RESORT|R_RESORT_INIT|R_INDEX, OPTSTRICTTHREADS, 0 },
  /*
  ** .pp
  ** If set, threading will only make use of the ``In-Reply-To'' and
  ** ``References'' fields when you ``$$sort'' by message threads.  By
  ** default, messages with the same subject are grouped together in
  ** ``pseudo threads.''  This may not always be desirable, such as in a
  ** personal mailbox where you might have several unrelated messages with
  ** the subject ``hi'' which will get grouped together.
  */
  { "suspend",		DT_BOOL, R_NONE, OPTSUSPEND, 1 },
  /*
  ** .pp
  ** When \fIunset\fP, mutt won't stop when the user presses the terminal's
  ** \fIsusp\fP key, usually ``control-Z''. This is useful if you run mutt
  ** inside an xterm using a command like xterm -e mutt.
  */
  { "text_flowed", 	DT_BOOL, R_NONE, OPTTEXTFLOWED,  0 },
  /*
  ** .pp
  ** When set, mutt will generate text/plain; format=flowed attachments.
  ** This format is easier to handle for some mailing software, and generally
  ** just looks like ordinary text.  To actually make use of this format's 
  ** features, you'll need support in your editor.
  ** .pp
  ** Note that $$indent_string is ignored when this option is set.
  */
  { "thread_received",	DT_BOOL, R_RESORT|R_RESORT_INIT|R_INDEX, OPTTHREADRECEIVED, 0 },
  /*
  ** .pp
  ** When set, mutt uses the date received rather than the date sent
  ** to thread messages by subject.
  */
  { "thorough_search",	DT_BOOL, R_NONE, OPTTHOROUGHSRC, 0 },
  /*
  ** .pp
  ** Affects the \fI~b\fP and \fI~h\fP search operations described in
  ** section ``$patterns'' above.  If set, the headers and attachments of
  ** messages to be searched are decoded before searching.  If unset,
  ** messages are searched as they appear in the folder.
  */
  { "tilde",		DT_BOOL, R_PAGER, OPTTILDE, 0 },
  /*
  ** .pp
  ** When set, the internal-pager will pad blank lines to the bottom of the
  ** screen with a tilde (~).
  */
  { "timeout",		DT_NUM,	 R_NONE, UL &Timeout, 600 },
  /*
  ** .pp
  ** This variable controls the \fInumber of seconds\fP Mutt will wait
  ** for a key to be pressed in the main menu before timing out and
  ** checking for new mail.  A value of zero or less will cause Mutt
  ** to never time out.
  */
  { "tmpdir",		DT_PATH, R_NONE, UL &Tempdir, 0 },
  /*
  ** .pp
  ** This variable allows you to specify where Mutt will place its
  ** temporary files needed for displaying and composing messages.  If
  ** this variable is not set, the environment variable TMPDIR is
  ** used.  If TMPDIR is not set then "/tmp" is used.
  */
  { "to_chars",		DT_STR,	 R_BOTH, UL &Tochars, UL " +TCFL" },
  /*
  ** .pp
  ** Controls the character used to indicate mail addressed to you.  The
  ** first character is the one used when the mail is NOT addressed to your
  ** address (default: space).  The second is used when you are the only
  ** recipient of the message (default: +).  The third is when your address
  ** appears in the TO header field, but you are not the only recipient of
  ** the message (default: T).  The fourth character is used when your
  ** address is specified in the CC header field, but you are not the only
  ** recipient.  The fifth character is used to indicate mail that was sent
  ** by \fIyou\fP.  The sixth character is used to indicate when a mail
  ** was sent to a mailing-list you subscribe to (default: L).
  */
#ifdef USE_SOCKET
  { "tunnel",            DT_STR, R_NONE, UL &Tunnel, UL 0 },
  /*
  ** .pp
  ** Setting this variable will cause mutt to open a pipe to a command
  ** instead of a raw socket. You may be able to use this to set up
  ** preauthenticated connections to your IMAP/POP3 server. Example:
  ** .pp
  ** tunnel="ssh -q mailhost.net /usr/local/libexec/imapd"
  ** .pp
  ** NOTE: For this example to work you must be able to log in to the remote
  ** machine without having to enter a password.
  */
#endif
  { "use_8bitmime",	DT_BOOL, R_NONE, OPTUSE8BITMIME, 0 },
  /*
  ** .pp
  ** \fBWarning:\fP do not set this variable unless you are using a version
  ** of sendmail which supports the -B8BITMIME flag (such as sendmail
  ** 8.8.x) or you may not be able to send mail.
  ** .pp
  ** When \fIset\fP, Mutt will invoke ``$$sendmail'' with the -B8BITMIME
  ** flag when sending 8-bit messages to enable ESMTP negotiation.
  */
  { "use_domain",	DT_BOOL, R_NONE, OPTUSEDOMAIN, 1 },
  /*
  ** .pp
  ** When set, Mutt will qualify all local addresses (ones without the
  ** @host portion) with the value of ``$$hostname''.  If \fIunset\fP, no
  ** addresses will be qualified.
  */
  { "use_from",		DT_BOOL, R_NONE, OPTUSEFROM, 1 },
  /*
  ** .pp
  ** When \fIset\fP, Mutt will generate the `From:' header field when
  ** sending messages.  If \fIunset\fP, no `From:' header field will be
  ** generated unless the user explicitly sets one using the ``$my_hdr''
  ** command.
  */
#ifdef HAVE_LIBIDN
  { "use_idn",		DT_BOOL, R_BOTH, OPTUSEIDN, 1},
  /*
  ** .pp
  ** When \fIset\fP, Mutt will show you international domain names decoded.
  ** Note: You can use IDNs for addresses even if this is \fIunset\fP.
  ** This variable only affects decoding.
  */
#endif /* HAVE_LIBIDN */
#ifdef HAVE_GETADDRINFO
  { "use_ipv6",		DT_BOOL, R_NONE, OPTUSEIPV6, 1},
  /*
  ** .pp
  ** When \fIset\fP, Mutt will look for IPv6 addresses of hosts it tries to
  ** contact.  If this option is unset, Mutt will restrict itself to IPv4 addresses.
  ** Normally, the default should work.
  */
#endif /* HAVE_GETADDRINFO */
  { "user_agent",	DT_BOOL, R_NONE, OPTXMAILER, 1},
  /*
  ** .pp
  ** When \fIset\fP, mutt will add a "User-Agent" header to outgoing
  ** messages, indicating which version of mutt was used for composing
  ** them.
  */
  { "visual",		DT_PATH, R_NONE, UL &Visual, 0 },
  /*
  ** .pp
  ** Specifies the visual editor to invoke when the \fI~v\fP command is
  ** given in the builtin editor.
  */
  { "wait_key",		DT_BOOL, R_NONE, OPTWAITKEY, 1 },
  /*
  ** .pp
  ** Controls whether Mutt will ask you to press a key after \fIshell-
  ** escape\fP, \fIpipe-message\fP, \fIpipe-entry\fP, \fIprint-message\fP,
  ** and \fIprint-entry\fP commands.
  ** .pp
  ** It is also used when viewing attachments with ``$auto_view'', provided
  ** that the corresponding mailcap entry has a \fIneedsterminal\fP flag,
  ** and the external program is interactive.
  ** .pp
  ** When set, Mutt will always ask for a key. When unset, Mutt will wait
  ** for a key only if the external command returned a non-zero status.
  */
  { "weed",		DT_BOOL, R_NONE, OPTWEED, 1 },
  /*
  ** .pp
  ** When set, mutt will weed headers when when displaying, forwarding,
  ** printing, or replying to messages.
  */
  { "wrap_search",	DT_BOOL, R_NONE, OPTWRAPSEARCH, 1 },
  /*
  ** .pp
  ** Controls whether searches wrap around the end of the mailbox.
  ** .pp
  ** When set, searches will wrap around the first (or last) message. When
  ** unset, searches will not wrap.
  */
  { "wrapmargin",	DT_NUM,	 R_PAGER, UL &WrapMargin, 0 },
  /*
  ** .pp
  ** Controls the size of the margin remaining at the right side of
  ** the terminal when mutt's pager does smart wrapping.
  */
  { "write_inc",	DT_NUM,	 R_NONE, UL &WriteInc, 10 },
  /*
  ** .pp
  ** When writing a mailbox, a message will be printed every
  ** \fIwrite_inc\fP messages to indicate progress.  If set to 0, only a
  ** single message will be displayed before writing a mailbox.
  ** .pp
  ** Also see the ``$$read_inc'' variable.
  */
  { "write_bcc",	DT_BOOL, R_NONE, OPTWRITEBCC, 1},
  /*
  ** .pp
  ** Controls whether mutt writes out the Bcc header when preparing
  ** messages to be sent.  Exim users may wish to use this.
  */
  /*--*/
  { NULL }
};

const struct mapping_t SortMethods[] = {
  { "date",		SORT_DATE },
  { "date-sent",	SORT_DATE },
  { "date-received",	SORT_RECEIVED },
  { "mailbox-order",	SORT_ORDER },
  { "subject",		SORT_SUBJECT },
  { "from",		SORT_FROM },
  { "size",		SORT_SIZE },
  { "threads",		SORT_THREADS },
  { "to",		SORT_TO },
  { "score",		SORT_SCORE },
  { "spam",		SORT_SPAM },
  { NULL,		0 }
};

/* same as SortMethods, but with "threads" replaced by "date" */

const struct mapping_t SortAuxMethods[] = {
  { "date",		SORT_DATE },
  { "date-sent",	SORT_DATE },
  { "date-received",	SORT_RECEIVED },
  { "mailbox-order",	SORT_ORDER },
  { "subject",		SORT_SUBJECT },
  { "from",		SORT_FROM },
  { "size",		SORT_SIZE },
  { "threads",		SORT_DATE },	/* note: sort_aux == threads
					 * isn't possible. 
					 */
  { "to",		SORT_TO },
  { "score",		SORT_SCORE },
  { "spam",		SORT_SPAM },
  { NULL,		0 }
};
  

const struct mapping_t SortBrowserMethods[] = {
  { "alpha",	SORT_SUBJECT },
  { "date",	SORT_DATE },
  { "size",	SORT_SIZE },
  { "unsorted",	SORT_ORDER },
  { NULL }
};

const struct mapping_t SortAliasMethods[] = {
  { "alias",	SORT_ALIAS },
  { "address",	SORT_ADDRESS },
  { "unsorted", SORT_ORDER },
  { NULL }
};

const struct mapping_t SortKeyMethods[] = {
  { "address",	SORT_ADDRESS },
  { "date",	SORT_DATE },
  { "keyid",	SORT_KEYID },
  { "trust",	SORT_TRUST },
  { NULL }
};


/* functions used to parse commands in a rc file */

static int parse_list (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_rx_list (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_spam_list (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_unlist (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_rx_unlist (BUFFER *, BUFFER *, unsigned long, BUFFER *);

static int parse_unlists (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_alias (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_unalias (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_ignore (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_unignore (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_source (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_set (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_my_hdr (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_unmy_hdr (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_subscribe (BUFFER *, BUFFER *, unsigned long, BUFFER *);

static int parse_alternates (BUFFER *, BUFFER *, unsigned long, BUFFER *);
static int parse_unalternates (BUFFER *, BUFFER *, unsigned long, BUFFER *);

struct command_t
{
  char *name;
  int (*func) (BUFFER *, BUFFER *, unsigned long, BUFFER *);
  unsigned long data;
};

struct command_t Commands[] = {
  { "alternates",	parse_alternates,	UL &Alternates },
  { "unalternates",	parse_unalternates,	UL &Alternates },
#ifdef USE_SOCKET
  { "account-hook",     mutt_parse_hook,        M_ACCOUNTHOOK },
#endif
  { "alias",		parse_alias,		0 },
  { "auto_view",	parse_list,		UL &AutoViewList },
  { "alternative_order",	parse_list,	UL &AlternativeOrderList},
  { "bind",		mutt_parse_bind,	0 },
  { "charset-hook",	mutt_parse_hook,	M_CHARSETHOOK },
#ifdef HAVE_COLOR
  { "color",		mutt_parse_color,	0 },
  { "uncolor",		mutt_parse_uncolor,	0 },
#endif
  { "exec",		mutt_parse_exec,	0 },
  { "fcc-hook",		mutt_parse_hook,	M_FCCHOOK },
  { "fcc-save-hook",	mutt_parse_hook,	M_FCCHOOK | M_SAVEHOOK },
  { "folder-hook",	mutt_parse_hook,	M_FOLDERHOOK },
  { "hdr_order",	parse_list,		UL &HeaderOrderList },
#ifdef HAVE_ICONV
  { "iconv-hook",	mutt_parse_hook,	M_ICONVHOOK }, 
#endif
  { "ignore",		parse_ignore,		0 },
  { "lists",		parse_rx_list,		UL &MailLists },
  { "macro",		mutt_parse_macro,	0 },
  { "mailboxes",	mutt_parse_mailboxes,	M_MAILBOXES },
  { "unmailboxes",	mutt_parse_mailboxes,	M_UNMAILBOXES },
  { "message-hook",	mutt_parse_hook,	M_MESSAGEHOOK },
  { "mbox-hook",	mutt_parse_hook,	M_MBOXHOOK },
  { "mime_lookup",	parse_list,	UL &MimeLookupList },
  { "unmime_lookup",	parse_unlist,	UL &MimeLookupList },
  { "mono",		mutt_parse_mono,	0 },
  { "my_hdr",		parse_my_hdr,		0 },
  { "pgp-hook",		mutt_parse_hook,	M_CRYPTHOOK },
  { "crypt-hook",	mutt_parse_hook,	M_CRYPTHOOK },
  { "push",		mutt_parse_push,	0 },
  { "reply-hook",	mutt_parse_hook,	M_REPLYHOOK },
  { "reset",		parse_set,		M_SET_RESET },
  { "save-hook",	mutt_parse_hook,	M_SAVEHOOK },
  { "score",		mutt_parse_score,	0 },
  { "send-hook",	mutt_parse_hook,	M_SENDHOOK },
  { "send2-hook",	mutt_parse_hook,	M_SEND2HOOK },
  { "set",		parse_set,		0 },
  { "source",		parse_source,		0 },
  { "spam",		parse_spam_list,	M_SPAM },
  { "nospam",		parse_spam_list,	M_NOSPAM },
  { "subscribe",	parse_subscribe,	0 },
  { "toggle",		parse_set,		M_SET_INV },
  { "unalias",		parse_unalias,		0 },
  { "unalternative_order",parse_unlist,		UL &AlternativeOrderList },
  { "unauto_view",	parse_unlist,		UL &AutoViewList },
  { "unhdr_order",	parse_unlist,		UL &HeaderOrderList },
  { "unhook",		mutt_parse_unhook,	0 },
  { "unignore",		parse_unignore,		0 },
  { "unlists",		parse_unlists,		0 },
  { "unmono",		mutt_parse_unmono,	0 },
  { "unmy_hdr",		parse_unmy_hdr,		0 },
  { "unscore",		mutt_parse_unscore,	0 },
  { "unset",		parse_set,		M_SET_UNSET },
  { "unsubscribe",	parse_rx_unlist,	UL &SubscribedLists },
  { NULL }
};
