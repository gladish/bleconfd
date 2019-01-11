#!/bin/sh
ifconfig  | grep Ether | head -1 | awk '{print $2}'
