# lbar
Tiny X11 status bar that i made for [lwm](https://github.com/miublue/lwm) to learn how to use Xft.

## Installing
Compile with:
```sh
make install
```

## Usage
Pipe text ending in new line to lbar, format with '\r' followed by a special character.

## Formatting
`\r[U|L|C|R]`  
`\rU` toggles underline. `\rL`, `\rC` and `\rR` draws text on the left, center and right sides of the bar respectively.  
Example script can be found [here](/input.sh).
Run with:
```sh
./input.sh | lbar
```

## Configuring
Simply edit `config.h` and recompile. Alternatively, change values via command-line options:
```
lbar [-h|-b|-f font|-u size|-g geom|-F foreground|-B background|-U underline]
    -h          show help
    -b          place bar at the bottom of the screen
    -f font     set bar font
    -u size     set bar underline height in pixels
    -g geom     set bar geometry {width}x{height}+{xoffset}+{yoffset}
    -F #RRGGBB  set bar text color
    -B #RRGGBB  set bar background color
    -U #RRGGBB  set bar underline color
```

