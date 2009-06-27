#! /bin/sh

XBEE_DRIVER_PATH=$(dirname $0)

insmod ${XBEE_DRIVER_PATH}/n_turk.ko
${XBEE_DRIVER_PATH}/ldisc_daemon

ifconfig zigbee0 10.0.0.0 netmask 255.255.255.0 up


