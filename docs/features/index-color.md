# Index Color

_Custom rules for theming the email index_

## Support

**Since:** NeoMutt 2016-03-07

**Dependencies:**

[status-color feature](/feature/status-color)

## Introduction

The "index-color" feature allows you to specify colors for individual parts of the email index. e.g. Subject, Author, Flags.

First choose which part of the index you'd like to color. Then, if needed, pick a pattern to match.

Note: The pattern does not have to refer to the object you wish to color. e.g.

```
color index_author red default "~sneomutt"
```

The author appears red when the subject (~s) contains "neomutt".

## Colors

All the colors default to `default`, i.e. unset.

The index objects can be themed using the `color` command and an optional pattern. A missing pattern is equivalent to a match-all `.*` pattern.

```
color index-object foreground background [pattern]
```

| Object | Highlights |
| ------ | ---------- |
| `index` | Entire index line |
| `index_author` | Author name, %A %a %F %L %n |
| `index_collapsed` | Number of messages in a collapsed thread, %M |
| `index_date` | Date field |
| `index_flags` | Message flags, %S %Z |
| `index_label` | Message label, %y %Y |
| `index_number` | Message number, %C |
| `index_size` | Message size, %c %cr %l |
| `index_subject` | Subject, %s |
| `index_tag` | Message tags,%G |
| `index_tags` | Transformed message tags,%g %J |

## neomuttrc

```
# Example NeoMutt config file for the index-color feature.

# Entire index line
color index white black '.*'
# Author name, %A %a %F %L %n
# Give the author column a dark grey background
color index_author default color234 '.*'
# Highlight a particular from (~f)
color index_author brightyellow color234 '~fRay Charles'
# Message flags, %S %Z
# Highlight the flags for flagged (~F) emails
color index_flags default red '~F'
# Subject, %s
# Look for a particular subject (~s)
color index_subject brightcyan default '~s\(closes #[0-9]+\)'
# Number of messages in a collapsed thread, %M
color index_collapsed default brightblue
# Date field
color index_date green default
# Message label, %y %Y
color index_label default brightgreen
# Message number, %C
color index_number red default
# Message size, %c %cr %l
color index_size cyan default

# vim: syntax=neomuttrc
```

## See Also

* [Regular Expressions](/guide/advancedusage.html#regex)
* [Patterns](/guide/advancedusage.html#patterns)
* [`$index_format`](/guide/reference.html#index-format)
* [Color command](/guide/configuration.html#color)
* [Status-Color feature](/feature/status-color)

## Known Bugs

None

## Credits

Christian Aichinger, Christoph "Myon" Berg, Elimar Riesebieter, Eric Davis, Vladimir Marek, Richard Russon
