# Trash Folder

_Automatically move deleted emails to a trash bin_

## Support

**Since:** NeoMutt 2016-03-07

**Dependencies:** None

## Introduction

The "trash" feature allows you to specify a trash folder where deleted messages are moved instead of being permanently deleted.

This provides a safety net for accidentally deleted messages.

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `trash` | string | (empty) |

## neomuttrc

```
# Example NeoMutt config file for the trash feature.

# The 'trash' feature allows you to specify a trash folder for deleted messages.

# Set the trash folder
set trash = "~/Mail/trash"

# vim: syntax=neomuttrc
```

## See Also

* [Mailbox Formats](/guide/configuration.html#mailboxes)

## Known Bugs

None

## Credits

Richard Russon
