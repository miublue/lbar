# lbar
Tiny X11 status bar i made for for [lwm](https://github.com/miublue/lwm) to learn how to use Xft.

## Installing
Compile with:
```sh
make install
```

## Usage
Pipe text ending in '\n' to lbar, format with '\r' followed by a special character.

## Formatting
\r[U|L|C|R]  
\rU toggles underline. \rL, \rC and \rR draws text on the left, center and right sides of the bar respectively.  
Example script can be found [here](/input.sh).

