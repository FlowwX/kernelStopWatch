#!/bin/sh
module="timer"



# remove old module
/sbin/rmmod ./$module.ko $* || true

#clean
make clean

#compile
make all

# load new module
/sbin/insmod ./$module.ko $* || exit 1

# change permissions
chmod 666 /dev/timer?