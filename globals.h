/**
 * @file
 * Hundreds of global variables to back the user variables
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2016 Michael R. Elkins <me@mutt.org>
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

#ifndef MUTT_GLOBALS_H
#define MUTT_GLOBALS_H

#include "config.h"
#include <signal.h> // IWYU pragma: keep
#include <stdbool.h>
#include "mutt/mutt.h"
#include "keymap.h"
#include "where.h"

#ifdef MAIN_C
/* so that global vars get included */
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "sort.h"
#endif /* MAIN_C */

WHERE struct Colors    *Colors; ///< Wrapper around the user's colour settings

WHERE struct Context *Context;

WHERE bool ErrorBufMessage;            ///< true if the last message was an error
WHERE char ErrorBuf[256];              ///< Copy of the last error message
WHERE char AttachmentMarker[256];      ///< Unique ANSI string to mark PGP messages in an email
WHERE char ProtectedHeaderMarker[256]; ///< Unique ANSI string to mark protected headers in an email

WHERE char *HomeDir;       ///< User's home directory
WHERE char *ShortHostname; ///< Short version of the hostname

WHERE char *Username; ///< User's login name

WHERE char *CurrentFolder; ///< Currently selected mailbox
WHERE char *LastFolder;    ///< Previously selected mailbox

extern const char *GitVer;

WHERE struct Hash *ReverseAliases;     ///< Hash table of aliases (email address -> alias)
WHERE struct Hash *TagFormats;         ///< Hash table of tag-formats (tag -> format string)

/* Lists of strings */
WHERE struct ListHead AlternativeOrderList INITVAL(STAILQ_HEAD_INITIALIZER(AlternativeOrderList)); ///< List of preferred mime types to display
WHERE struct ListHead AutoViewList INITVAL(STAILQ_HEAD_INITIALIZER(AutoViewList));                 ///< List of mime types to auto view
WHERE struct ListHead HeaderOrderList INITVAL(STAILQ_HEAD_INITIALIZER(HeaderOrderList));           ///< List of header fields in the order they should be displayed
WHERE struct ListHead MimeLookupList INITVAL(STAILQ_HEAD_INITIALIZER(MimeLookupList));             ///< List of mime types that that shouldn't use the mailcap entry
WHERE struct ListHead Muttrc INITVAL(STAILQ_HEAD_INITIALIZER(Muttrc));                             ///< List of config files to read
#ifdef USE_SIDEBAR
WHERE struct ListHead SidebarWhitelist INITVAL(STAILQ_HEAD_INITIALIZER(SidebarWhitelist));         ///< List of mailboxes to always display in the sidebar
#endif
WHERE struct ListHead TempAttachmentsList INITVAL(STAILQ_HEAD_INITIALIZER(TempAttachmentsList));   ///< List of temporary files for displaying attachments
WHERE struct ListHead UserHeader INITVAL(STAILQ_HEAD_INITIALIZER(UserHeader));                     ///< List of custom headers to add to outgoing emails

WHERE struct RegexList Alternates INITVAL(STAILQ_HEAD_INITIALIZER(Alternates));               ///< List of regexes to match the user's alternate email addresses
WHERE struct RegexList UnAlternates INITVAL(STAILQ_HEAD_INITIALIZER(UnAlternates));           ///< List of regexes to blacklist false matches in Alternates

/* flags for received signals */
WHERE SIG_ATOMIC_VOLATILE_T SigAlrm;  ///< true after SIGALRM is received
WHERE SIG_ATOMIC_VOLATILE_T SigInt;   ///< true after SIGINT is received
WHERE SIG_ATOMIC_VOLATILE_T SigWinch; ///< true after SIGWINCH is received

WHERE enum MenuType CurrentMenu; ///< Current Menu, e.g. #MENU_PAGER

WHERE struct AliasList Aliases INITVAL(TAILQ_HEAD_INITIALIZER(Aliases)); ///< List of all the user's email aliases

#ifdef USE_AUTOCRYPT
WHERE char *AutocryptSignAs;     ///< Autocrypt Key id to sign as
WHERE char *AutocryptDefaultKey; ///< Autocrypt default key id (used for postponing messages)
#endif

/* All the variables below are backing for config items */

WHERE struct Address *C_EnvelopeFromAddress; ///< Config: Manually set the sender for outgoing messages
WHERE struct Address *C_From;                ///< Config: Default 'From' address to use, if isn't otherwise set

WHERE char *C_AliasFile;                     ///< Config: Save new aliases to this file
WHERE char *C_Attribution;                   ///< Config: Message to start a reply, "On DATE, PERSON wrote:"
WHERE char *C_AttributionLocale;             ///< Config: Locale for dates in the attribution message
WHERE char *C_AttachFormat;                  ///< Config: printf-like format string for the attachment menu
#ifdef USE_AUTOCRYPT
WHERE char *C_AutocryptAcctFormat;           ///< Config: Format of the autocrypt account menu
WHERE char *C_AutocryptDir;                  ///< Config: Location of autocrypt files, including the GPG keyring and sqlite database
#endif
WHERE char *C_ConfigCharset;                 ///< Config: Character set that the config files are in
WHERE char *C_CryptProtectedHeadersSubject;  ///< Config: Use this as the subject for encrypted emails
WHERE char *C_DateFormat;                    ///< Config: strftime format string for the `%d` expando
WHERE char *C_DsnNotify;                     ///< Config: Request notification for message delivery or delay
WHERE char *C_DsnReturn;                     ///< Config: What to send as a notification of message delivery or delay
WHERE char *C_Editor;                        ///< Config: External command to use as an email editor
WHERE char *C_ExternalSearchCommand;         ///< Config: External search command
WHERE char *C_Hostname;                      ///< Config: Fully-qualified domain name of this machine
WHERE char *C_IndexFormat;                   ///< Config: printf-like format string for the index menu (emails)

#ifdef USE_IMAP
WHERE char *C_ImapUser;                      ///< Config: (imap) Username for the IMAP server
#endif
WHERE char *C_Mbox;                          ///< Config: Folder that receives read emails (see Move)
WHERE struct Slist *C_MailcapPath;           ///< Config: Colon-separated list of mailcap files
WHERE char *C_Folder;                        ///< Config: Base folder for a set of mailboxes
#ifdef USE_HCACHE
WHERE char *C_HeaderCache;                   ///< Config: (hcache) Directory/file for the header cache database
#if defined(HAVE_GDBM) || defined(HAVE_BDB)
WHERE long C_HeaderCachePagesize;            ///< Config: (hcache) Database page size (gdbm,bdb4)
#endif /* HAVE_GDBM || HAVE_BDB */
#endif /* USE_HCACHE */


#ifdef USE_NNTP
WHERE char *C_NewsServer;                    ///< Config: (nntp) Url of the news server
#endif
WHERE char *C_Record;                        ///< Config: Folder to save 'sent' messages
WHERE char *C_Pager;                         ///< Config: External command for viewing messages, or 'builtin' to use NeoMutt's
WHERE char *C_PagerFormat;                   ///< Config: printf-like format string for the pager's status bar
WHERE char *C_Postponed;                     ///< Config: Folder to store postponed messages
WHERE char *C_IndentString;                  ///< Config: String used to indent 'reply' text
WHERE char *C_PrintCommand;                  ///< Config: External command to print a message
WHERE char *C_NewMailCommand;                ///< Config: External command to run when new mail arrives
WHERE char *C_Realname;                      ///< Config: Real name of the user
WHERE char *C_Shell;                         ///< Config: External command to run subshells in
WHERE char *C_SimpleSearch;                  ///< Config: Pattern to search for when search doesn't contain ~'s
#ifdef USE_SMTP
WHERE char *C_SmtpUrl;                       ///< Config: (smtp) Url of the SMTP server
#endif /* USE_SMTP */
WHERE char *C_Spoolfile;                     ///< Config: Inbox
WHERE char *C_StatusFormat;                  ///< Config: printf-like format string for the index's status line
WHERE char *C_TsStatusFormat;                ///< Config: printf-like format string for the terminal's status (window title)
WHERE char *C_TsIconFormat;                  ///< Config: printf-like format string for the terminal's icon title
WHERE char *C_Visual;                        ///< Config: Editor to use when '~v' is given in the built-in editor

WHERE short C_SleepTime;                     ///< Config: Time to pause after certain info messages
WHERE short C_Timeout;                       ///< Config: Time to wait for user input in menus
WHERE short C_Wrap;                          ///< Config: Width to wrap text in the pager

#ifdef USE_IMAP
WHERE short C_ImapKeepalive;                 ///< Config: (imap) Time to wait before polling an open IMAP connection
WHERE short C_ImapPollTimeout;               ///< Config: (imap) Maximum time to wait for a server response
#endif

WHERE char *C_PgpDefaultKey;                 ///< Config: Default key to use for PGP operations
WHERE char *C_PgpSignAs;                     ///< Config: Use this alternative key for signing messages
WHERE char *C_PgpEntryFormat;                ///< Config: printf-like format string for the PGP key selection menu

WHERE char *C_SmimeDefaultKey;               ///< Config: Default key for SMIME operations
WHERE char *C_SmimeSignAs;                   ///< Config: Use this alternative key for signing messages
WHERE char *C_SmimeEncryptWith;              ///< Config: Algorithm for encryption

#ifdef USE_NOTMUCH
WHERE int C_NmQueryWindowDuration;           ///< Config: (notmuch) Time duration of the current search window
WHERE char *C_NmQueryWindowCurrentSearch;    ///< Config: (notmuch) Current search parameters
#endif

/* These variables are backing for config items */
WHERE struct Regex *C_Mask;                  ///< Config: Only display files/dirs matching this regex in the browser
WHERE struct Regex *C_QuoteRegex;            ///< Config: Regex to match quoted text in a reply
WHERE int C_ToggleQuotedShowLevels;          ///< Config: Number of quote levels to show with toggle-quoted

/* Quad-options */
WHERE unsigned char C_Bounce;                ///< Config: Confirm before bouncing a message
WHERE unsigned char C_Copy;                  ///< Config: Save outgoing emails to $record
WHERE unsigned char C_Delete;                ///< Config: Really delete messages, when the mailbox is closed
WHERE unsigned char C_ForwardAttachments;    ///< Config: Forward attachments when forwarding a message
WHERE unsigned char C_MimeForward;           ///< Config: Forward a message as a 'message/RFC822' MIME part
WHERE unsigned char C_Print;                 ///< Config: Confirm before printing a message
WHERE unsigned char C_Quit;                  ///< Config: Prompt before exiting NeoMutt
#ifdef USE_SSL
WHERE unsigned char C_SslStarttls;           ///< Config: (ssl) Use STARTTLS on servers advertising the capability
#endif
#ifdef USE_NNTP
WHERE unsigned char C_PostModerated;         ///< Config: (nntp) Allow posting to moderated newsgroups
WHERE unsigned char C_FollowupToPoster;      ///< Config: (nntp) Reply to the poster if 'poster' is in the 'Followup-To' header
#endif

WHERE bool C_ArrowCursor;                    ///< Config: Use an arrow '->' instead of highlighting in the index
WHERE bool C_AsciiChars;                     ///< Config: Use plain ASCII characters, when drawing email threads
WHERE bool C_Askbcc;                         ///< Config: Ask the user for the blind-carbon-copy recipients
WHERE bool C_Askcc;                          ///< Config: Ask the user for the carbon-copy recipients
#ifdef USE_AUTOCRYPT
WHERE bool C_Autocrypt;                      ///< Config: Enables the Autocrypt feature
WHERE bool C_AutocryptReply;                 ///< Config: Replying to an autocrypt email automatically enables autocrypt in the reply
#endif
WHERE bool C_Autoedit;                       ///< Config: Skip the initial compose menu and edit the email
WHERE bool C_AutoTag;                        ///< Config: Automatically apply actions to all tagged messages
WHERE bool C_Beep;                           ///< Config: Make a noise when an error occurs
WHERE bool C_BeepNew;                        ///< Config: Make a noise when new mail arrives
WHERE bool C_BrailleFriendly;                ///< Config: Move the cursor to the beginning of the line
WHERE bool C_CheckMboxSize;                  ///< Config: (mbox,mmdf) Use mailbox size as an indicator of new mail
WHERE bool C_Confirmappend;                  ///< Config: Confirm before appending emails to a mailbox
WHERE bool C_Confirmcreate;                  ///< Config: Confirm before creating a new mailbox
WHERE bool C_DeleteUntag;                    ///< Config: Untag messages when they are marked for deletion
WHERE bool C_EditHeaders;                    ///< Config: Let the user edit the email headers whilst editing an email
WHERE bool C_FlagSafe;                       ///< Config: Protect flagged messages from deletion
WHERE bool C_ForwardDecode;                  ///< Config: Decode the message when forwarding it
WHERE bool C_ForwardQuote;                   ///< Config: Automatically quote a forwarded message using #C_IndentString
#ifdef USE_HCACHE
#if defined(HAVE_QDBM) || defined(HAVE_TC) || defined(HAVE_KC)
WHERE bool C_HeaderCacheCompress;            ///< Config: (hcache) Enable database compression (qdbm,tokyocabinet,kyotocabinet)
#endif /* HAVE_QDBM | HAVE_TC | HAVE_KC */
#endif /* USE_HCACHE */
WHERE bool C_Header;                         ///< Config: Include the message headers in the reply email (Weed applies)
WHERE bool C_Help;                           ///< Config: Display a help line with common key bindings
#ifdef USE_IMAP
WHERE bool C_ImapCheckSubscribed;            ///< Config: (imap) When opening a mailbox, ask the server for a list of subscribed folders
WHERE bool C_ImapCondstore;                  ///< Config: (imap) Enable the CONDSTORE extension
WHERE bool C_ImapListSubscribed;             ///< Config: (imap) When browsing a mailbox, only display subscribed folders
WHERE bool C_ImapPassive;                    ///< Config: (imap) Reuse an existing IMAP connection to check for new mail
WHERE bool C_ImapPeek;                       ///< Config: (imap) Don't mark messages as read when fetching them from the server
WHERE bool C_ImapQresync;                    ///< Config: (imap) Enable the QRESYNC extension
#endif
#ifdef USE_SSL
#ifndef USE_SSL_GNUTLS
WHERE bool C_SslUsesystemcerts;              ///< Config: (ssl) Use CA certificates in the system-wide store
WHERE bool C_SslUseSslv2;                    ///< Config: (ssl) INSECURE: Use SSLv2 for authentication
#endif /* USE_SSL_GNUTLS */
WHERE bool C_SslForceTls;                    ///< Config: (ssl) Require TLS encryption for all connections
#if defined(USE_SSL_OPENSSL) && defined(HAVE_SSL_PARTIAL_CHAIN)
WHERE bool C_SslVerifyPartialChains;         ///< Config: (ssl) Allow verification using partial certificate chains
#endif /* USE_SSL_OPENSSL */
#endif /* defined(USE_SSL) */
WHERE bool C_MailCheckRecent;                ///< Config: Notify the user about new mail since the last time the mailbox was opened
WHERE bool C_MaildirTrash;                   ///< Config: Use the maildir 'trashed' flag, rather than deleting
WHERE bool C_Markers;                        ///< Config: Display a '+' at the beginning of wrapped lines in the pager
#if defined(USE_IMAP) || defined(USE_POP)
WHERE bool C_MessageCacheClean;              ///< Config: (imap/pop) Clean out obsolete entries from the message cache
#endif
WHERE bool C_ReadOnly;                       ///< Config: Open folders in read-only mode
WHERE bool C_Resolve;                        ///< Config: Move to the next email whenever a command modifies an email
WHERE bool C_ResumeDraftFiles;               ///< Config: Process draft files like postponed messages
WHERE bool C_SaveAddress;                    ///< Config: Use sender's full address as a default save folder
WHERE bool C_SaveEmpty;                      ///< Config: (mbox,mmdf) Preserve empty mailboxes
WHERE bool C_Score;                          ///< Config: Use message scoring
WHERE bool C_SizeShowBytes;                  ///< Config: Show smaller sizes in bytes
WHERE bool C_SizeShowFractions;              ///< Config: Show size fractions with a single decimal place
WHERE bool C_SizeShowMb;                     ///< Config: Show sizes in megabytes for sizes greater than 1 megabyte
WHERE bool C_SizeUnitsOnLeft;                ///< Config: Show the units as a prefix to the size
WHERE bool C_StatusOnTop;                    ///< Config: Display the status bar at the top
WHERE bool C_Suspend;                        ///< Config: Allow the user to suspend NeoMutt using '^Z'
WHERE bool C_TextFlowed;                     ///< Config: Generate 'format=flowed' messages
WHERE bool C_TsEnabled;                      ///< Config: Allow NeoMutt to set the terminal status line and icon
WHERE bool C_UseDomain;                      ///< Config: Qualify local addresses using this domain
WHERE bool C_WaitKey;                        ///< Config: Prompt to press a key after running external commands
WHERE bool C_WrapSearch;                     ///< Config: Wrap around when the search hits the end
WHERE bool C_WriteBcc;                       ///< Config: Write out the 'Bcc' field when preparing to send a mail

WHERE bool C_CryptUsePka;                    ///< Config: Use GPGME to use PKA (lookup PGP keys using DNS)

/* PGP options */

WHERE bool C_CryptConfirmhook;               ///< Config: Prompt the user to confirm keys before use
WHERE bool C_CryptOpportunisticEncrypt;      ///< Config: Enable encryption when the recipient's key is available
WHERE bool C_CryptProtectedHeadersRead;      ///< Config: Display protected headers (Memory Hole) in the pager
WHERE bool C_CryptProtectedHeadersSave;      ///< Config: Save the cleartext Subject with the headers
WHERE bool C_CryptProtectedHeadersWrite;     ///< Config: Generate protected header (Memory Hole) for signed and encrypted emails
WHERE bool C_SmimeIsDefault;                 ///< Config: Use SMIME rather than PGP by default
WHERE bool C_PgpIgnoreSubkeys;               ///< Config: Only use the principal PGP key
WHERE bool C_PgpLongIds;                     ///< Config: Display long PGP key IDs to the user
WHERE bool C_PgpShowUnusable;                ///< Config: Show non-usable keys in the key selection
WHERE bool C_PgpAutoinline;                  ///< Config: Use old-style inline PGP messages (not recommended)

/* news options */

#ifdef USE_NNTP
WHERE bool C_SaveUnsubscribed;               ///< Config: (nntp) Save a list of unsubscribed newsgroups to the 'newsrc'
WHERE bool C_XCommentTo;                     ///< Config: (nntp) Add 'X-Comment-To' header that contains article author
#endif

#ifdef USE_NOTMUCH
WHERE bool C_VirtualSpoolfile;               ///< Config: (notmuch) Use the first virtual mailbox as a spool file
#endif

#endif /* MUTT_GLOBALS_H */
