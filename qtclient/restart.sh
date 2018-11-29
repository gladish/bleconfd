#!/bin/sh
/etc/init.d/bluetooth stop
hciconfig hci0 down
hciconfig hci0 up
rm -rf /var/lib/bluetooth/*
/etc/init.d/bluetooth start
