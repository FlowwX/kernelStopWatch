#!/bin/sh
module="timer"



# remove old module
/sbin/rmmod ./$module.ko $* || true

make clean

make all

# load new module
/sbin/insmod ./$module.ko $* || exit 1

echo "================================="
echo
cat /dev/timerf
echo
echo