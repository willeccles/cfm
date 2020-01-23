# Cactus File Manager

Cactus File Manager is a cool and good file manager which is fast and good and
cool. You should use it.

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

If you have installed the manpage, RTFM. Basic overview:

| Key(s) | Function |
| ------ | -------- |
| q, ESC | Quit cfm |
| h | Go up a directory |
| j | Move down |
| k | Move up |
| l | Enter directory, or open file in `EDITOR` |
| dd | Delete currently selected file or directory (there is no confirmation, be
careful) |
| r | Reload the current directory |
| e | Open currently selected file or directory in `EDITOR` |
| o | Open currently selected file in `OPENER`, or change directory to selected
directory |
| RET | If an `OPENER` and `ENTER_OPENS` are both enabled in `config.h`, return
works like o; else, return works like l |
| gg | Move to top |
| G | Move to bottom |

*Note: the arrow keys work the same as HJKL.*
