# Sensible Browser

_Make the file browser behave_

## Support

**Since:** NeoMutt 2016-09-10

**Dependencies:** None

## Introduction

The "sensible-browser" feature improves the behavior of the file browser in NeoMutt. It makes the browser more intuitive and user-friendly.

The feature changes the default behavior of the browser to be more sensible, such as sorting directories first, hiding dot files, etc.

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `browser_abbreviate_mailboxes` | boolean | `yes` |
| `browser_sort_order` | sort | `alpha` |

## neomuttrc

```
# Example NeoMutt config file for the sensible-browser feature.

# The 'sensible-browser' feature improves the behavior of the file browser.

# Abbreviate mailbox names in the browser
set browser_abbreviate_mailboxes = yes
# Sort browser entries alphabetically
set browser_sort_order = alpha

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Richard Russon
