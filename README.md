# Cactus File Manager

Cactus File manager (cfm) is a TUI file manager with some features. It's quick
and light. Whether or not you should use it depends on whether or not you like
the name and/or the dev, like with all software.

![Example Configuration](screenshot.png)

*Note: the screenshot above has a non-default pointer.*

[Demo](https://asciinema.org/a/296578)

## Configuration

To configure cfm before building, you should copy `config.def.h` to `config.h`
and then modify it to suit your needs. Each option is explained within the file.
If you build cfm without creating a config, it will create a default one for
you.

There are some options which cfm will attempt to use environment variables for.
These are `$EDITOR`, `$SHELL`, and `$OPENER`. If you want to specify a specific
option just for cfm, it will first try to find these variables prefixed with
`$CFM_`. For example, if your `$EDITOR` is set to `vim` but you want to tell
cfm to use emacs, you could use `export CFM_EDITOR='emacs'`.

## Building

`make`

## Installing

`sudo make install`

## Usage

The functions of some keys may be dependent on values set in `config.h`.

| Key(s) | Function |
| ------ | -------- |
| q, ESC | Quit cfm |
| h | Go up a directory |
| j | Move down |
| k | Move up |
| l | Enter directory, or open file in `EDITOR` |
| dd | Delete currently selected file or directory (there is no confirmation, be careful) |
| gg | Move to top |
| G | Move to bottom |
| m | Mark for deletion |
| D | Delete marked files (does not touch unmarked files) |
| e | Open file or directory in `EDITOR` |
| o | Open file or directory in `OPENER` |
| r | Reload directory |
| . | Toggle visibility of hidden files (dotfiles) |
| RET | Works like `o` if `ENTER_OPENS` was enabled at compile-time, else works like `l` |

*Note: the arrow keys work the same as HJKL.*
