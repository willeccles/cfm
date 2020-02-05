# Cactus File Manager

![build](https://github.com/WillEccles/cfm/workflows/CFM%20Build/badge.svg)

Cactus File manager (cfm) is a TUI file manager with some features here and
there. It's quick and light. Whether or not you should use it depends on whether
or not you like the name and/or the dev, like with all software.

![Example Configuration](screenshot.png)

*Note: the screenshot above has a non-default pointer and no alternative
views enabled.*

[**Demo**](https://asciinema.org/a/297087) showing off deletion, undo, marks, and basic
navigation.

## Configuration

To configure cfm before building, you should copy `config.def.h` to `config.h`
and then modify it to suit your needs. Each option is explained within the file.
If you build cfm without creating a config, it will create a default one for
you.

There are some options which cfm will attempt to use environment variables for.
These are `$EDITOR`, `$SHELL`, and `$OPENER`. If you want to specify a specific
option just for cfm, it will first try to find these variables prefixed with
`$CFM_`. For example, if your `$EDITOR` is set to vim but you want to tell
cfm to use emacs, you could use `export CFM_EDITOR='emacs'`. If you installed
cfm via a package manager, or if you are using the default configuration, you
can specify these environment variables to configure cfm without rebuilding.

## Building

`make`

## Installing

If you are on Arch Linux or an Arch based distro you can install `cfm` via the AUR manually using `makepkg` or by using an AUR helper.
`https://aur.archlinux.org/packages/cfm/`
`yay -S cfm'
or equivalent for your favorite AUR helper.
The AUR package is currently maintained by Hawkeye0021.

If you are installing from source, use `sudo make install`.

cfm is currently available as a package in the KISS community repository. If
you are using the `kiss` package manager, update your community repo and use
`kiss b cfm` and `kiss i cfm` to install cfm.

The manual page will be installed regardless of how you install cfm.

## Usage

The functions of some keys may be dependent on values set in `config.h`.

| Key(s) | Function |
| ------ | -------- |
| q, ESC | Quit cfm |
| h | Go up a directory[<sup>1</sup>](#1) |
| j | Move down[<sup>1</sup>](#1) |
| k | Move up[<sup>1</sup>](#1) |
| l | Enter directory, or open file in `EDITOR`[<sup>1</sup>](#1) |
| dd | Delete currently selected file or directory (there is no confirmation, be careful) |
| T | Creates a new file, opening `EDITOR` to obtain a filename[<sup>2</sup>](#2) |
| M | Creates a new directory, opening `EDITOR` to obtain a directory name[<sup>2</sup>](#2) |
| R | Renames a file, opening `EDITOR` to edit the filename[<sup>2</sup>](#2) |
| gg | Move to top |
| G | Move to bottom |
| m | Mark for deletion |
| D | Delete marked files (does not touch unmarked files) |
| u | Undo the last deletion operation (if cfm was unable to access/create its trash directory `~/.cfmtrash`, deletion is permanent and this will not work) |
| e | Open file or directory in `EDITOR` |
| o | Open file or directory in `OPENER` |
| S | Spawns a `SHELL` in the current directory |
| r | Reload directory |
| . | Toggle visibility of hidden files (dotfiles) |
| RET | Works like `o` if `ENTER_OPENS` was enabled at compile-time, else works like `l` |
| TAB | Switches to the next view |
| \` | Switches to the previous view |
| 1-0 | Switches to view N, up to the number specified by `VIEW_COUNT` (default 2) |

---

<a class="anchor" id="1"></a><sup>1</sup> The arrow keys work the same as HJKL.

<a class="anchor" id="2"></a><sup>2</sup> The available characters for filenames are `A-Za-z
._-` by default, which is POSIX "fully portable filenames" plus spaces. If
you wish, you can disable spaces by setting `ALLOW_SPACES` to 0.
