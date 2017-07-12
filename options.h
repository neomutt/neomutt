/**
 * @file
 * Handling of global boolean variables
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_OPTIONS_H_
#define _MUTT_OPTIONS_H_

/**
 * enum GlobalBool - boolean vars
 */
enum GlobalBool
{
  OPTALLOW8BIT,
  OPTALLOWANSI,
  OPTARROWCURSOR,
  OPTASCIICHARS,
  OPTASKBCC,
  OPTASKCC,
  OPTASKFOLLOWUP,
  OPTASKXCOMMENTTO,
  OPTATTACHSPLIT,
  OPTAUTOEDIT,
  OPTAUTOTAG,
  OPTBEEP,
  OPTBEEPNEW,
  OPTBOUNCEDELIVERED,
  OPTBRAILLEFRIENDLY,
  OPTCHECKMBOXSIZE,
  OPTCHECKNEW,
  OPTCOLLAPSEALL,
  OPTCOLLAPSEUNREAD,
  OPTCOLLAPSEFLAGGED,
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
  OPTFLAGSAFE,
  OPTFOLLOWUPTO,
  OPTFORCENAME,
  OPTFORWDECODE,
  OPTFORWQUOTE,
  OPTFORWREF,
#ifdef USE_HCACHE
  OPTHCACHEVERIFY,
#if defined(HAVE_QDBM) || defined(HAVE_TC) || defined(HAVE_KC)
  OPTHCACHECOMPRESS,
#endif /* HAVE_QDBM */
#endif
  OPTHDRS,
  OPTHEADER,
  OPTHEADERCOLORPARTIAL,
  OPTHELP,
  OPTHIDDENHOST,
  OPTHIDELIMITED,
  OPTHIDEMISSING,
  OPTHIDETHREADSUBJECT,
  OPTHIDETOPLIMITED,
  OPTHIDETOPMISSING,
  OPTHISTREMOVEDUPS,
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
#ifdef USE_SSL
#ifndef USE_SSL_GNUTLS
  OPTSSLSYSTEMCERTS,
  OPTSSLV2,
#endif /* USE_SSL_GNUTLS */
  OPTSSLV3,
  OPTTLSV1,
  OPTTLSV1_1,
  OPTTLSV1_2,
  OPTSSLFORCETLS,
  OPTSSLVERIFYDATES,
  OPTSSLVERIFYHOST,
#if defined(USE_SSL_OPENSSL) && defined(HAVE_SSL_PARTIAL_CHAIN)
  OPTSSLVERIFYPARTIAL,
#endif /* USE_SSL_OPENSSL */
#endif /* defined(USE_SSL) */
  OPTIMPLICITAUTOVIEW,
  OPTINCLUDEONLYFIRST,
  OPTKEEPFLAGGED,
  OPTKEYWORDSLEGACY,
  OPTKEYWORDSSTANDARD,
  OPTMAILCAPSANITIZE,
  OPTMAILCHECKRECENT,
  OPTMAILCHECKSTATS,
  OPTMAILDIRTRASH,
  OPTMAILDIRCHECKCUR,
  OPTMARKERS,
  OPTMARKOLD,
  OPTMENUSCROLL,  /**< scroll menu instead of implicit next-page */
  OPTMENUMOVEOFF, /**< allow menu to scroll past last entry */
#if defined(USE_IMAP) || defined(USE_POP)
  OPTMESSAGECACHECLEAN,
#endif
  OPTMETAKEY, /**< interpret ALT-x as ESC-x */
  OPTMETOO,
  OPTMHPURGE,
  OPTMIMEFORWDECODE,
#ifdef USE_NNTP
  OPTMIMESUBJECT, /**< encode subject line with RFC2047 */
#endif
  OPTNARROWTREE,
  OPTPAGERSTOP,
  OPTPIPEDECODE,
  OPTPIPESPLIT,
#ifdef USE_POP
  OPTPOPAUTHTRYALL,
  OPTPOPLAST,
#endif
  OPTPOSTPONEENCRYPT,
  OPTPRINTDECODE,
  OPTPRINTSPLIT,
  OPTPROMPTAFTER,
  OPTREADONLY,
  OPTREFLOWSPACEQUOTES,
  OPTREFLOWTEXT,
  OPTREPLYSELF,
  OPTREPLYWITHXORIG,
  OPTRESOLVE,
  OPTRESUMEDRAFTFILES,
  OPTRESUMEEDITEDDRAFTFILES,
  OPTREVALIAS,
  OPTREVNAME,
  OPTREVREAL,
  OPTRFC2047PARAMS,
  OPTSAVEADDRESS,
  OPTSAVEEMPTY,
  OPTSAVENAME,
  OPTSCORE,
#ifdef USE_SIDEBAR
  OPTSIDEBAR,
  OPTSIDEBARFOLDERINDENT,
  OPTSIDEBARNEWMAILONLY,
  OPTSIDEBARNEXTNEWWRAP,
  OPTSIDEBARSHORTPATH,
  OPTSIDEBARONRIGHT,
#endif
  OPTSIGDASHES,
  OPTSIGONTOP,
  OPTSORTRE,
  OPTSTATUSONTOP,
  OPTSTRICTTHREADS,
  OPTSUSPEND,
  OPTTEXTFLOWED,
  OPTTHOROUGHSRC,
  OPTTHREADRECEIVED,
  OPTTILDE,
  OPTTSENABLED,
  OPTUNCOLLAPSEJUMP,
  OPTUNCOLLAPSENEW,
  OPTUSE8BITMIME,
  OPTUSEDOMAIN,
  OPTUSEFROM,
  OPTUSEGPGAGENT,
#ifdef HAVE_LIBIDN
  OPTIDNDECODE,
  OPTIDNENCODE,
#endif
#ifdef HAVE_GETADDRINFO
  OPTUSEIPV6,
#endif
  OPTWAITKEY,
  OPTWEED,
  OPTWRAP,
  OPTWRAPSEARCH,
  OPTWRITEBCC, /**< write out a bcc header? */
  OPTXMAILER,

  OPTCRYPTUSEGPGME,
  OPTCRYPTUSEPKA,

  /* PGP options */

  OPTCRYPTAUTOSIGN,
  OPTCRYPTAUTOENCRYPT,
  OPTCRYPTAUTOPGP,
  OPTCRYPTAUTOSMIME,
  OPTCRYPTCONFIRMHOOK,
  OPTCRYPTOPPORTUNISTICENCRYPT,
  OPTCRYPTREPLYENCRYPT,
  OPTCRYPTREPLYSIGN,
  OPTCRYPTREPLYSIGNENCRYPTED,
  OPTCRYPTTIMESTAMP,
  OPTSMIMEISDEFAULT,
  OPTSMIMESELFENCRYPT,
  OPTASKCERTLABEL,
  OPTSDEFAULTDECRYPTKEY,
  OPTPGPIGNORESUB,
  OPTPGPCHECKEXIT,
  OPTPGPLONGIDS,
  OPTPGPAUTODEC,
  OPTPGPRETAINABLESIG,
  OPTPGPSELFENCRYPT,
  OPTPGPSTRICTENC,
  OPTFORWDECRYPT,
  OPTPGPSHOWUNUSABLE,
  OPTPGPAUTOINLINE,
  OPTPGPREPLYINLINE,

/* news options */

#ifdef USE_NNTP
  OPTSHOWNEWNEWS,
  OPTSHOWONLYUNREAD,
  OPTSAVEUNSUB,
  OPTLISTGROUP,
  OPTLOADDESC,
  OPTXCOMMENTTO,
#endif

  /* pseudo options */

  OPTAUXSORT,           /**< (pseudo) using auxiliary sort function */
  OPTFORCEREFRESH,      /**< (pseudo) refresh even during macros */
  OPTLOCALES,           /**< (pseudo) set if user has valid locale definition */
  OPTNOCURSES,          /**< (pseudo) when sending in batch mode */
  OPTSEARCHREVERSE,     /**< (pseudo) used by ci_search_command */
  OPTMSGERR,            /**< (pseudo) used by mutt_error/mutt_message */
  OPTSEARCHINVALID,     /**< (pseudo) used to invalidate the search pat */
  OPTSIGNALSBLOCKED,    /**< (pseudo) using by mutt_block_signals () */
  OPTSYSSIGNALSBLOCKED, /**< (pseudo) using by mutt_block_signals_system () */
  OPTNEEDRESORT,        /**< (pseudo) used to force a re-sort */
  OPTRESORTINIT,        /**< (pseudo) used to force the next resort to be from scratch */
  OPTVIEWATTACH,        /**< (pseudo) signals that we are viewing attachments */
  OPTSORTSUBTHREADS,    /**< (pseudo) used when $sort_aux changes */
  OPTNEEDRESCORE,       /**< (pseudo) set when the `score' command is used */
  OPTATTACHMSG,         /**< (pseudo) used by attach-message */
  OPTHIDEREAD,          /**< (pseudo) whether or not hide read messages */
  OPTKEEPQUIET,         /**< (pseudo) shut up the message and refresh
                         *            functions while we are executing an
                         *            external program.  */
  OPTMENUCALLER,        /**< (pseudo) tell menu to give caller a take */
  OPTREDRAWTREE,        /**< (pseudo) redraw the thread tree */
  OPTPGPCHECKTRUST,     /**< (pseudo) used by pgp_select_key () */
  OPTDONTHANDLEPGPKEYS, /**< (pseudo) used to extract PGP keys */
  OPTIGNOREMACROEVENTS, /**< (pseudo) don't process macro/push/exec events while set */

#ifdef USE_NNTP
  OPTNEWS,              /**< (pseudo) used to change reader mode */
  OPTNEWSSEND,          /**< (pseudo) used to change behavior when posting */
#endif
#ifdef USE_NOTMUCH
  OPTVIRTSPOOLFILE,
  OPTNOTMUCHRECORD,
#endif

  OPTMAX
};

#define mutt_bit_set(v, n)    v[n / 8] |= (1 << (n % 8))
#define mutt_bit_unset(v, n)  v[n / 8] &= ~(1 << (n % 8))
#define mutt_bit_toggle(v, n) v[n / 8] ^= (1 << (n % 8))
#define mutt_bit_isset(v, n)  (v[n / 8] & (1 << (n % 8)))

/* bit vector for boolean variables */
#ifdef MAIN_C
unsigned char Options[(OPTMAX + 7) / 8];
#else
extern unsigned char Options[];
#endif

#define set_option(x) mutt_bit_set(Options, x)
#define unset_option(x) mutt_bit_unset(Options, x)
#define toggle_option(x) mutt_bit_toggle(Options, x)
#define option(x) mutt_bit_isset(Options, x)

#endif /* _MUTT_OPTIONS_H_ */
