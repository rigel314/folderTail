folderTail
==========

Displays a `tail -f` for each regular file or link to a regular file that is a direct child of `<path>`.
Implements what multiltail's broken -q option should do.

Dependencies: ncurses and math.h

Build: make

Install: make install

Uninstall: make uninstall

I didn't want to figure out what multitail was doing, and I figured it wouldn't be too hard to do this.

Usage: `folderTail <path>`
