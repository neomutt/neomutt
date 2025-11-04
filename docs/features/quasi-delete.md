# Quasi-Delete

_Mark emails that should be hidden, but not deleted_

## Support

**Since:** NeoMutt 2016-03-07

**Dependencies:** None

## Introduction

The "quasi-delete" feature allows you to mark messages as deleted without actually deleting them. This is useful for reviewing messages before permanently deleting them.

Messages marked as quasi-deleted are hidden from the index but can be restored.

## Commands

* `quasi-delete` - Mark the current message as quasi-deleted

## neomuttrc

```
# Example NeoMutt config file for the quasi-delete feature.

# The 'quasi-delete' feature allows you to mark messages as deleted without actually deleting them.

# Bind quasi-delete to a key
bind index d quasi-delete

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Richard Russon
