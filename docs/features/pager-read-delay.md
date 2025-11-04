# Pager Read Delay

_Delay when the pager marks a previewed message as read_

## Support

**Since:** NeoMutt 2021-06-17

**Dependencies:** None

## Introduction

The "pager-read-delay" feature allows you to delay marking a message as read when you view it in the pager. This can be useful if you want to preview messages without marking them as read immediately.

By default, NeoMutt marks a message as read as soon as you view it in the pager. With this feature, you can set a delay before the message is marked as read.

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `pager_read_delay` | number | `0` |

The `pager_read_delay` variable specifies the number of seconds to wait before marking a message as read when viewed in the pager. If set to 0, the message is marked as read immediately.

## neomuttrc

```
# Example NeoMutt config file for the pager-read-delay feature.

# The 'pager-read-delay' feature allows you to delay marking a message as read when you view it in the pager.

# Set the delay to 5 seconds
set pager_read_delay = 5

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Dennis Schön
