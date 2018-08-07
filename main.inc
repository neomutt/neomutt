/**
 * @mainpage Code Docs
 *
 * ## State of Play
 *
 * This is a Work in Progress.  All the existing comments have been converted
 * to doxygen format and now appear in these pages.
 *
 * Furthermore, all the files, structs, enums and unions have had descriptions
 * written.  Some descriptions are a bit vague, due to ignorance, or poor
 * design decisions.
 *
 * <img style="float: left; padding-right: 0.5em;" src="functions.svg">
 * <img style="float: left; padding-right: 0.5em;" src="structs.svg">
 * <img style="float: left; padding-right: 0.5em;" src="members.svg">
 * <img style="float: left; padding-right: 0.5em;" src="enums.svg">
 * <img style="float: left; padding-right: 0.5em;" src="globals.svg">
 * <img style="float: left; padding-right: 0.5em;" src="config.svg">
 * <br>
 *
 * ## Libraries
 *
 * Each library helps to untangle the code by grouping similar functions and
 * reducing dependencies.
 *
 * The goal is that each library is:
 * - Self-contained (it may rely on other libraries)
 * - Independently testable (i.e. without using NeoMutt)
 * - Fully documented
 * - Robust
 *
 * | Library        |
 * | :------------- |
 * | @ref config    |
 * | @ref conn      |
 * | @ref email     |
 * | @ref hcache    |
 * | @ref imap_imap |
 * | @ref maildir   |
 * | @ref mbox      |
 * | @ref mutt      |
 * | @ref ncrypt    |
 * | @ref nntp      |
 * | @ref notmuch   |
 * | @ref pop       |
 *
 * ## Miscellaneous files
 *
 * These file form the main body of NeoMutt.
 *
 * | File            | Description            |
 * | :-------------- | :--------------------- |
 * | addrbook.c      | @subpage addrbook      |
 * | complete.c      | @subpage complete      |
 * | compress.c      | @subpage compress      |
 * | copy.c          | @subpage copy          |
 * | editmsg.c       | @subpage editmsg       |
 * | enter.c         | @subpage enter         |
 * | filter.c        | @subpage filter        |
 * | flags.c         | @subpage flags         |
 * | hook.c          | @subpage hook          |
 * | main.c          | @subpage main          |
 * | mutt_account.c  | @subpage mutt_account  |
 * | mutt_logging.c  | @subpage mutt_logging  |
 * | mutt_signal.c   | @subpage mutt_signal   |
 * | mutt_socket.c   | @subpage mutt_socket   |
 * | mutt_window.c   | @subpage window        |
 * | mx.c            | @subpage mx            |
 * | postpone.c      | @subpage postpone      |
 * | progress.c      | @subpage progress      |
 * | resize.c        | @subpage resize        |
 * | rfc1524.c       | @subpage rfc1524       |
 * | safe_asprintf.c | @subpage safe_asprintf |
 * | sidebar.c       | @subpage sidebar       |
 * | status.c        | @subpage status        |
 * | system.c        | @subpage system        |
 * | terminal.c      | @subpage terminal      |
 * | version.c       | @subpage version       |
 *
 * ## Building these Docs
 *
 * The config for building the docs is in the main source repo.
 *
 * Everything possible is turned on in the config file, so you'll need to
 * install a few dependencies like `dot` from the graphviz package.
 *
 * Simply: `doxygen doxygen/doxygen.conf` in the main directory.
 *
 * ## Installing the Docs
 *
 * These docs aren't in the main website repo -- they weigh in at 100M
 * Instead, they're stored in the [doxygen repo](https://github.com/neomutt/doxygen)
 * They are included in the website as a "git submodule"
 */
