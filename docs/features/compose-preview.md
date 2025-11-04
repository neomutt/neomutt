# Compose Preview

_Show a preview of the message in the Compose Dialog_

## Support

**Since:** NeoMutt 2024-12-12

**Dependencies:** None

## Introduction

NeoMutt shows you a preview of the message you are about to send in the compose dialog.

## Variables

| Name | Type | Default | Description |
| ---- | ---- | ------- | ----------- |
| `compose_preview_above_attachments` | boolean | `no` | Show the message preview above the attachments list. |
| `compose_preview_min_rows` | number | `5` | Hide the preview if it has fewer than this number of rows |
| `compose_show_preview` | boolean | `no` | Enable or disable the message preview feature |

## Functions

The message preview is controlled by the following functions.

| Menus | Function | Description | Default |
| ----- | -------- | ----------- | ------- |
| compose | `<preview-page-down>` | show the next page of the message | `<PageDown>` |
| compose | `<preview-page-up>` | show the previous page of the message | `<PageUp>` |

## Limitations

This is a new feature and it's still under development. If you find any problems, or you'd like to help improve it, please let us know.

* Pager displays simple text, no colour or attributes
* Smart text wrapping is not supported

## Credits

Dennis Schön
