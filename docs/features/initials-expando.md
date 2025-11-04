# Initials Expando

_Expando for author's initials_

## Support

**Since:** NeoMutt 2016-03-07

**Dependencies:** None

## Introduction

The "initials" feature adds an expando (%I) for an author's initials.

The index panel displays a list of emails. Its layout is controlled by the `$index_format` variable. Using this expando saves space in the index panel. This can be useful if you are regularly working with a small set of people.

## Variables

This feature has no config of its own. It adds an expando which can be used in the `$index_format` variable.

## neomuttrc

```
# Example NeoMutt config file for the initials feature.

# The 'initials' feature has no config of its own.

# It adds an expando for an author's initials,
# which can be used in the 'index_format' variable.

# The default 'index_format' is:
set index_format='%4C %Z %{%b %d} %-15.15L (%<l?%4l&%4c>) %s'
# Where %L represents the author/recipient
# This might look like:
#       1   + Nov 17 David Bowie   Changesbowie    ( 689)
#       2   ! Nov 17 Stevie Nicks  Rumours         ( 555)
#       3   + Nov 16 Jimi Hendrix  Voodoo Child    ( 263)
#       4   + Nov 16 Debbie Harry  Parallel Lines  ( 540)
# Using the %I expando:
set index_format='%4C %Z %{%b %d} %I (%<l?%4l&%4c>) %s'
# This might look like:
#       1   + Nov 17 DB Changesbowie    ( 689)
#       2   ! Nov 17 SN Rumours         ( 555)
#       3   + Nov 16 JH Voodoo Child    ( 263)
#       4   + Nov 16 DH Parallel Lines  ( 540)

# vim: syntax=neomuttrc
```

## See Also

* [`$index_format`](/guide/reference.html#index-format)
* [index-color feature](/feature/index-color)
* [folder-hook](/guide/configuration.html#folder-hook)

## Known Bugs

None

## Credits

Vsevolod Volkov, Richard Russon
