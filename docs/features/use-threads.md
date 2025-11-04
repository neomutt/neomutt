# Use Threads

_Improve the experience with viewing threads in the index_

## Support

**Since:** NeoMutt 2021-08-01

**Dependencies:** None

## Introduction

The "use-threads" feature improves the threading experience in NeoMutt. It provides better handling of email threads in the index.

The feature includes options to control how threads are displayed and sorted.

## Variables

| Name | Type | Default |
| ---- | ---- | ------- |
| `use_threads` | sort | (empty) |
| `thread_received` | boolean | `no` |

## neomuttrc

```
# Example NeoMutt config file for the use-threads feature.

# The 'use-threads' feature improves the threading experience.

# Set threading sort order
set use_threads = date
# Use received date for threading
set thread_received = yes

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Richard Russon
