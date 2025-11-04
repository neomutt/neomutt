# NeoMutt

[![Stars](https://img.shields.io/github/stars/neomutt/neomutt.svg?style=social&label=Stars)](https://github.com/neomutt/neomutt "Give us a Star")
[![Twitter](https://img.shields.io/twitter/follow/NeoMutt_Org.svg?style=social&label=Follow)](https://twitter.com/NeoMutt_Org "Follow us on Twitter")
[![Contributors](https://img.shields.io/badge/Contributors-292-orange.svg)](https://github.com/neomutt/neomutt/blob/main/AUTHORS.md "All of NeoMutt's Contributors")
[![Release](https://img.shields.io/github/release/neomutt/neomutt.svg)](https://github.com/neomutt/neomutt/releases/latest "Latest Release Notes")
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://github.com/neomutt/neomutt/blob/main/LICENSE.md "Copyright Statement")
[![Code build](https://github.com/neomutt/neomutt/actions/workflows/build-and-test.yml/badge.svg?branch=main&event=push)](https://github.com/neomutt/neomutt/actions/workflows/build-and-test.yml "Latest Automatic Code Build")
[![Coverity Scan](https://img.shields.io/coverity/scan/8495.svg)](https://scan.coverity.com/projects/neomutt-neomutt "Latest Code Static Analysis")
[![Website build](https://github.com/neomutt/neomutt.github.io/actions/workflows/pages/pages-build-deployment/badge.svg)](https://neomutt.org/ "Website Build")

- [About](#table-of-contents)
- [Install](#install)
- [Features](#features)
- [Contributing](#contributing)
- [Screenshots](#screenshots)
- [License](#license)
- [Community](#community)
- [Resources](#resources)

Welcome to NeoMutt—a modern, feature-rich email client forked from Mutt, with enhancements like sidebar navigation, Notmuch integration, and compose preview.

NeoMutt was started by Richard Russon (@FlatCap),
who took all the old patches and painstakingly sorted through them,
fixed them up and documented them.
He remains the lead developer and has done the bulk of the work to make NeoMutt what it is today.

## Install

NeoMutt is available in the package repositories of most Linux and BSD distributions. Refer to your distribution's packaging instructions for how to install NeoMutt.

If NeoMutt is not available in your distribution's repositories, you can [build from source](docs/BUILD.md):

- Clone the repo: `git clone https://github.com/neomutt/neomutt`
- Build: `./configure && make && make install`
- Launch: `neomutt`

**Prerequisites**: Requires a C compiler, ncurses, and optional libraries like GPGME for encryption.

[![](https://repology.org/badge/vertical-allrepos/neomutt.svg?columns=3)](https://repology.org/project/neomutt/versions)

## Features

See [docs/features/README.md](docs/features/README.md) for a detailed list of NeoMutt's features, including sidebar navigation, encryption support, Notmuch integration, and compose preview.

## Contributing

We welcome contributions! See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for guidelines on reporting issues, submitting patches, and joining the community.

## Screenshots

<img src="docs/neomutt-screenshot.png" width="600" alt="NeoMutt Screenshot">

## License

NeoMutt is licensed under GPL-2.0-or-later. See [LICENSE.md](LICENSE.md) for full details.

## Community
- [Issues & Bugs](https://github.com/neomutt/neomutt/issues)
- IRC: [irc://irc.libera.chat/neomutt](https://web.libera.chat/#neomutt) (be patient, we're a small team!)
- Mailing Lists: [neomutt-users](mailto:neomutt-users-request@neomutt.org?subject=subscribe) and [neomutt-devel](mailto:neomutt-devel-request@neomutt.org?subject=subscribe)

## Resources
- [Source Code](https://github.com/neomutt/neomutt)
- [Releases](https://github.com/neomutt/neomutt/releases/latest)
- [Website](https://neomutt.org)
- [Development](https://neomutt.org/dev.html)
- [Features](docs/features/README.md)
- [Contributors](AUTHORS.md)
