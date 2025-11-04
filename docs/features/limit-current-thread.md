# Limit Current Thread

_Focus on one Email Thread_

## Support

**Since:** NeoMutt 2016-03-28

**Dependencies:** None

## Introduction

The "limit-current-thread" feature adds a new command that limits the view to the current thread. This is useful when you want to focus on one conversation and temporarily ignore all other emails.

The command will limit the view to the thread of the currently highlighted message. This is equivalent to pressing `l` (limit) and typing `~T` (threads).

## Commands

* `limit-current-thread`

## neomuttrc

```
# Example NeoMutt config file for the limit-current-thread feature.

# The 'limit-current-thread' feature adds a new command that limits the view to the current thread.

# This is useful when you want to focus on one conversation and temporarily ignore all other emails.

# Bind the command to a key
bind index l limit-current-thread

# vim: syntax=neomuttrc
```

## See Also

* [Limiting](/guide/advancedusage.html#limiting)
* [Patterns](/guide/advancedusage.html#patterns)

## Known Bugs

None

## Credits

David J. Pearce, Richard Russon
