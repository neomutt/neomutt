# TLS-SNI

_Negotiate with a server for a TLS/SSL certificate_

## Support

**Since:** NeoMutt 2016-03-07

**Dependencies:** None

## Introduction

The "tls-sni" feature enables Server Name Indication (SNI) for TLS/SSL connections. SNI allows a server to present multiple certificates on the same IP address and port number.

This is useful for servers that host multiple domains.

## Variables

The feature is enabled by default if the underlying TLS library supports it.

## neomuttrc

```
# Example NeoMutt config file for the tls-sni feature.

# The 'tls-sni' feature enables Server Name Indication for TLS connections.

# It is enabled by default if supported.

# vim: syntax=neomuttrc
```

## Known Bugs

None

## Credits

Richard Russon
