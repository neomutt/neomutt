# Mac OSX Keychain

## Prerequisites

- Python 3

- `security` program located in `/usr/bin`

## Setup

The OSX Keychain can be accessed via the GUI or via the `security` program.
Information displayed in the GUI (Applications > Utilities > Keychain Access)
is rather limited, so we are going to use the command line most of the time.

Generally, any item we want to store in the Keychain is associated with a
server name and an account name. We also give that combination a label, such
that we can easily find it in Keychain Access GUI later.

Specifically, let's assume we want to setup IMAP with SSL/TLS for account
`me@example.com`, which has its IMAP server at `imap.example.com`. The
following command will add this account to the Keychain:

```
$ security add-internet-password -l imap-example -a me@example.com \
  -s imaps://imap.example.com
```

Here, `-l` specifies a user-defined label which will be listed as `Name` in
the Keychain Access GUI; `-a` is the user name; and `-s` defines the server
name string. Please note the use of `imaps://` in the definition of this
string, which is generally composed of `protocol://server_name`. The string
for `protocol` is one of NeoMutt's supported account types, `imap`, `pop`,
`smtp`, `nntp` and with a `s` added in case of SSL/TLS use.

The password could be supplied with another option, `-w`. Be aware if this is
done, the cleartext password will be stored in your `~/.bash_history` file.
So instead use the Keychain Access utility and search your newly created
account name (supplied with option `-l`, here it is `imap-example`),
double-click that item in the list and enter your password in the dialog.

Similarly, if sending email is done via host `smtp.example.com`, the following
command will add the corresponding account to the Keychain:

```
$ security add-internet-password -l smtp-example -a me@example.com \
  -s smtps://smtp.example.com
```

Again, the password is better entered via the Keychain Access GUI.

You can test that you have successfully entered your credentials in multiple
ways.

```
$ security find-internet-password -g -l imap-example
```

and

```
$ security find-internet-password -g -a me@example.com \
  -s imaps://imap.example.com
```

will cause the `security` program to query the passwords in the Keychain.
You will be asked your login password to unlock the Keychain. At this dialog
you can decide if you want to allow this once ("Allow") or if you accept to
not be asked in the future again ("Always Allow"). Both commands will dump a
set of Keychain attributes with `password:` somewhere at the end of the output
(option `-g`).

The `keychain.py` program can be tested accordingly, it will output lines
containing `username: `, `login: `, `password: ` for the associated account.

```
$ ./keychain.py --hostname imap.example.com --username me@example.com \
  --type imaps
```

The same commands work with the `smtps` account, too.

In your NeoMutt configuration file, be sure to remove any `*_pass` variables
and set your folder and smtp URLs accordingly, e.g.:

```
set folder = "imaps://me@example.com@imap.example.com"
set smtp_url = "smtps://me@example.com@smtp.example.com"
set account_command = "/usr/share/doc/neomutt/account-command/keychain/keychain.py"
```

From now on, your passwords will be queried through the OSX Keychain.

## Considerations

If for convenience you "Always Allow" the `security` program to access the
Keychain, any other program under your system's account will be able to run
`security` and retrieve Keychain data without any further checks.

If you want to be asked for passwords again, lock the Keychain in Keychain
Access. This is also useful if you once pressed "Deny" when `security` wanted
to retrieve data from the Keychain.
