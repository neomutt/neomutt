
/* program to use for shell commands */
#define EXECSHELL "/bin/sh"

/* Define to `int' if <signal.h> doesn't define.  */
#undef sig_atomic_t

@BOTTOM@
/* Define if you have start_color, as a function or macro.  */
#undef HAVE_START_COLOR

/* Define if you have typeahead, as a function or macro.  */
#undef HAVE_TYPEAHEAD

/* Define if you have bkgdset, as a function or macro.  */
#undef HAVE_BKGDSET

/* Define if you have curs_set, as a function or macro.  */
#undef HAVE_CURS_SET

/* Define if you have meta, as a function or macro.  */
#undef HAVE_META

/* Define if you have use_default_colors, as a function or macro.  */
#undef HAVE_USE_DEFAULT_COLORS

/* Define if you have resizeterm, as a function or macro.  */
#undef HAVE_RESIZETERM

/* Some systems declare sig_atomic_t as volatile, some others -- no.
 * This define will have value `sig_atomic_t' or `volatile sig_atomic_t'
 * accordingly. */
#undef SIG_ATOMIC_VOLATILE_T

/* Define as 1 if iconv() only converts exactly and we should treat
 * all return values other than (size_t)(-1) as equivalent. */
#undef ICONV_NONTRANS

