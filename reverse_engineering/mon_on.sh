#!/bin/bash

systemctl stop wpa_supplicant@wlp2s0.service
airmon-ng start wlp2s0 1
airodump-ng wlp2s0mon -c1 -d 0C:8C:24:33:C4:C3 -w sg500

