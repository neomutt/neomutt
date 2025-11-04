# Account Command

_Populate account credentials via an external command_

## Support

**Since:** NeoMutt 2022-05-16

**Dependencies:** None

## Introduction

NeoMutt provides dedicated config variables to specify credentials for network servers. These include `imap_user`, `imap_pass`, `smtp_user`, `smtp_pass`, etc. There are a few downsides to this approach. For one thing, their use encourages storing usernames and passwords in plain text inside a NeoMutt config file. People have come up with solutions to this, including using gpg-encrypted files and populating `my_` variables via external scripts through `source "/path/to/script|"`. However, once the variables are set, the secrets can be inspected with the `set` command. Also, because these config variables are not account-specific, they have been the cause of a proliferation of ways to mimic per-account setups using a combination of convoluted hooks and macros to modify them on folder change or on a keypress.

The goal of this feature is to get rid of most `_user` and `_pass` variables. To do so, we provide a way of specifying an external command that NeoMutt will call to populate account credentials for network servers such as IMAP, POP, or SMTP. The external command is called with a number of arguments indicating the known properties of the account such as the account type and hostname; the external command provides NeoMutt with a list of additional properties such as username and password.

## Usage

The variable `account_command` configures an external program to be used to gather account credentials.

```
set account_command = "/path/to/my/script.sh"
```

The program specified will be called by NeoMutt with a number of key-value command line arguments.

* `--hostname val`: the network host name of the service
* `--username val`: the username for the account. This might be specified in the URL itself, e.g., `set folder="imaps://me@example.com@example.com"` or with a dedicated existing variable, e.g., `set imap_user=me@example.com`.
* `--type val`: the type of the account, one of `imap`, `pop`, `smtp`, `nntp`, with an optional trailing `s` if SSL/TLS is required.

The program specified will have to respond by printing to stdout a number of `key: value` lines. NeoMutt currently recognizes the following keys.

* login
* username
* password

Because password can contain any characters, including spaces, we expect lines to match the regex `^([[:alpha:]]+): (.*)$` exactly.

## Known Bugs

None

## Credits

Pietro Cerutti
