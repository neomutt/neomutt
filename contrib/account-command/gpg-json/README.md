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

Place the `credentials.sh` shell script alongside your `credentials.json.gpg` file.
You can now point the NeoMutt `account_command` setting to this file.
