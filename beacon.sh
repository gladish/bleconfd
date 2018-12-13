#!/bin/sh
hciconfig hci0 down
hciconfig hci0 up
#hciconfig hci0 class 000432
hciconfig hci0 class 3a0430
hciconfig hci0 noleadv
hcitool cmd 0x08 0x0008 1F 02 01 05 03 03 12 18 03 19 C1 03 0E 09 58 66 69 6E 69 74 79 2E 78 43 61 6D 32 04 FF 5D 00 04
hcitool cmd 0x08 0x0006 A0 00 A0 00 00 00 00 00 00 00 00 00 00 07 00
hciconfig hci0 leadv

