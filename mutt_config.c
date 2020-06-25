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
 * @page mutt_config Definitions of config variables
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
#include "browser.h"
#include "commands.h"
#include "compose.h"
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
#include "pattern.h"
#include "progress.h"
#include "recvattach.h"
#include "recvcmd.h"
#include "remailer.h"
#include "rfc3676.h"
#include "score.h"
#include "sort.h"
#include "status.h"
#include "bcache/lib.h"
#include "maildir/lib.h"
#include "ncrypt/lib.h"

#ifndef ISPELL
#define ISPELL "ispell"
#endif

/* These options are deprecated */
char *C_Escape = NULL;
bool C_IgnoreLinearWhiteSpace = false;

// clang-format off
struct ConfigDef MainVars[] = {
  { "abort_backspace", DT_BOOL, &C_AbortBackspace, true },
  { "abort_key", DT_STRING|DT_NOT_EMPTY, &C_AbortKey, IP "\007" },
  { "alias_file", DT_PATH|DT_PATH_FILE, &C_AliasFile, IP "~/.neomuttrc" },
  { "alias_format", DT_STRING|DT_NOT_EMPTY, &C_AliasFormat, IP "%3n %f%t %-15a %-56r | %c" },
  { "allow_ansi", DT_BOOL, &C_AllowAnsi, false },
  { "arrow_cursor", DT_BOOL|R_MENU, &C_ArrowCursor, false },
  { "arrow_string", DT_STRING|DT_NOT_EMPTY, &C_ArrowString, IP "->" },
  { "ascii_chars", DT_BOOL|R_INDEX|R_PAGER, &C_AsciiChars, false },
  { "askbcc", DT_BOOL, &C_Askbcc, false },
  { "askcc", DT_BOOL, &C_Askcc, false },
  { "assumed_charset", DT_STRING, &C_AssumedCharset, 0, 0, charset_validator },
  { "attach_format", DT_STRING|DT_NOT_EMPTY, &C_AttachFormat, IP "%u%D%I %t%4n %T%.40d%> [%.7m/%.10M, %.6e%?C?, %C?, %s] " },
  { "attach_save_dir", DT_PATH|DT_PATH_DIR, &C_AttachSaveDir, IP "./" },
  { "attach_save_without_prompting", DT_BOOL, &C_AttachSaveWithoutPrompting, false },
  { "attach_sep", DT_STRING, &C_AttachSep, IP "\n" },
  { "attach_split", DT_BOOL, &C_AttachSplit, true },
  { "attribution", DT_STRING, &C_Attribution, IP "On %d, %n wrote:" },
  { "attribution_locale", DT_STRING, &C_AttributionLocale, 0 },
  { "auto_subscribe", DT_BOOL, &C_AutoSubscribe, false },
  { "auto_tag", DT_BOOL, &C_AutoTag, false },
  { "autoedit", DT_BOOL, &C_Autoedit, false },
  { "beep", DT_BOOL, &C_Beep, true },
  { "beep_new", DT_BOOL, &C_BeepNew, false },
  { "bounce", DT_QUAD, &C_Bounce, MUTT_ASKYES },
  { "braille_friendly", DT_BOOL, &C_BrailleFriendly, false },
  { "browser_abbreviate_mailboxes", DT_BOOL, &C_BrowserAbbreviateMailboxes, true },
  { "change_folder_next", DT_BOOL, &C_ChangeFolderNext, false },
  { "charset", DT_STRING|DT_NOT_EMPTY, &C_Charset, 0, 0, charset_validator },
  { "collapse_all", DT_BOOL, &C_CollapseAll, false },
  { "collapse_flagged", DT_BOOL, &C_CollapseFlagged, true },
  { "collapse_unread", DT_BOOL, &C_CollapseUnread, true },
  { "compose_format", DT_STRING|R_MENU, &C_ComposeFormat, IP "-- NeoMutt: Compose  [Approx. msg size: %l   Atts: %a]%>-" },
  { "config_charset", DT_STRING, &C_ConfigCharset, 0, 0, charset_validator },
  { "confirmappend", DT_BOOL, &C_Confirmappend, true },
  { "confirmcreate", DT_BOOL, &C_Confirmcreate, true },
  { "copy", DT_QUAD, &C_Copy, MUTT_YES },
  { "copy_decode_weed", DT_BOOL, &C_CopyDecodeWeed, false },
  { "crypt_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_CryptChars, IP "SPsK " },
  { "date_format", DT_STRING|DT_NOT_EMPTY|R_MENU, &C_DateFormat, IP "!%a, %b %d, %Y at %I:%M:%S%p %Z" },
  { "debug_file", DT_PATH|DT_PATH_FILE, &C_DebugFile, IP "~/.neomuttdebug" },
  { "debug_level", DT_NUMBER, &C_DebugLevel, 0, 0, level_validator },
  { "default_hook", DT_STRING, &C_DefaultHook, IP "~f %s !~P | (~P ~C %s)" },
  { "delete", DT_QUAD, &C_Delete, MUTT_ASKYES },
  { "delete_untag", DT_BOOL, &C_DeleteUntag, true },
  { "digest_collapse", DT_BOOL, &C_DigestCollapse, true },
  { "display_filter", DT_STRING|DT_COMMAND|R_PAGER, &C_DisplayFilter, 0 },
  { "duplicate_threads", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, &C_DuplicateThreads, true, 0, pager_validator },
  { "edit_headers", DT_BOOL, &C_EditHeaders, false },
  { "editor", DT_STRING|DT_NOT_EMPTY|DT_COMMAND, &C_Editor, IP "vi" },
  { "external_search_command", DT_STRING|DT_COMMAND, &C_ExternalSearchCommand, 0 },
  { "flag_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_FlagChars, IP "*!DdrONon- " },
  { "flag_safe", DT_BOOL, &C_FlagSafe, false },
  { "folder", DT_STRING|DT_MAILBOX, &C_Folder, IP "~/Mail" },
  { "folder_format", DT_STRING|DT_NOT_EMPTY|R_MENU, &C_FolderFormat, IP "%2C %t %N %F %2l %-8.8u %-8.8g %8s %d %i" },
  { "force_name", DT_BOOL, &C_ForceName, false },
  { "forward_attachments", DT_QUAD, &C_ForwardAttachments, MUTT_ASKYES },
  { "forward_decode", DT_BOOL, &C_ForwardDecode, true },
  { "forward_quote", DT_BOOL, &C_ForwardQuote, false },
  { "from", DT_ADDRESS, &C_From, 0 },
  { "from_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_FromChars, 0 },
  { "gecos_mask", DT_REGEX, &C_GecosMask, IP "^[^,]*" },
  { "header", DT_BOOL, &C_Header, false },
  { "header_color_partial", DT_BOOL|R_PAGER_FLOW, &C_HeaderColorPartial, false },
  { "help", DT_BOOL|R_REFLOW, &C_Help, true },
  { "hidden_tags", DT_SLIST|SLIST_SEP_COMMA, &C_HiddenTags, IP "unread,draft,flagged,passed,replied,attachment,signed,encrypted" },
  { "hide_limited", DT_BOOL|R_TREE|R_INDEX, &C_HideLimited, false },
  { "hide_missing", DT_BOOL|R_TREE|R_INDEX, &C_HideMissing, true },
  { "hide_thread_subject", DT_BOOL|R_TREE|R_INDEX, &C_HideThreadSubject, true },
  { "hide_top_limited", DT_BOOL|R_TREE|R_INDEX, &C_HideTopLimited, false },
  { "hide_top_missing", DT_BOOL|R_TREE|R_INDEX, &C_HideTopMissing, true },
  { "honor_disposition", DT_BOOL, &C_HonorDisposition, false },
  { "hostname", DT_STRING, &C_Hostname, 0 },
#ifdef HAVE_LIBIDN
  { "idn_decode", DT_BOOL|R_MENU, &C_IdnDecode, true },
  { "idn_encode", DT_BOOL|R_MENU, &C_IdnEncode, true },
#endif
  { "implicit_autoview", DT_BOOL, &C_ImplicitAutoview, false },
  { "include_encrypted", DT_BOOL, &C_IncludeEncrypted, false },
  { "include_onlyfirst", DT_BOOL, &C_IncludeOnlyfirst, false },
  { "indent_string", DT_STRING, &C_IndentString, IP "> " },
  { "index_format", DT_STRING|DT_NOT_EMPTY|R_INDEX|R_PAGER, &C_IndexFormat, IP "%4C %Z %{%b %d} %-15.15L (%?l?%4l&%4c?) %s" },
  { "ispell", DT_STRING|DT_COMMAND, &C_Ispell, IP ISPELL },
  { "keep_flagged", DT_BOOL, &C_KeepFlagged, false },
  { "mail_check", DT_NUMBER|DT_NOT_NEGATIVE, &C_MailCheck, 5 },
  { "mail_check_recent", DT_BOOL, &C_MailCheckRecent, true },
  { "mail_check_stats", DT_BOOL, &C_MailCheckStats, false },
  { "mail_check_stats_interval", DT_NUMBER|DT_NOT_NEGATIVE, &C_MailCheckStatsInterval, 60 },
  { "mailcap_path", DT_SLIST|SLIST_SEP_COLON, &C_MailcapPath, IP "~/.mailcap:" PKGDATADIR "/mailcap:" SYSCONFDIR "/mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap" },
  { "mailcap_sanitize", DT_BOOL, &C_MailcapSanitize, true },
  { "mark_macro_prefix", DT_STRING, &C_MarkMacroPrefix, IP "'" },
  { "mark_old", DT_BOOL|R_INDEX|R_PAGER, &C_MarkOld, true },
  { "markers", DT_BOOL|R_PAGER_FLOW, &C_Markers, true },
  { "mask", DT_REGEX|DT_REGEX_MATCH_CASE|DT_REGEX_ALLOW_NOT|DT_REGEX_NOSUB, &C_Mask, IP "!^\\.[^.]" },
  { "mbox", DT_STRING|DT_MAILBOX|R_INDEX|R_PAGER, &C_Mbox, IP "~/mbox" },
  { "mbox_type", DT_ENUM, &C_MboxType, MUTT_MBOX, IP &MboxTypeDef },
  { "menu_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_MenuContext, 0 },
  { "menu_move_off", DT_BOOL, &C_MenuMoveOff, true },
  { "menu_scroll", DT_BOOL, &C_MenuScroll, false },
  { "message_cache_clean", DT_BOOL, &C_MessageCacheClean, false },
  { "message_cachedir", DT_PATH|DT_PATH_DIR, &C_MessageCachedir, 0 },
  { "message_format", DT_STRING|DT_NOT_EMPTY, &C_MessageFormat, IP "%s" },
  { "meta_key", DT_BOOL, &C_MetaKey, false },
  { "mime_forward", DT_QUAD, &C_MimeForward, MUTT_NO },
  { "mime_forward_rest", DT_QUAD, &C_MimeForwardRest, MUTT_YES },
#ifdef MIXMASTER
  { "mix_entry_format", DT_STRING|DT_NOT_EMPTY, &C_MixEntryFormat, IP "%4n %c %-16s %a" },
  { "mixmaster", DT_STRING|DT_COMMAND, &C_Mixmaster, IP MIXMASTER },
#endif
  { "move", DT_QUAD, &C_Move, MUTT_NO },
  { "narrow_tree", DT_BOOL|R_TREE|R_INDEX, &C_NarrowTree, false },
  { "net_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_NetInc, 10 },
  { "new_mail_command", DT_STRING|DT_COMMAND, &C_NewMailCommand, 0 },
  { "pager", DT_STRING|DT_COMMAND, &C_Pager, IP "builtin" },
  { "pager_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_PagerContext, 0 },
  { "pager_format", DT_STRING|R_PAGER, &C_PagerFormat, IP "-%Z- %C/%m: %-20.20n   %s%*  -- (%P)" },
  { "pager_index_lines", DT_NUMBER|DT_NOT_NEGATIVE|R_PAGER|R_REFLOW, &C_PagerIndexLines, 0 },
  { "pager_stop", DT_BOOL, &C_PagerStop, false },
  { "pattern_format", DT_STRING, &C_PatternFormat, IP "%2n %-15e  %d" },
  { "pipe_decode", DT_BOOL, &C_PipeDecode, false },
  { "pipe_decode_weed", DT_BOOL, &C_PipeDecodeWeed, true },
  { "pipe_sep", DT_STRING, &C_PipeSep, IP "\n" },
  { "pipe_split", DT_BOOL, &C_PipeSplit, false },
  { "postpone", DT_QUAD, &C_Postpone, MUTT_ASKYES },
  { "postponed", DT_STRING|DT_MAILBOX|R_INDEX, &C_Postponed, IP "~/postponed" },
  { "preferred_languages", DT_SLIST|SLIST_SEP_COMMA, &C_PreferredLanguages, 0 },
  { "print", DT_QUAD, &C_Print, MUTT_ASKNO },
  { "print_command", DT_STRING|DT_COMMAND, &C_PrintCommand, IP "lpr" },
  { "print_decode", DT_BOOL, &C_PrintDecode, true },
  { "print_decode_weed", DT_BOOL, &C_PrintDecodeWeed, true },
  { "print_split", DT_BOOL, &C_PrintSplit, false },
  { "prompt_after", DT_BOOL, &C_PromptAfter, true },
  { "query_command", DT_STRING|DT_COMMAND, &C_QueryCommand, 0 },
  { "query_format", DT_STRING|DT_NOT_EMPTY, &C_QueryFormat, IP "%3c %t %-25.25n %-25.25a | %e" },
  { "quit", DT_QUAD, &C_Quit, MUTT_YES },
  { "quote_regex", DT_REGEX|R_PAGER, &C_QuoteRegex, IP "^([ \t]*[|>:}#])+" },
  { "read_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_ReadInc, 10 },
  { "read_only", DT_BOOL, &C_ReadOnly, false },
  { "realname", DT_STRING|R_INDEX|R_PAGER, &C_Realname, 0 },
  { "record", DT_STRING|DT_MAILBOX, &C_Record, IP "~/sent" },
  { "reflow_space_quotes", DT_BOOL, &C_ReflowSpaceQuotes, true },
  { "reflow_text", DT_BOOL, &C_ReflowText, true },
  { "reflow_wrap", DT_NUMBER, &C_ReflowWrap, 78 },
  { "reply_regex", DT_REGEX|R_INDEX|R_RESORT, &C_ReplyRegex, IP "^((re|aw|sv)(\\[[0-9]+\\])*:[ \t]*)*", 0, reply_validator },
  { "resolve", DT_BOOL, &C_Resolve, true },
  { "resume_draft_files", DT_BOOL, &C_ResumeDraftFiles, false },
  { "resume_edited_draft_files", DT_BOOL, &C_ResumeEditedDraftFiles, true },
  { "reverse_alias", DT_BOOL|R_INDEX|R_PAGER, &C_ReverseAlias, false },
  { "rfc2047_parameters", DT_BOOL, &C_Rfc2047Parameters, false },
  { "save_address", DT_BOOL, &C_SaveAddress, false },
  { "save_empty", DT_BOOL, &C_SaveEmpty, true },
  { "save_name", DT_BOOL, &C_SaveName, false },
  { "score", DT_BOOL, &C_Score, true },
  { "score_threshold_delete", DT_NUMBER, &C_ScoreThresholdDelete, -1 },
  { "score_threshold_flag", DT_NUMBER, &C_ScoreThresholdFlag, 9999 },
  { "score_threshold_read", DT_NUMBER, &C_ScoreThresholdRead, -1 },
  { "search_context", DT_NUMBER|DT_NOT_NEGATIVE, &C_SearchContext, 0 },
  { "send_charset", DT_STRING, &C_SendCharset, IP "us-ascii:iso-8859-1:utf-8", 0, charset_validator },
  { "shell", DT_STRING|DT_COMMAND, &C_Shell, IP "/bin/sh" },
  { "show_multipart_alternative", DT_STRING, &C_ShowMultipartAlternative, 0, 0, multipart_validator },
  { "simple_search", DT_STRING, &C_SimpleSearch, IP "~f %s | ~s %s" },
  { "size_show_bytes", DT_BOOL|R_MENU, &C_SizeShowBytes, false },
  { "size_show_fractions", DT_BOOL|R_MENU, &C_SizeShowFractions, true },
  { "size_show_mb", DT_BOOL|R_MENU, &C_SizeShowMb, true },
  { "size_units_on_left", DT_BOOL|R_MENU, &C_SizeUnitsOnLeft, false },
  { "skip_quoted_offset", DT_NUMBER|DT_NOT_NEGATIVE, &C_SkipQuotedOffset, 0 },
  { "sleep_time", DT_NUMBER|DT_NOT_NEGATIVE, &C_SleepTime, 1 },
  { "smart_wrap", DT_BOOL|R_PAGER_FLOW, &C_SmartWrap, true },
  { "smileys", DT_REGEX|R_PAGER, &C_Smileys, IP "(>From )|(:[-^]?[][)(><}{|/DP])" },
  { "sort", DT_SORT|R_INDEX|R_RESORT, &C_Sort, SORT_DATE, 0, pager_validator },
  { "sort_alias", DT_SORT|DT_SORT_ALIAS, &C_SortAlias, SORT_ALIAS },
  { "sort_aux", DT_SORT|DT_SORT_AUX|R_INDEX|R_RESORT|R_RESORT_SUB, &C_SortAux, SORT_DATE },
  { "sort_browser", DT_SORT|DT_SORT_BROWSER, &C_SortBrowser, SORT_ALPHA },
  { "sort_re", DT_BOOL|R_INDEX|R_RESORT|R_RESORT_INIT, &C_SortRe, true, 0, pager_validator },
  { "spam_separator", DT_STRING, &C_SpamSeparator, IP "," },
  { "spoolfile", DT_STRING|DT_MAILBOX, &C_Spoolfile, 0 },
  { "status_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_StatusChars, IP "-*%A" },
  { "status_format", DT_STRING|R_INDEX|R_PAGER, &C_StatusFormat, IP "-%r-NeoMutt: %D [Msgs:%?M?%M/?%m%?n? New:%n?%?o? Old:%o?%?d? Del:%d?%?F? Flag:%F?%?t? Tag:%t?%?p? Post:%p?%?b? Inc:%b?%?l? %l?]---(%s/%S)-%>-(%P)---" },
  { "status_on_top", DT_BOOL|R_REFLOW, &C_StatusOnTop, false },
  { "strict_threads", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, &C_StrictThreads, false, 0, pager_validator },
  { "suspend", DT_BOOL, &C_Suspend, true },
  { "text_flowed", DT_BOOL, &C_TextFlowed, false },
  { "thorough_search", DT_BOOL, &C_ThoroughSearch, true },
  { "thread_received", DT_BOOL|R_RESORT|R_RESORT_INIT|R_INDEX, &C_ThreadReceived, false, 0, pager_validator },
  { "tilde", DT_BOOL|R_PAGER, &C_Tilde, false },
  { "time_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_TimeInc, 0 },
  { "timeout", DT_NUMBER|DT_NOT_NEGATIVE, &C_Timeout, 600 },
  { "tmpdir", DT_PATH|DT_PATH_DIR|DT_NOT_EMPTY, &C_Tmpdir, IP "/tmp" },
  { "toggle_quoted_show_levels", DT_NUMBER|DT_NOT_NEGATIVE, &C_ToggleQuotedShowLevels, 0 },
  { "to_chars", DT_MBTABLE|R_INDEX|R_PAGER, &C_ToChars, IP " +TCFLR" },
  { "trash", DT_STRING|DT_MAILBOX, &C_Trash, 0 },
  { "ts_enabled", DT_BOOL|R_INDEX|R_PAGER, &C_TsEnabled, false },
  { "ts_icon_format", DT_STRING|R_INDEX|R_PAGER, &C_TsIconFormat, IP "M%?n?AIL&ail?" },
  { "ts_status_format", DT_STRING|R_INDEX|R_PAGER, &C_TsStatusFormat, IP "NeoMutt with %?m?%m messages&no messages?%?n? [%n NEW]?" },
  { "uncollapse_jump", DT_BOOL, &C_UncollapseJump, false },
  { "uncollapse_new", DT_BOOL, &C_UncollapseNew, true },
  { "use_domain", DT_BOOL, &C_UseDomain, true },
  { "visual", DT_STRING|DT_COMMAND, &C_Visual, IP "vi" },
  { "wait_key", DT_BOOL, &C_WaitKey, true },
  { "weed", DT_BOOL, &C_Weed, true },
  { "wrap", DT_NUMBER|R_PAGER_FLOW, &C_Wrap, 0 },
  { "wrap_search", DT_BOOL, &C_WrapSearch, true },
  { "write_bcc", DT_BOOL, &C_WriteBcc, false },
  { "write_inc", DT_NUMBER|DT_NOT_NEGATIVE, &C_WriteInc, 10 },
  { "escape", DT_DEPRECATED|DT_STRING, &C_Escape, IP "~" },

  { "ignore_linear_white_space", DT_DEPRECATED|DT_BOOL,            &C_IgnoreLinearWhiteSpace, false   },

  { "edit_hdrs",              DT_SYNONYM, NULL, IP "edit_headers",             },
  { "forw_decode",            DT_SYNONYM, NULL, IP "forward_decode",           },
  { "forw_quote",             DT_SYNONYM, NULL, IP "forward_quote",            },
  { "hdr_format",             DT_SYNONYM, NULL, IP "index_format",             },
  { "indent_str",             DT_SYNONYM, NULL, IP "indent_string",            },
  { "mime_fwd",               DT_SYNONYM, NULL, IP "mime_forward",             },
  { "msg_format",             DT_SYNONYM, NULL, IP "message_format",           },
  { "print_cmd",              DT_SYNONYM, NULL, IP "print_command",            },
  { "quote_regexp",           DT_SYNONYM, NULL, IP "quote_regex",              },
  { "reply_regexp",           DT_SYNONYM, NULL, IP "reply_regex",              },
  { "xterm_icon",             DT_SYNONYM, NULL, IP "ts_icon_format",           },
  { "xterm_set_titles",       DT_SYNONYM, NULL, IP "ts_enabled",               },
  { "xterm_title",            DT_SYNONYM, NULL, IP "ts_status_format",         },

  { NULL, 0, NULL, 0, 0, NULL },
};
// clang-format on

/**
 * config_init_main - Register main config variables
 */
bool config_init_main(struct ConfigSet *cs)
{
  return cs_register_variables(cs, MainVars, 0);
}
