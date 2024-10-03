#/usr/bin/bash

Window() {
    echo -n $(xdotool getwindowname $(xdotool getwindowfocus))
}

Memory() {
    echo -n $(free --si -h | grep 'Mem:' | awk '{printf "%s/%s",$3,$2;}')
}

Date() {
    echo -n $(date "+%d-%m-%y %H:%M")
}

while sleep 0.1; do
    echo "&L&U $(Memory)  &U &C $(Window) &R&U $(Date)  &U"
done
