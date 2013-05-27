#!/bin/sh

export LD_LIBRARY_PATH=`pwd`:/usr/local/lib

let nx=4
let ny=4

let nx_minis1=$nx-1
let ny_minis1=$ny-1

for (( y = 0; y <= $ny_minis1; y ++ ))
do 
    for (( x = 0; x <= $nx_minis1; x ++ ))
    do
        echo $x $y
        let n=$x+$y*$nx
        ./mfx_player -i $1 -hw -no_window_title -wall_n $n -wall_h $ny -wall_w $nx -numsourceseek 100 0 100  &
        sleep 1
    done
done    
