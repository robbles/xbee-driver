#! /bin/sh

ifconfig zigbee0 down

pkill -f ldisc_daemon
rmmod n_turk

echo "Xbee driver stopped"

