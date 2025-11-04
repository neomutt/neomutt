# Skip Quoted

_Managing quoted text in the pager_

## Support

**Since:** NeoMutt 2016-03-28

**Dependencies:** None

## Introduction

The "skip-quoted" feature allows you to skip over quoted text in the pager. This is useful for reading long email threads where much of the text is quoted from previous messages.

The feature provides commands to jump to the next or previous unquoted line.

## Commands

* `skip-quoted` - Skip to the next unquoted line
* `skip-quoted-backward` - Skip to the previous unquoted line

## neomuttrc

```
# Example NeoMutt config file for the skip-quoted feature.

# The 'skip-quoted' feature allows you to skip over quoted text in the pager.

# Bind skip-quoted to keys
bind pager <Space> skip-quoted
bind pager - skip-quoted-backward

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Richard Russon
