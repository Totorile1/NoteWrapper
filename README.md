# NoteWrapper

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/tomasriveral/notewrapper)

## Why?

I started journaling and note-taking with **[Obsidian](https://obsidian.md/)** but wanted to use only free software. I also tried **[Logseq](https://logseq.com/)** and **[Joplin](https://github.com/laurent22/joplin)**, but I preferred a terminal-based workflow.

Using my Neovim setup, I explored plugins such as **[neorg](https://github.com/nvim-neorg/neorg)**, **[orgmode](https://github.com/nvim-orgmode)**, and **[today.nvim](https://github.com/VVoruganti/today.nvim)**. While they offered useful features, none fully met my needs: some lacked external Markdown rendering, some relied on custom file formats, and others did not provide a true journal workflow.

For real-time Markdown rendering from Neovim, I found **[Vivify](https://github.com/jannis-baum/Vivify/)**, which NoteWrapper relies on.

The goal was to create a terminal-based interface for accessing vaults and notes using standard Markdown, with minimal additional complexity. Although editor support is currently limited, NoteWrapper is designed as a standalone wrapper that can be adapted to other editors with minimal changes.

---

## Features

* Terminal-based note and journal system
* Vault-based organization (similar to Obsidian)
* Standard Markdown files (no custom format)
* Journal support with flexible entry formats
* Real-time Markdown rendering via Vivify
* Backup support using `rsync`
* Supports multiple editors

---

## Dependencies

Before building NoteWrapper, you must install the following dependencies:

* `ncurses`
* `cjson`
* `make`
* `Vivify`
* `rsync`
* `pkg-config`
* `sed`
* `ripgrep`
* `fzf`

You must also have a [supported editor (and their associated plugin if needed)](#editor-support) installed:

* `helix`
* `nano`
* `neovim`
* `vim`

---

## How to install

1. Install all required dependencies listed above.
2. Ensure you have a supported editor installed.
3. Clone the repository:

```shell
git clone https://github.com/Totorile1/NoteWrapper.git
cd NoteWrapper
```

4. Build the project:

* On **NixOS**:

```shell
nix-build
```

* On other systems:

```shell
make
```

5. Configure `./config.json` (it is automatically created on first launch if it does not already exist).
6. Run the program:

```shell
./notewrapper
```

---

## Packaging

There is currently no official package available for NoteWrapper.

However, contributions are welcome for packaging on any distribution.

As a NixOS user, I will likely package it for **nixpkgs** in the future if the project gains traction.

---

## Usage

```
Usage: notewrapper [options]
Options:
  -c, --config <path/to/config>               Specify the config file.
  -d, --directory <path/to/directory>         Specify the vaults' directory.
  -h, --help                                  Display this message.
  -e, --editor                                Specify the editor to open.
  -j, --jump                                  Jump to the end of the file on opening.
  -J, --no-jump                               Do not jump to the end of the file.
  -n, --note <note's name>                    Specify the note (or journal).
  -r, --render                                Render the note with Vivify.
  -R, --no-render                             Do not render.
  -v, --vault <vault's name>                 Specify the vault.
  --version                                  Display the program version and the GPL3 notice.
  -V, --verbose                              Show debug information.
```

Files are organized similarly to Obsidian. You have a directory containing all your vaults, where each vault is a separate directory.

NoteWrapper distinguishes two types of files:

* **Notes**: act as a single continuous file for writing
* **Journals**: split into timed entries (daily, monthly, or custom formats — see [configuration](#configuration))

Journals can be of two types:

* **Divided**: one separate file per entry
* **Unified**: one file where new entries are appended

---

## Editor support

NoteWrapper relies on certain editor features, so not all functionality is supported by every editor.

### Features requiring editor support:

* **Bufferless rendering**: updates the rendered view while typing (without saving)
* **Cursor following**: rendered view follows the cursor position
* **Jump to end on open**: automatically moves the cursor to the end of the file

The first two features depend on [Vivify's editor integration](https://github.com/jannis-baum/Vivify?tab=readme-ov-file#existing-integration) and are mainly useful if you want external Markdown rendering in your browser.

If your editor does not support these features, you can implement a plugin using [Vivify's API](https://github.com/jannis-baum/Vivify?tab=readme-ov-file#editor-support).

| Editor | Bufferless  | Cursor  | Jump to end  | Plugin required                                         |
| ------ | ----------- | ------- | ------------ | --------------------------------------------------------|
| Helix  | ❌          | ❌      | ✅           | —                                                       |
| Nano   | ❌          | ❌      | ✅           | —                                                       |
| Neovim | ✅          | ✅      | ✅           | [vivify-vim](https://github.com/jannis-baum/vivify.vim) |
| Vim    | ✅          | ✅      | ✅           | [vivify-vim](https://github.com/jannis-baum/vivify.vim) |

[How to add support for another editor](./CONTRIBUTING.md#adding-editor-support)

---

## Configuration

Edit `~/.config/notewrapper/config.json`. If it does not exist, it will be created on first launch.

```json
{
  "directory": "~/Documents/Notes/",
  "render": true,
  "jumpToEndOfFileOnLaunch": true,
  "editor": "neovim",
  "journalRegex": ".*journal.*",
  "dateEntry": "# %a %d %m %Y",
  "newLineOnOpening": true,
  "backup": {
    "enable": false,
    "directory": "/path/to/backup",
    "interval": "weekly",
    "rsyncArgs": ["-Lqah", "--update"]
  }
}
```

### Fields

* `directory`: root directory containing all vaults
* `render`: enable/disable Vivify rendering
* `jumpToEndOfFileOnLaunch`: move cursor to end of file on open
* `editor`: selected editor (must be supported)
* `journalRegex`: regex used to detect journal files
* `dateEntry`: format for journal entries (see `strftime`)
* `newLineOnOpening`: add a newline when opening a note
* `backup.enable`: enable automatic backups using `rsync`
* `backup.directory`: destination directory for backups
* `backup.interval`: backup frequency (`daily`, `weekly`, `monthly`, or integer)
* `backup.rsyncArgs`: arguments passed to `rsync`

---

## Vivify configuration

Some settings must be configured in Vivify itself. See:
[https://github.com/jannis-baum/Vivify/blob/main/docs/customization.md](https://github.com/jannis-baum/Vivify/blob/main/docs/customization.md)

It is recommended to use a browser different from your main one for rendering.

---

## Planned features

* [ ] A converter between journal types
* [ ] Support multiple vault directories
* [ ] Port NoteWrapper to other editors (non-exhaustive list of planned ports: `emacs -nw`, `micro`, `jed`, kakoune, `ad`, flow-control, `ee`, `amp`, `dte`, `cano`, `mle`, `zee`, `ptext`, `kibi`, `ox`, `ne`, `nvi`, `dit`, `zile`, `moe`, `joe`)
* [ ] Default to $EDITOR
