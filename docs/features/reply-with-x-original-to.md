# Reply With X-Original-To

_Direct reply to email using X-Original-To header_

## Support

**Since:** NeoMutt 2016-09-10

**Dependencies:** None

## Introduction

The "reply-with-xorig" feature allows you to reply to emails using the X-Original-To header instead of the To header. This is useful for mailing lists where the To header may not contain the correct address.

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `reply_with_xorig` | boolean | `no` |

## neomuttrc

```
# Example NeoMutt config file for the reply-with-xorig feature.

# The 'reply-with-xorig' feature allows you to reply to emails using the X-Original-To header.

# Enable replying with X-Original-To
set reply_with_xorig = yes

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Richard Russon
