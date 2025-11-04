# New Mail

_Execute a command upon the receipt of new mail._

## Support

**Since:** NeoMutt 2016-07-23

**Dependencies:** None

## Introduction

The "new-mail" feature allows you to execute a command when new mail arrives. This can be used to play a sound, send a notification, or perform any other action.

The command is executed when NeoMutt detects new mail in any mailbox. The command is passed the following arguments:

* The number of new messages
* The number of old messages
* The number of flagged messages
* The mailbox name

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `new_mail_command` | string | (empty) |

## neomuttrc

```
# Example NeoMutt config file for the new-mail feature.

# The 'new-mail' feature allows you to execute a command when new mail arrives.

# This can be used to play a sound, send a notification, or perform any other action.

# Execute a command when new mail arrives
set new_mail_command = "notify-send 'New mail in %f'"

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Richard Russon
