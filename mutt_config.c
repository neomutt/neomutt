/**
 * @file
 * Definitions of config variables
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013,2016 Michael R. Elkins <me@mutt.org>
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

/**
 * @page neo_mutt_config Definitions of config variables
 *
 * Definitions of config variables
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "bcache/lib.h"
#include "compose/lib.h"
#include "browser.h"
#include "commands.h"
#include "handler.h"
#include "hdrline.h"
#include "hook.h"
#include "index.h"
#include "init.h"
#include "mailcap.h"
#include "main.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "mutt_menu.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mx.h"
#include "pager.h"
#include "progress.h"
#include "recvattach.h"
#include "recvcmd.h"
#include "remailer.h"
#include "rfc3676.h"
#include "score.h"
#include "sort.h"
#include "status.h"

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType cst_##NAME;                                \
  cs_register_type(CS, &cst_##NAME)

#define CONFIG_INIT_VARS(CS, NAME)                                             \
  bool config_init_##NAME(struct ConfigSet *cs);                               \
  config_init_##NAME(CS)

/* These options are deprecated */
char *C_Escape = NULL;
bool C_IgnoreLinearWhiteSpace = false;

/**
 * SortAuxMethods - Sort methods for '$sort_aux' for the index
 */
const struct Mapping SortAuxMethods[] = {
  // clang-format off
  { "date",          SORT_DATE },
  { "date-sent",     SORT_DATE },
  { "threads",       SORT_DATE },
  { "date-received", SORT_RECEIVED },
  { "from",          SORT_FROM },
  { "label",         SORT_LABEL },
  { "unsorted",      SORT_ORDER },
  { "mailbox-order", SORT_ORDER },
  { "score",         SORT_SCORE },
  { "size",          SORT_SIZE },
  { "spam",          SORT_SPAM },
  { "subject",       SORT_SUBJECT },
  { "to",            SORT_TO },
  { NULL,            0 },
  // clang-format on
};

/**
 * SortMethods - Sort methods for '$sort' for the index
 */
const struct Mapping SortMethods[] = {
  // clang-format off
  { "date",          SORT_DATE },
  { "date-sent",     SORT_DATE },
  { "date-received", SORT_RECEIVED },
  { "from",          SORT_FROM },
  { "label",         SORT_LABEL },
  { "unsorted",      SORT_ORDER },
  { "mailbox-order", SORT_ORDER },
  { "score",         SORT_SCORE },
  { "size",          SORT_SIZE },
  { "spam",          SORT_SPAM },
  { "subject",       SORT_SUBJECT },
  { "threads",       SORT_THREADS },
  { "to",            SORT_TO },
  { NULL,            0 },
  // clang-format on
};

/**
 * SortBrowserMethods - Sort methods for the folder/dir browser
 */
const struct Mapping SortBrowserMethods[] = {
  // clang-format off
  { "alpha",    SORT_SUBJECT },
  { "count",    SORT_COUNT },
  { "date",     SORT_DATE },
  { "desc",     SORT_DESC },
  { "new",      SORT_UNREAD },
  { "unread",   SORT_UNREAD },
  { "size",     SORT_SIZE },
  { "unsorted", SORT_ORDER },
  { NULL,       0 },
  // clang-format on
};

/**
 * multipart_validator - Validate the "show_multipart_alternative" config variable - Implements ConfigDef::validator()
 */
int multipart_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                        intptr_t value, struct Buffer *err)
{
  if (value == 0)
    return CSR_SUCCESS;

  const char *str = (const char *) value;

  if (mutt_str_equal(str, "inline") || mutt_str_equal(str, "info"))
    return CSR_SUCCESS;

  mutt_buffer_printf(err, _("Invalid value for option %s: %s"), cdef->name, str);
  return CSR_ERR_INVALID;
}

/**
 * pager_validator - Check for config variables that can't be set from the pager - Implements ConfigDef::validator()
 */
int pager_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                    intptr_t value, struct Buffer *err)
{
  if (CurrentMenu == MENU_PAGER)
  {
    mutt_buffer_printf(err, _("Option %s may not be set or reset from the pager"),
                       cdef->name);
    return CSR_ERR_INVALID;
  }

  return CSR_SUCCESS;
}

/**
 * reply_validator - Validate the "reply_regex" config variable - Implements ConfigDef::validator()
 */
int reply_validator(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                    intptr_t value, struct Buffer *err)
{
  if (pager_validator(cs, cdef, value, err) != CSR_SUCCESS)
    return CSR_ERR_INVALID;

  if (!OptAttachMsg)
    return CSR_SUCCESS;

  mutt_buffer_printf(err, _("Option %s may not be set when in attach-message mode"),
                     cdef->name);
  return CSR_ERR_INVALID;
}

struct ConfigDef MainVars[] = {
  // clang-format off
  { "abort_backspace", DT_BOOL, &C_AbortBackspace, true, 0, NULL,
    "Hitting backspace against an empty prompt aborts the prompt"
  },
  { "abort_key", DT_STRING|DT_NOT_EMPTY, &C_AbortKey, IP "\007", 0, NULL,
    "String representation of key to abort prompts"
  },
  { "allow_ansi", DT_BOOL, &C_AllowAnsi, false, 0, NULL,
    "Allow ANSI colour codes in rich text messages"
  },
  { "arrow_cursor", DT_BOOL|R_MENU, &C_ArrowCursor, false, 0, NULL,
    "Use an arrow '->' instead of highlighting in the index"
  },
  { "arrow_string", DT_STRING|DT_NOT_EMPTY, &C_ArrowString, IP "->", 0, NULL,
    "Use an custom string for arrow_cursor"
  },
  { "ascii_chars", DT_BOOL|R_INDEX|R_PAGER, &C_AsciiChars, false, 0, NULL,
    "Use plain ASCII characters, when drawing email threads"
  },
  { "askbcc", DT_BOOL, &C_Askbcc, false, 0, NULL,
    "Ask the user for the blind-carbon-copy recipients"
  },
  { "askcc", DT_BOOL, &C_Askcc, false, 0, NULL,
    "Ask the user for the carbon-copy recipients"
  },
  { "assumed_charset", DT_STRING, &C_AssumedCharset, 0, 0, charset_validator,
    "If a message is missing a character set, assume this character set"
  },
  { "attach_format", DT_STRING|DT_NOT_EMPTY, &C_AttachFormat, IP "%u%D%I %t%4n %T%.40d%> [%.7m/%.10M, %.6e%?C?, %C?, %s] ", 0, NULL,
    "printf-like format string for the attachment menu"
  },
  { "attach_save_dir", DT_PATH|DT_PATH_DIR, &C_AttachSaveDir, IP "./", 0, NULL,
    "Default directory where attachments are saved"
  },
  { "attach_save_without_prompting", DT_BOOL, &C_AttachSaveWithoutPrompting, false, 0, NULL,
    "If true, then don't prompt to save"
  },
  { "attach_sep", DT_STRING, &C_AttachSep, IP "\n", 0, NULL,
    "Separator to add between saved/printed/piped attachments"
  },
  { "attach_split", DT_BOOL, &C_AttachSplit, true, 0, NULL,
    "Save/print/pipe tagged messages individually"
  },
  { "attribution", DT_STRING, &C_Attribution, IP "On %d, %n wrote:", 0, NULL,
    "Message to start a reply, 'On DATE, PERSON wrote:'"
  },
  { "attribution_locale", DT_STRING, &C_AttributionLocale, 0, 0, NULL,
    "Locale for dates in the attribution message"
  },
  { "auto_subscribe", DT_BOOL, &C_AutoSubscribe, false, 0, NULL,
    "Automatically check if the user is subscribed to a mailing list"
  },
  { "auto_tag", DT_BOOL, &C_AutoTag, false, 0, NULL,
    "Automatically apply actions to all tagged messages"
  },
  { "autoedit", DT_BOOL, &C_Autoedit, false, 0, NULL,
    "Skip the initial compose menu and edit the email"
  },
  { "beep", DT_BOOL, &C_Beep, true, 0, NULL,
    "Make a noise when an error occurs"
  },
  { "beep_new", DT_BOOL, &C_BeepNew, false, 0, NULL,
    "Make a noise when new mail arrives"
  },
  { "bounce", DT_QUAD, &C_Bounce, MUTT_ASKYES, 0, NULL,
    "Confirm before bouncing a message"
  },
  { "braille_friendly", DT_BOOL, &C_BrailleFriendly, false, 0, NULL,
    "Move the cursor to the beginning of the line"
  },
  { "browser_abbreviate_mailboxes", DT_BOOL, &C_BrowserAbbreviateMailboxes, true, 0, NULL,
    "Abbreviate mailboxes using '~' and '=' in the browser"
  },
  { "change_folder_next", DT_BOOL, &C_ChangeFolderNext, false, 0, NULL,
    "Suggest the next folder, rather than the first when using '<change-folder>'"
  },
  { "charset", DT_STRING|DT_NOT_EMPTY|DT_CHARSET_SINGLE, &C_Charset, 0, 0, charset_validator,
    "Default character set for displaying text on screen"
  },
  { "collapse_all", DT_BOOL, &C_CollapseAll, false, 0, NULL,
    "Collapse all threads when entering a folder"
  },
  { "collapse_flagged", DT_BOOL, &C_CollapseFlagged, true, 0, NULL,
    "Prevent the collapse of threads with flagged emails"
  },
  { "collapse_unread", DT_BOOL, &C_CollapseUnread, true, 0, NULL,
    "Prevent the collapse of threads with unread emails"
  },
  { "config_charset", DT_STRING, &C_ConfigCharset, 0, 0, charset_validator,
    "Character set that the config files are in"
  },
  { "confirmappend", DT_BOOL, &C_Confirmappend, true, 0, NULL,
    "Confirm before appending emails to a mailbox"
  },
  { "confirmcreate", DT_BOOL, &C_Confirmcreate, true, 0, NULL,
    "Confirm before creating a new mailbox"
  },
  { "copy_decode_weed", DT_BOOL, &C_CopyDecodeWeed, false, 0, NULL,
    "Controls whether to weed headers when copying or saving emails"
  },
  { "count_alternatives", DT_BOOL, &C_CountAlternatives, false, 0, NULL,
    "Recurse inside multipart/alternatives while counting attachments"
  },
  { "crypt_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_CryptChars, IP "SPsK ", 0, NULL,
    "User-configurable crypto flags: signed, encrypted etc."
  },
  { "date_format", DT_STRING|DT_NOT_EMPTY|R_MENU, &C_DateFormat, IP "!%a, %b %d, %Y at %I:%M:%S%p %Z", 0, NULL,
    "strftime format string for the `%d` expando"
  },
  { "debug_file", DT_PATH|DT_PATH_FILE, &C_DebugFile, IP "~/.neomuttdebug", 0, NULL,
    "File to save debug logs"
  },
  { "debug_level", DT_NUMBER, &C_DebugLevel, 0, 0, level_validator,
    "Logging level for debug logs"
  },
  { "default_hook", DT_STRING, &C_DefaultHook, IP "~f %s !~P | (~P ~C %s)", 0, NULL,
    "Pattern to use for hooks that only have a simple regex"
  },
  { "delete", DT_QUAD, &C_Delete, MUTT_ASKYES, 0, NULL,
    "Really delete messages, when the mailbox is closed"
  },
  { "delete_untag", DT_BOOL, &C_DeleteUntag, true, 0, NULL,
    "Untag messages when they are marked for deletion"
  },
  { "digest_collapse", DT_BOOL, &C_DigestCollapse, true, 0, NULL,
    "Hide the subparts of a multipart/digest"
  },
  { "display_filter", DT_STRING|DT_COMMAND|R_PAGER, &C_DisplayFilter, 0, 0, NULL,
    "External command to pre-process an email before display"
  },
  { "duplicate_threads", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, &C_DuplicateThreads, true, 0, pager_validator,
    "Highlight messages with duplicated message IDs"
  },
  { "editor", DT_STRING|DT_NOT_EMPTY|DT_COMMAND, &C_Editor, IP "vi", 0, NULL,
    "External command to use as an email editor"
  },
  { "flag_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_FlagChars, IP "*!DdrONon- ", 0, NULL,
    "User-configurable index flags: tagged, new, etc"
  },
  { "flag_safe", DT_BOOL, &C_FlagSafe, false, 0, NULL,
    "Protect flagged messages from deletion"
  },
  { "folder", DT_STRING|DT_MAILBOX, &C_Folder, IP "~/Mail", 0, NULL,
    "Base folder for a set of mailboxes"
  },
  { "folder_format", DT_STRING|DT_NOT_EMPTY|R_MENU, &C_FolderFormat, IP "%2C %t %N %F %2l %-8.8u %-8.8g %8s %d %i", 0, NULL,
    "printf-like format string for the browser's display of folders"
  },
  { "force_name", DT_BOOL, &C_ForceName, false, 0, NULL,
    "Save outgoing mail in a folder of their name"
  },
  { "forward_attachments", DT_QUAD, &C_ForwardAttachments, MUTT_ASKYES, 0, NULL,
    "Forward attachments when forwarding a message"
  },
  { "forward_decode", DT_BOOL, &C_ForwardDecode, true, 0, NULL,
    "Decode the message when forwarding it"
  },
  { "forward_quote", DT_BOOL, &C_ForwardQuote, false, 0, NULL,
    "Automatically quote a forwarded message using `$indent_string`"
  },
  { "from", DT_ADDRESS, &C_From, 0, 0, NULL,
    "Default 'From' address to use, if isn't otherwise set"
  },
  { "from_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_FromChars, 0, 0, NULL,
    "User-configurable index flags: to address, cc address, etc"
  },
  { "gecos_mask", DT_REGEX, &C_GecosMask, IP "^[^,]*", 0, NULL,
    "Regex for parsing GECOS field of /etc/passwd"
  },
  { "header", DT_BOOL, &C_Header, false, 0, NULL,
    "Include the message headers in the reply email (Weed applies)"
  },
  { "header_color_partial", DT_BOOL|R_PAGER_FLOW, &C_HeaderColorPartial, false, 0, NULL,
    "Only colour the part of the header matching the regex"
  },
  { "hidden_tags", DT_SLIST|SLIST_SEP_COMMA, &C_HiddenTags, IP "unread,draft,flagged,passed,replied,attachment,signed,encrypted", 0, NULL,
    "Tags that shouldn't be displayed on screen"
  },
  { "hide_limited", DT_BOOL|R_TREE|R_INDEX, &C_HideLimited, false, 0, NULL,
    "Don't indicate hidden messages, in the thread tree"
  },
  { "hide_missing", DT_BOOL|R_TREE|R_INDEX, &C_HideMissing, true, 0, NULL,
    "Don't indicate missing messages, in the thread tree"
  },
  { "hide_thread_subject", DT_BOOL|R_TREE|R_INDEX, &C_HideThreadSubject, true, 0, NULL,
    "Hide subjects that are similar to that of the parent message"
  },
  { "hide_top_limited", DT_BOOL|R_TREE|R_INDEX, &C_HideTopLimited, false, 0, NULL,
    "Don't indicate hidden top message, in the thread tree"
  },
  { "hide_top_missing", DT_BOOL|R_TREE|R_INDEX, &C_HideTopMissing, true, 0, NULL,
    "Don't indicate missing top message, in the thread tree"
  },
  { "honor_disposition", DT_BOOL, &C_HonorDisposition, false, 0, NULL,
    "Don't display MIME parts inline if they have a disposition of 'attachment'"
  },
  { "hostname", DT_STRING, &C_Hostname, 0, 0, NULL,
    "Fully-qualified domain name of this machine"
  },
#ifdef HAVE_LIBIDN
  { "idn_decode", DT_BOOL|R_MENU, &C_IdnDecode, true, 0, NULL,
    "(idn) Decode international domain names"
  },
  { "idn_encode", DT_BOOL|R_MENU, &C_IdnEncode, true, 0, NULL,
    "(idn) Encode international domain names"
  },
#endif
  { "implicit_autoview", DT_BOOL, &C_ImplicitAutoview, false, 0, NULL,
    "Display MIME attachments inline if a 'copiousoutput' mailcap entry exists"
  },
  { "include_encrypted", DT_BOOL, &C_IncludeEncrypted, false, 0, NULL,
    "Whether to include encrypted content when replying"
  },
  { "include_onlyfirst", DT_BOOL, &C_IncludeOnlyfirst, false, 0, NULL,
    "Only include the first attachment when replying"
  },
  { "indent_string", DT_STRING, &C_IndentString, IP "> ", 0, NULL,
    "String used to indent 'reply' text"
  },
  { "index_format", DT_STRING|DT_NOT_EMPTY|R_INDEX|R_PAGER, &C_IndexFormat, IP "%4C %Z %{%b %d} %-15.15L (%?l?%4l&%4c?) %s", 0, NULL,
    "printf-like format string for the index menu (emails)"
  },
  { "keep_flagged", DT_BOOL, &C_KeepFlagged, false, 0, NULL,
    "Don't move flagged messages from `$spoolfile` to `$mbox`"
  },
  { "mail_check", DT_NUMBER|DT_NOT_NEGATIVE, &C_MailCheck, 5, 0, NULL,
    "Number of seconds before NeoMutt checks for new mail"
  },
  { "mail_check_recent", DT_BOOL, &C_MailCheckRecent, true, 0, NULL,
    "Notify the user about new mail since the last time the mailbox was opened"
  },
  { "mail_check_stats", DT_BOOL, &C_MailCheckStats, false, 0, NULL,
    "Periodically check for new mail"
  },
  { "mail_check_stats_interval", DT_NUMBER|DT_NOT_NEGATIVE, &C_MailCheckStatsInterval, 60, 0, NULL,
    "How often to check for new mail"
  },
  { "mailcap_path", DT_SLIST|SLIST_SEP_COLON, &C_MailcapPath, IP "~/.mailcap:" PKGDATADIR "/mailcap:" SYSCONFDIR "/mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap", 0, NULL,
    "Colon-separated list of mailcap files"
  },
  { "mailcap_sanitize", DT_BOOL, &C_MailcapSanitize, true, 0, NULL,
    "Restrict the possible characters in mailcap expandos"
  },
  { "mark_macro_prefix", DT_STRING, &C_MarkMacroPrefix, IP "'", 0, NULL,
    "Prefix for macros using '<mark-message>'"
  },
  { "mark_old", DT_BOOL|R_INDEX|R_PAGER, &C_MarkOld, true, 0, NULL,
    "Mark new emails as old when leaving the mailbox"
  },
  { "markers", DT_BOOL|R_PAGER_FLOW, &C_Markers, true, 0, NULL,
    "Display a '+' at the beginning of wrapped lines in the pager"
  },
  { "mask", DT_REGEX|DT_REGEX_MATCH_CASE|DT_REGEX_ALLOW_NOT|DT_REGEX_NOSUB, &C_Mask, IP "!^\\.[^.]", 0, NULL,
    "Only display files/dirs matching this regex in the browser"
  },
  { "mbox", DT_STRING|DT_MAILBOX|R_INDEX|R_PAGER, &C_Mbox, IP "~/mbox", 0, NULL,
    "Folder that receives read emails (see Move)"
  },
  { "mbox_type", DT_ENUM, &C_MboxType, MUTT_MBOX, IP &MboxTypeDef, NULL,
    "Default type for creating new mailboxes"
  },
  { "menu_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_MenuContext, 0, 0, NULL,
    "Number of lines of overlap when changing pages in the index"
  },
  { "menu_move_off", DT_BOOL, &C_MenuMoveOff, true, 0, NULL,
    "Allow the last menu item to move off the bottom of the screen"
  },
  { "menu_scroll", DT_BOOL, &C_MenuScroll, false, 0, NULL,
    "Scroll the menu/index by one line, rather than a page"
  },
  { "message_cache_clean", DT_BOOL, &C_MessageCacheClean, false, 0, NULL,
    "(imap/pop) Clean out obsolete entries from the message cache"
  },
  { "message_cachedir", DT_PATH|DT_PATH_DIR, &C_MessageCachedir, 0, 0, NULL,
    "(imap/pop) Directory for the message cache"
  },
  { "message_format", DT_STRING|DT_NOT_EMPTY, &C_MessageFormat, IP "%s", 0, NULL,
    "printf-like format string for listing attached messages"
  },
  { "meta_key", DT_BOOL, &C_MetaKey, false, 0, NULL,
    "Interpret 'ALT-x' as 'ESC-x'"
  },
  { "mime_forward", DT_QUAD, &C_MimeForward, MUTT_NO, 0, NULL,
    "Forward a message as a 'message/RFC822' MIME part"
  },
  { "mime_forward_rest", DT_QUAD, &C_MimeForwardRest, MUTT_YES, 0, NULL,
    "Forward all attachments, even if they can't be decoded"
  },
#ifdef MIXMASTER
  { "mix_entry_format", DT_STRING|DT_NOT_EMPTY, &C_MixEntryFormat, IP "%4n %c %-16s %a", 0, NULL,
    "(mixmaster) printf-like format string for the mixmaster chain"
  },
  { "mixmaster", DT_STRING|DT_COMMAND, &C_Mixmaster, IP MIXMASTER, 0, NULL,
    "(mixmaster) External command to route a mixmaster message"
  },
#endif
  { "move", DT_QUAD, &C_Move, MUTT_NO, 0, NULL,
    "Move emails from `$spoolfile` to `$mbox` when read"
  },
  { "narrow_tree", DT_BOOL|R_TREE|R_INDEX, &C_NarrowTree, false, 0, NULL,
    "Draw a narrower thread tree in the index"
  },
  { "net_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_NetInc, 10, 0, NULL,
    "(socket) Update the progress bar after this many KB sent/received (0 to disable)"
  },
  { "new_mail_command", DT_STRING|DT_COMMAND, &C_NewMailCommand, 0, 0, NULL,
    "External command to run when new mail arrives"
  },
  { "pager", DT_STRING|DT_COMMAND, &C_Pager, IP "builtin", 0, NULL,
    "External command for viewing messages, or 'builtin' to use NeoMutt's"
  },
  { "pager_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_PagerContext, 0, 0, NULL,
    "Number of lines of overlap when changing pages in the pager"
  },
  { "pager_format", DT_STRING|R_PAGER, &C_PagerFormat, IP "-%Z- %C/%m: %-20.20n   %s%*  -- (%P)", 0, NULL,
    "printf-like format string for the pager's status bar"
  },
  { "pager_index_lines", DT_NUMBER|DT_NOT_NEGATIVE|R_PAGER|R_REFLOW, &C_PagerIndexLines, 0, 0, NULL,
    "Number of index lines to display above the pager"
  },
  { "pager_stop", DT_BOOL, &C_PagerStop, false, 0, NULL,
    "Don't automatically open the next message when at the end of a message"
  },
  { "pipe_decode", DT_BOOL, &C_PipeDecode, false, 0, NULL,
    "Decode the message when piping it"
  },
  { "pipe_decode_weed", DT_BOOL, &C_PipeDecodeWeed, true, 0, NULL,
    "Control whether to weed headers when piping an email"
  },
  { "pipe_sep", DT_STRING, &C_PipeSep, IP "\n", 0, NULL,
    "Separator to add between multiple piped messages"
  },
  { "pipe_split", DT_BOOL, &C_PipeSplit, false, 0, NULL,
    "Run the pipe command on each message separately"
  },
  { "postponed", DT_STRING|DT_MAILBOX|R_INDEX, &C_Postponed, IP "~/postponed", 0, NULL,
    "Folder to store postponed messages"
  },
  { "preferred_languages", DT_SLIST|SLIST_SEP_COMMA, &C_PreferredLanguages, 0, 0, NULL,
    "Preferred languages for multilingual MIME"
  },
  { "print", DT_QUAD, &C_Print, MUTT_ASKNO, 0, NULL,
    "Confirm before printing a message"
  },
  { "print_command", DT_STRING|DT_COMMAND, &C_PrintCommand, IP "lpr", 0, NULL,
    "External command to print a message"
  },
  { "print_decode", DT_BOOL, &C_PrintDecode, true, 0, NULL,
    "Decode message before printing it"
  },
  { "print_decode_weed", DT_BOOL, &C_PrintDecodeWeed, true, 0, NULL,
    "Control whether to weed headers when printing an email "
  },
  { "print_split", DT_BOOL, &C_PrintSplit, false, 0, NULL,
    "Print multiple messages separately"
  },
  { "prompt_after", DT_BOOL, &C_PromptAfter, true, 0, NULL,
    "Pause after running an external pager"
  },
  { "quit", DT_QUAD, &C_Quit, MUTT_YES, 0, NULL,
    "Prompt before exiting NeoMutt"
  },
  { "quote_regex", DT_REGEX|R_PAGER, &C_QuoteRegex, IP "^([ \t]*[|>:}#])+", 0, NULL,
    "Regex to match quoted text in a reply"
  },
  { "read_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_ReadInc, 10, 0, NULL,
    "Update the progress bar after this many records read (0 to disable)"
  },
  { "read_only", DT_BOOL, &C_ReadOnly, false, 0, NULL,
    "Open folders in read-only mode"
  },
  { "realname", DT_STRING|R_INDEX|R_PAGER, &C_Realname, 0, 0, NULL,
    "Real name of the user"
  },
  { "record", DT_STRING|DT_MAILBOX, &C_Record, IP "~/sent", 0, NULL,
    "Folder to save 'sent' messages"
  },
  { "reflow_space_quotes", DT_BOOL, &C_ReflowSpaceQuotes, true, 0, NULL,
    "Insert spaces into reply quotes for 'format=flowed' messages"
  },
  { "reflow_text", DT_BOOL, &C_ReflowText, true, 0, NULL,
    "Reformat paragraphs of 'format=flowed' text"
  },
  { "reflow_wrap", DT_NUMBER, &C_ReflowWrap, 78, 0, NULL,
    "Maximum paragraph width for reformatting 'format=flowed' text"
  },
  { "reply_regex", DT_REGEX|R_INDEX|R_RESORT, &C_ReplyRegex, IP "^((re|aw|sv)(\\[[0-9]+\\])*:[ \t]*)*", 0, reply_validator,
    "Regex to match message reply subjects like 're: '"
  },
  { "resolve", DT_BOOL, &C_Resolve, true, 0, NULL,
    "Move to the next email whenever a command modifies an email"
  },
  { "resume_draft_files", DT_BOOL, &C_ResumeDraftFiles, false, 0, NULL,
    "Process draft files like postponed messages"
  },
  { "resume_edited_draft_files", DT_BOOL, &C_ResumeEditedDraftFiles, true, 0, NULL,
    "Resume editing previously saved draft files"
  },
  { "reverse_alias", DT_BOOL|R_INDEX|R_PAGER, &C_ReverseAlias, false, 0, NULL,
    "Display the alias in the index, rather than the message's sender"
  },
  { "rfc2047_parameters", DT_BOOL, &C_Rfc2047Parameters, false, 0, NULL,
    "Decode RFC2047-encoded MIME parameters"
  },
  { "save_address", DT_BOOL, &C_SaveAddress, false, 0, NULL,
    "Use sender's full address as a default save folder"
  },
  { "save_empty", DT_BOOL, &C_SaveEmpty, true, 0, NULL,
    "(mbox,mmdf) Preserve empty mailboxes"
  },
  { "save_name", DT_BOOL, &C_SaveName, false, 0, NULL,
    "Save outgoing message to mailbox of recipient's name if it exists"
  },
  { "score", DT_BOOL, &C_Score, true, 0, NULL,
    "Use message scoring"
  },
  { "score_threshold_delete", DT_NUMBER, &C_ScoreThresholdDelete, -1, 0, NULL,
    "Messages with a lower score will be automatically deleted"
  },
  { "score_threshold_flag", DT_NUMBER, &C_ScoreThresholdFlag, 9999, 0, NULL,
    "Messages with a greater score will be automatically flagged"
  },
  { "score_threshold_read", DT_NUMBER, &C_ScoreThresholdRead, -1, 0, NULL,
    "Messages with a lower score will be automatically marked read"
  },
  { "search_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_SearchContext, 0, 0, NULL,
    "Context to display around search matches"
  },
  { "send_charset", DT_STRING|DT_CHARSET_STRICT, &C_SendCharset, IP "us-ascii:iso-8859-1:utf-8", 0, charset_validator,
    "Character sets for outgoing mail"
  },
  { "shell", DT_STRING|DT_COMMAND, &C_Shell, IP "/bin/sh", 0, NULL,
    "External command to run subshells in"
  },
  { "show_multipart_alternative", DT_STRING, &C_ShowMultipartAlternative, 0, 0, multipart_validator,
    "How to display 'multipart/alternative' MIME parts"
  },
  { "simple_search", DT_STRING, &C_SimpleSearch, IP "~f %s | ~s %s", 0, NULL,
    "Pattern to search for when search doesn't contain ~'s"
  },
  { "size_show_bytes", DT_BOOL|R_MENU, &C_SizeShowBytes, false, 0, NULL,
    "Show smaller sizes in bytes"
  },
  { "size_show_fractions", DT_BOOL|R_MENU, &C_SizeShowFractions, true, 0, NULL,
    "Show size fractions with a single decimal place"
  },
  { "size_show_mb", DT_BOOL|R_MENU, &C_SizeShowMb, true, 0, NULL,
    "Show sizes in megabytes for sizes greater than 1 megabyte"
  },
  { "size_units_on_left", DT_BOOL|R_MENU, &C_SizeUnitsOnLeft, false, 0, NULL,
    "Show the units as a prefix to the size"
  },
  { "skip_quoted_offset", DT_NUMBER|DT_NOT_NEGATIVE, &C_SkipQuotedOffset, 0, 0, NULL,
    "Lines of context to show when skipping quoted text"
  },
  { "sleep_time", DT_NUMBER|DT_NOT_NEGATIVE, &C_SleepTime, 1, 0, NULL,
    "Time to pause after certain info messages"
  },
  { "smart_wrap", DT_BOOL|R_PAGER_FLOW, &C_SmartWrap, true, 0, NULL,
    "Wrap text at word boundaries"
  },
  { "smileys", DT_REGEX|R_PAGER, &C_Smileys, IP "(>From )|(:[-^]?[][)(><}{|/DP])", 0, NULL,
    "Regex to match smileys to prevent mistakes when quoting text"
  },
  { "sort", DT_SORT|R_INDEX|R_RESORT|DT_SORT_REVERSE, &C_Sort, SORT_DATE, IP SortMethods, pager_validator,
    "Sort method for the index"
  },
  { "sort_aux", DT_SORT|DT_SORT_REVERSE|DT_SORT_LAST|R_INDEX|R_RESORT|R_RESORT_SUB, &C_SortAux, SORT_DATE, IP SortAuxMethods, NULL,
    "Secondary sort method for the index"
  },
  { "sort_browser", DT_SORT|DT_SORT_REVERSE, &C_SortBrowser, SORT_ALPHA, IP SortBrowserMethods, NULL,
    "Sort method for the browser"
  },
  { "sort_re", DT_BOOL|R_INDEX|R_RESORT|R_RESORT_INIT, &C_SortRe, true, 0, pager_validator,
    "Sort method for the sidebar"
  },
  { "spam_separator", DT_STRING, &C_SpamSeparator, IP ",", 0, NULL,
    "Separator for multiple spam headers"
  },
  { "spoolfile", DT_STRING|DT_MAILBOX, &C_Spoolfile, 0, 0, NULL,
    "Inbox"
  },
  { "status_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_StatusChars, IP "-*%A", 0, NULL,
    "Indicator characters for the status bar"
  },
  { "status_format", DT_STRING|R_INDEX|R_PAGER, &C_StatusFormat, IP "-%r-NeoMutt: %D [Msgs:%?M?%M/?%m%?n? New:%n?%?o? Old:%o?%?d? Del:%d?%?F? Flag:%F?%?t? Tag:%t?%?p? Post:%p?%?b? Inc:%b?%?l? %l?]---(%s/%S)-%>-(%P)---", 0, NULL,
    "printf-like format string for the index's status line"
  },
  { "status_on_top", DT_BOOL|R_REFLOW, &C_StatusOnTop, false, 0, NULL,
    "Display the status bar at the top"
  },
  { "strict_threads", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, &C_StrictThreads, false, 0, pager_validator,
    "Thread messages using 'In-Reply-To' and 'References' headers"
  },
  { "suspend", DT_BOOL, &C_Suspend, true, 0, NULL,
    "Allow the user to suspend NeoMutt using '^Z'"
  },
  { "text_flowed", DT_BOOL, &C_TextFlowed, false, 0, NULL,
    "Generate 'format=flowed' messages"
  },
  { "thread_received", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, &C_ThreadReceived, false, 0, pager_validator,
    "Sort threaded messages by their received date"
  },
  { "tilde", DT_BOOL|R_PAGER, &C_Tilde, false, 0, NULL,
    "Character to pad blank lines in the pager"
  },
  { "time_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_TimeInc, 0, 0, NULL,
    "Frequency of progress bar updates (milliseconds)"
  },
  { "timeout", DT_NUMBER|DT_NOT_NEGATIVE, &C_Timeout, 600, 0, NULL,
    "Time to wait for user input in menus"
  },
  { "tmpdir", DT_PATH|DT_PATH_DIR|DT_NOT_EMPTY, &C_Tmpdir, IP "/tmp", 0, NULL,
    "Directory for temporary files"
  },
  { "toggle_quoted_show_levels", DT_NUMBER|DT_NOT_NEGATIVE, &C_ToggleQuotedShowLevels, 0, 0, NULL,
    "Number of quote levels to show with toggle-quoted"
  },
  { "to_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_ToChars, IP " +TCFLR", 0, NULL,
    "Indicator characters for the 'To' field in the index"
  },
  { "trash", DT_STRING|DT_MAILBOX, &C_Trash, 0, 0, NULL,
    "Folder to put deleted emails"
  },
  { "ts_enabled", DT_BOOL|R_INDEX|R_PAGER, &C_TsEnabled, false, 0, NULL,
    "Allow NeoMutt to set the terminal status line and icon"
  },
  { "ts_icon_format", DT_STRING|R_INDEX|R_PAGER, &C_TsIconFormat, IP "M%?n?AIL&ail?", 0, NULL,
    "printf-like format string for the terminal's icon title"
  },
  { "ts_status_format", DT_STRING|R_INDEX|R_PAGER, &C_TsStatusFormat, IP "NeoMutt with %?m?%m messages&no messages?%?n? [%n NEW]?", 0, NULL,
    "printf-like format string for the terminal's status (window title)"
  },
  { "uncollapse_jump", DT_BOOL, &C_UncollapseJump, false, 0, NULL,
    "When opening a thread, jump to the next unread message"
  },
  { "uncollapse_new", DT_BOOL, &C_UncollapseNew, true, 0, NULL,
    "Open collapsed threads when new mail arrives"
  },
  { "use_domain", DT_BOOL, &C_UseDomain, true, 0, NULL,
    "Qualify local addresses using this domain"
  },
  { "visual", DT_STRING|DT_COMMAND, &C_Visual, IP "vi", 0, NULL,
    "Editor to use when '~v' is given in the built-in editor"
  },
  { "wait_key", DT_BOOL, &C_WaitKey, true, 0, NULL,
    "Prompt to press a key after running external commands"
  },
  { "weed", DT_BOOL, &C_Weed, true, 0, NULL,
    "Filter headers when displaying/forwarding/printing/replying"
  },
  { "wrap", DT_NUMBER|R_PAGER_FLOW, &C_Wrap, 0, 0, NULL,
    "Width to wrap text in the pager"
  },
  { "wrap_search", DT_BOOL, &C_WrapSearch, true, 0, NULL,
    "Wrap around when the search hits the end"
  },
  { "write_bcc", DT_BOOL, &C_WriteBcc, false, 0, NULL,
    "Write out the 'Bcc' field when preparing to send a mail"
  },
  { "write_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_WriteInc, 10, 0, NULL,
    "Update the progress bar after this many records written (0 to disable)"
  },

  { "escape", DT_DEPRECATED|DT_STRING, &C_Escape, IP "~" },
  { "ignore_linear_white_space", DT_DEPRECATED|DT_BOOL, &C_IgnoreLinearWhiteSpace, false },

  { "edit_hdrs",        DT_SYNONYM, NULL, IP "edit_headers",     },
  { "forw_decode",      DT_SYNONYM, NULL, IP "forward_decode",   },
  { "forw_quote",       DT_SYNONYM, NULL, IP "forward_quote",    },
  { "hdr_format",       DT_SYNONYM, NULL, IP "index_format",     },
  { "indent_str",       DT_SYNONYM, NULL, IP "indent_string",    },
  { "mime_fwd",         DT_SYNONYM, NULL, IP "mime_forward",     },
  { "msg_format",       DT_SYNONYM, NULL, IP "message_format",   },
  { "print_cmd",        DT_SYNONYM, NULL, IP "print_command",    },
  { "quote_regexp",     DT_SYNONYM, NULL, IP "quote_regex",      },
  { "reply_regexp",     DT_SYNONYM, NULL, IP "reply_regex",      },
  { "xterm_icon",       DT_SYNONYM, NULL, IP "ts_icon_format",   },
  { "xterm_set_titles", DT_SYNONYM, NULL, IP "ts_enabled",       },
  { "xterm_title",      DT_SYNONYM, NULL, IP "ts_status_format", },

  { NULL, 0, NULL, 0, 0, NULL, NULL },
  // clang-format on
};

/**
 * config_init_main - Register main config variables - Implements ::module_init_config_t
 */
static bool config_init_main(struct ConfigSet *cs)
{
  return cs_register_variables(cs, MainVars, 0);
}

/**
 * init_types - Create the config types
 * @param cs Config items
 *
 * Define the config types, e.g. #DT_STRING.
 */
static void init_types(struct ConfigSet *cs)
{
  CONFIG_INIT_TYPE(cs, address);
  CONFIG_INIT_TYPE(cs, bool);
  CONFIG_INIT_TYPE(cs, enum);
  CONFIG_INIT_TYPE(cs, long);
  CONFIG_INIT_TYPE(cs, mbtable);
  CONFIG_INIT_TYPE(cs, number);
  CONFIG_INIT_TYPE(cs, path);
  CONFIG_INIT_TYPE(cs, quad);
  CONFIG_INIT_TYPE(cs, regex);
  CONFIG_INIT_TYPE(cs, slist);
  CONFIG_INIT_TYPE(cs, sort);
  CONFIG_INIT_TYPE(cs, string);
}

/**
 * init_variables - Define the config variables
 * @param cs Config items
 */
static void init_variables(struct ConfigSet *cs)
{
  // Define the config variables
  config_init_main(cs);
  CONFIG_INIT_VARS(cs, alias);
#ifdef USE_AUTOCRYPT
  CONFIG_INIT_VARS(cs, autocrypt);
#endif
  CONFIG_INIT_VARS(cs, compose);
  CONFIG_INIT_VARS(cs, conn);
#ifdef USE_HCACHE
  CONFIG_INIT_VARS(cs, hcache);
#endif
  CONFIG_INIT_VARS(cs, helpbar);
  CONFIG_INIT_VARS(cs, history);
#ifdef USE_IMAP
  CONFIG_INIT_VARS(cs, imap);
#endif
  CONFIG_INIT_VARS(cs, maildir);
  CONFIG_INIT_VARS(cs, mbox);
  CONFIG_INIT_VARS(cs, ncrypt);
#ifdef USE_NNTP
  CONFIG_INIT_VARS(cs, nntp);
#endif
#ifdef USE_NOTMUCH
  CONFIG_INIT_VARS(cs, notmuch);
#endif
  CONFIG_INIT_VARS(cs, pattern);
#ifdef USE_POP
  CONFIG_INIT_VARS(cs, pop);
#endif
  CONFIG_INIT_VARS(cs, send);
#ifdef USE_SIDEBAR
  CONFIG_INIT_VARS(cs, sidebar);
#endif
}

/**
 * init_config - Initialise the config system
 * @param size Size for Config Hash Table
 * @retval ptr New Config Set
 */
struct ConfigSet *init_config(size_t size)
{
  struct ConfigSet *cs = cs_new(size);

  init_types(cs);
  init_variables(cs);

  return cs;
}
