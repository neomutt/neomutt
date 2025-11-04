# Global Hooks

_Define actions to run globally within NeoMutt_

## Introduction

These hooks are called when global events take place in NeoMutt.

**Run a command...**

* **timeout-hook** – periodically
* **startup-hook** – when NeoMutt starts up, before opening the first mailbox
* **shutdown-hook** – NeoMutt shuts down, before closing the last mailbox

The commands are NeoMutt commands. If you want to run an external shell command, you need to run them like this:

```
startup-hook 'echo `action.sh ARGS`'
```

The single quotes prevent the backticks from being expanded. The `echo` command prevents an empty command error.

### Timeout Hook

#### Run a command periodically

**Since:** NeoMutt 2016-08-08

This feature implements a new hook that is called periodically when NeoMutt checks for new mail. This hook is called every `$timeout` seconds.

### Startup Hook

#### Run a command when NeoMutt starts up, before opening the first mailbox

**Since:** NeoMutt 2016-11-25

This feature implements a new hook that is called when NeoMutt first starts up, but before opening the first mailbox. This is most likely to be useful to users of [notmuch](/feature/notmuch).

### Shutdown Hook

#### Run a command when NeoMutt shuts down, before closing the last mailbox

**Since:** NeoMutt 2016-11-25

This feature implements a hook that is called when NeoMutt shuts down, but before closing the last mailbox. This is most likely to be useful to users of [notmuch](/feature/notmuch).

## Commands

* `timeout-hook` _`command`_
* `startup-hook` _`command`_
* `shutdown-hook` _`command`_

## neomuttrc

```
# Example NeoMutt config file for the global hooks feature.

# --------------------------------------------------------------------------
# COMMANDS – shown with an example argument
# --------------------------------------------------------------------------
# After $timeout seconds of inactivity, run this NeoMutt command
timeout-hook 'exec sync-mailbox'
# When NeoMutt first loads, run this NeoMutt command
startup-hook 'exec sync-mailbox'
# When NeoMutt quits, run this NeoMutt command
shutdown-hook 'exec sync-mailbox'

# vim: syntax=neomuttrc
```

## See Also

* [`$timeout`](/guide/reference.html#timeout)

## Known Bugs

None

## Credits

Armin Wolfermann, Richard Russon, Thomas Adam
