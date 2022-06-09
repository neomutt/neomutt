# GPG-based JSON database

## Prerequisites

- [gnupg](https://gnupg.org)
- a private key, which we're assume belongs to me@example.com
- [jq](https://stedolan.github.io/jq/)

## Setup

### Generate your credentials database

Create a `credentials.json` JSON file containing your credentials.
The structure is an array of objects.
Each object represents an account.

```json
[
  { "host": "imap.gmail.com",    "user": "me@gmail.com",    "pass": "s3cr3t" },
  { "host": "imap.fastmail.com", "user": "me@fastmail.com", "pass": "foob4r" },
  { "host": "smtp.fastmail.com", "user": "me@fastmail.com", "pass": "foob4r" }
]
```

### Encrypt the credentials database

Encrypt your credentials file using gpg and get rid of the original file.

```sh
$ gpg -r me@example.com -e credentials.json && rm credentials.json
```

This will generate a `credentials.json.gpg` encrypted file.

### Setup your account command

In your NeoMUtt configuration file, be sure to remove any `*_pass` variables and setup your account command as follows:

```
set account_command = "/usr/share/doc/neomutt/account-command/gpg-json/credentials.sh --credfile /path/to/your/credentials.json.gpg"
```
