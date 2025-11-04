# Status Color

_Custom rules for theming the status bar_

## Support

**Since:** NeoMutt 2016-03-07

**Dependencies:** None

## Introduction

The "status-color" feature allows you to specify colors for the status bar. The status bar displays information about the current mailbox and message.

You can color different parts of the status bar based on patterns.

## Colors

| Object | Description |
| ------ | ----------- |
| `status` | The entire status bar |
| `status_bright` | Bright status bar |
| `status_dim` | Dim status bar |

## neomuttrc

```
# Example NeoMutt config file for the status-color feature.

# The 'status-color' feature allows you to specify colors for the status bar.

# Color the status bar
color status brightcyan blue
# Color for bright status
color status_bright white blue
# Color for dim status
color status_dim cyan blue

# vim: syntax=neomuttrc
```

## See Also

* [`$status_format`](/guide/reference.html#status-format)

## Known Bugs

None

## Credits

Richard Russon
