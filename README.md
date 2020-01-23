# Cactus File Manager

Cactus File manager (cfm) is a TUI file manager with some features. It's quick
and light. Whether or not you should use it depends on whether or not you like
the name and/or the dev, like with all software.

![Example Configuration](screenshot.png)

## Configuration

To configure cfm before building, you should copy `config.def.h` to `config.h`
and then modify it to suit your needs. Each option is explained within the file.
If you build cfm without creating a config, it will create a default one for
you.

## Building

`make`

## Installing

`sudo make install`

## Usage

This is a non-exhaustive overview. RTFM for a more comprehensive list of keys.
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

*Note: the arrow keys work the same as HJKL.*
