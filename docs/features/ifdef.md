# Ifdef

_Conditional config options_

## Support

**Since:** NeoMutt 2016-03-07

**Dependencies:** None

## Introduction

The "ifdef" feature introduces three new commands to NeoMutt and allows you to share one config file between versions of NeoMutt that may have different features compiled in.

```
ifdef  symbol "config-command [args...]"
# If a symbol is defined
ifndef symbol "config-command [args...]"
# If a symbol is not defined
finish
# Finish reading the current file
```

| Example Symbol | Description |
| -------------- | ----------- |
| `sidebar_format` | Config variable |
| `status-color,imap` | Compiled-in feature |
| `pgp-menu,group-related` | Function |
| `index-format-hook,tag-transforms` | Command |
| `indicator,sidebar_new` | Colour |
| `my_var` | My variable |
| `lmdb,tokyocabinet` | Store (database) |
| `HOME,COLUMNS` | Environment variable |

A list of compile-time symbols can be seen in the output of the command

```
neomutt -v
```

(in the "Compile options" section).

`finish` is particularly useful when combined with `ifndef`. e.g.

```
# Sidebar config file
ifndef sidebar finish
```

## Commands

* `ifdef` _`symbol`_ _`"config-command [args...]"`_
* `ifndef` _`symbol`_ _`"config-command [args...]"`_
* `finish`

## neomuttrc

```
# Example NeoMutt config file for the ifdef feature.

# This feature introduces three useful commands which allow you to share
# one config file between versions of NeoMutt that may have different
# features compiled in.

#   ifdef  symbol "config-command [args...]"
#   ifndef symbol "config-command [args...]"
#   finish
# The 'ifdef' command tests whether NeoMutt understands the name of
# a variable, environment variable, function, command or compile-time symbol.

# If it does, then it executes a config command.

# The 'ifndef' command tests whether a symbol does NOT exist.

# The 'finish' command tells NeoMutt to stop reading current config file.

# If the 'trash' variable exists, set it.
ifdef trash 'set trash=~/Mail/trash'
# If the 'PS1' environment variable exists, source config file.
ifdef PS1 'source .neomutt/interactive.rc'
# If the 'tag-pattern' function exists, bind a key to it.
ifdef tag-pattern 'bind index <F6> tag-pattern'
# If the 'imap-fetch-mail' command exists, read my IMAP config.
ifdef imap-fetch-mail 'source ~/.neomutt/imap.rc'
# If the compile-time symbol 'sidebar' does not exist, then
# stop reading the current config file.
ifndef sidebar finish

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Cedric Duval, Matteo F. Vescovi, Richard Russon
