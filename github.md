NeoMutt 0000-00-00

## :book: Notes

- Compose Preview Message

## :gem: Sponsors

Special thanks to our **sponsors**:

- First Last (@nick)

## :heart: Thanks

Many thanks to our **new contributors**:

- Chao-Kuei Hung (@ckhung)
- Dmitry Polunin (@frei-0xff)

and our **regular contributors**:

- Marius Gedminas (@mgedmin)
- Ryan d'Huart (@homoelectromagneticus)
- Страхиња Радић (@strahinja)
- Claes Nästén (@pekdon)
- Alejandro Colomar (@alejandro-colomar)
- Dennis Schön (@roccoblues)
- Scott Kostyshak (@scottkosty)
- Pietro Cerutti (@gahr)
- EC Herenz (@knusper)
- Carlos Henrique Lima Melara (@charles2910)

and our **sharp-eyed testers**:

- First Last (@nick)

## :lock: Security

- description

## :gift: Features

- #4437 show message preview in compose view
- #4439 add trailing commas when editing addresses

## :sparkles: Contrib

- description

## :lady_beetle: Bug Fixes

- #4444 expando: fix overflow
- #4461 Spaces can be wide
- #4464 Remove BOM from UTF-8 text
- #4467 Bug with wrong fingerprints in certificate_file
- #4470 fix postponed sorting assertion failure
- expando: fix crash on empty %[] date
- expando: fix container formatting
- browser: fix 'tag-' display
- index: force another refresh
- query: fix memory leak
- fix more arrow_cursor + search

## :wrench: Changed Config

- Config Renames:
  - `$pgp_sort_keys`       -> `$pgp_key_sort`
  - `$sidebar_sort_method` -> `$sidebar_sort`
  - `$sort_alias`          -> `$alias_sort`
  - `$sort_browser`        -> `$browser_sort`
- Changed Defaults:
  - `set alias_format = "%3i %f%t %-15a %-56A | %C%> %Y"`
  - `set query_format = "%3i %t %-25N %-25E | %C%> %Y"`

## :black_flag: Translations

- 00% :fr: French
- 00% :lithuania: Lithuanian
- 00% :serbia: Serbian
- 00% :taiwan: Chinese (Traditional)

## :shield: Coverity Defects

- Explicit null dereferenced
- Overflowed constant
- Overflowed return value
- Resource leak

## :books: Docs

* d0cef66f5 docs: alias tags

## :link: Website

- description

## :building_construction: Build

- 75f585100 #4452 only use struct tm.tm_gmtoff if available

## :gear: Code

- #4447 refactor 'sort' constants
- #4294 refactor memory allocation
- unify Menu data
- #4449 add mutt_window_swap()
- #4442 remove unused fields from ComposeSharedData
- move config to libraries
- unify Alias/Query
- expando factor out callbacks
- refactor simple_dialog_new()
d1c339c4a test: add TEST_CHECK_NUM_EQ()

## :wastebasket: Tidy

- description

## :recycle: Upstream

- #4448 Update mutt/queue.h

