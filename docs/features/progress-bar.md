# Progress Bar

_Show a visual progress bar on slow operations_

## Support

**Since:** NeoMutt 2016-03-07

**Dependencies:** None

## Introduction

The "progress" feature displays a progress bar during slow operations, such as opening a large mailbox or performing a search.

The progress bar shows the percentage complete and an estimate of the time remaining.

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `progress_format` | string | (empty) |

## neomuttrc

```
# Example NeoMutt config file for the progress feature.

# The 'progress' feature displays a progress bar during slow operations.

# Customize the progress bar format
set progress_format = " [%P] %s %S"

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Richard Russon
