---
author: flatcap
title: Release 2025-04-04
---

## :book: Notes

This is (mostly) a Bug-Fix Release.

Most of other changes are improving the startup/help, e.g.

- `neomutt -h       # Clearer help`
- `neomutt -h send  # Specific help with examples`
- `neomutt -DD      # Dump different (changed) config`

## :gem: Sponsors

Special thanks to our **sponsors**:

<table>
  <tr>
    <td align="center"><a href="https://github.com/jindraj/"><img width="80" src="https://avatars.githubusercontent.com/u/1755070"></a></td>
    <td align="center"><a href="https://github.com/scottkosty/"><img width="80" src="https://avatars.githubusercontent.com/u/1149353"></a></td>
    <td align="center"><a href="https://github.com/igor47/"><img width="80" src="https://avatars.githubusercontent.com/u/200575"></a></td>
    <td align="center"><a href="https://github.com/kmARC/"><img width="80" src="https://avatars.githubusercontent.com/u/6640417"></a></td>
    <td align="left" colspan="2"><a href="https://www.blunix.com/"><img width="80" src="https://neomutt.org/images/sponsors/blunix.png"></a></td>
  </tr>
  <tr>
    <td>Jakub&nbsp;Jindra<br>@jindraj</td>
    <td>Scott&nbsp;Kostyshak<br>@scottkosty</td>
    <td>Igor&nbsp;Serebryany<br>@igor47</td>
    <td>Mark&nbsp;Korondi<br>@kmARC</td>
    <td colspan="2">Blunix&nbsp;GmbH<br><a href="https://www.blunix.com/">Linux support company<br>from Berlin, Germany</a></td>
  </tr>
  <tr>
    <td align="center"><a href="https://github.com/bittorf"><img width="80" src="https://avatars.githubusercontent.com/u/198379"></a></td>
    <td align="center"><a href="https://github.com/nicoe"><img width="80" src="https://avatars.githubusercontent.com/u/44782"></a></td>
    <td align="center"><a href="https://github.com/Yutsuten"><img width="80" src="https://avatars.githubusercontent.com/u/7322925"></a></td>
    <td align="center"><a href="https://github.com/ricci"><img width="80" src="https://avatars.githubusercontent.com/u/829847"></a></td>
    <td align="left" colspan="2"><a href="https://github.com/terminaldweller"><img width="80" src="https://avatars.githubusercontent.com/u/20871975"></a></td>
  </tr>
  <tr>
    <td>Bastian&nbsp;Bittorf<br>@bittorf</td>
    <td>Nicolas&nbsp;Évrard<br>@nicoe</td>
    <td>Mateus&nbsp;Etto<br>@Yutsuten</td>
    <td>Robert Ricci<br>@ricci</td>
    <td>Farzad Sadeghi<br>@terminaldweller</td>
  </tr>
</table>

- Robert Labudda
- Patrick Koetter ([@patrickbenkoetter@troet.cafe](https://troet.cafe/@patrickbenkoetter))
- Reiko Kaps
- Morgan Kelly
- Izaac Mammadov (@IzaacMammadov)
- Anon (BitCoin)

[Become a sponsor of NeoMutt](https://neomutt.org/sponsor)

## :heart: Thanks

Many thanks to our **new contributors**:

- @wbob
- @Leif-W
- Michael J Gruber (@mjg)

and our **regular contributors**:

- Chao-Kuei Hung (@ckhung)
- Pietro Cerutti (@gahr)
- Rayford Shireman (@rayfordshire)
- Gerrit Rüsing (@kbcb)
- Ian Zimmerman (@nobrowser)
- Ryan d'Huart (@homoelectromagneticus)
- Dennis Schön (@roccoblues)
- Emir Sari (@bitigchi)
- Mateus Etto (@yutsuten)
- Marius Gedminas (@mgedmin)

## :gift: Features

- #4493 - config: don't quote enums
- #4493 - link config dump to docs
- #4494 - refactor the Help Page for clarity
- #4554 - CLI: `neomutt -DD` -- Dump Different
- #4593 - browser: tag select rather than descend

## :beetle: Bug Fixes

- #3469 - source: fix variable interpretation
- #4370 - `mutt_oauth2`: refactor `sasl_string` computation
- #4536 - expand tabs to spaces in compose preview
- #4537 - fix dumping of initial values
- #4538 - move `real_name` init
- #4542 - Remove `MUTT_NEWFOLDER`, fix appending to mbox
- #4546 - Respect Ignore when modifying an email's headers
- #4549 - fix refresh on toggle `hide_thread_subject`
- #4550 - buffer: fix seek
- #4551 - add comma to single `<complete-query>` match
- #4595 - notmuch: check for parse failure
- #4596 - query: allow `<>`s around email addresses
- pager: fix normal/stripe colour
- fix colour leaks in pager
- fix array leak in the verify certificate dialog

## :black_flag: Translations

- 100% :de: German
- 100% :tr: Turkish
- 96% :lithuania: Lithuanian
- 86% :fr: French
- 49% :taiwan: Chinese (Traditional)

## :building_construction: Build

- #4552 - Deprecate some configure options that aren't used anymore
- build: workaround for unused-result warning

## :gear: Code

- #4492 - colour refactoring
- #4543 - debug: Chain old SEGV Handler
- #4545 - Allow nested `ARRAY_FOREACH()`
- #4553 - config: API `has_been_set()`
- #4557 - config: drop ConfigSet from API functions
- #4558 - drop obsolete pgp/smime menus
- #4559 - array: `foreach_reverse()`
- #4560 - Change description of verify-key to be crypto-scheme agnostic
- #4561 - expando: move EnvList out of library
- #4570 - Simplify the management of NeoMutt Commands
- #4571 - libcli - parse the command line
- #4580 - Split CLI Usage into sections
- #4582 - pager: fix lost `NT_PAGER` notifications
- #4591 - pager: fix refresh on config/colour changes
- array: upgrade `get_elem_list()`
- Buffer refactoring
- coverity: fix defects
- improve `localise_config()`
- main: drop -B (batch mode) option
- merge init.[ch] into main.c
- refactor version code
- neomutt: `home_dir`, `username`, `env`
- query: unify NeoMutt `-D` and `-Q`
- refactor `main.c`/`init.c`
- sidebar: streamline expando callbacks
- test: lots of parse coverage
- window refactoring
- window: force recalc|repaint on new windows

## :recycle: Upstream

- Update mutt/queue.h
- Fix NULL pointer dereference when calling `imap_logout_all()`

