sudo ip addr add dev wlp2s0 172.16.10.1/24
sudo hostapd -d hostapd.conf

echo 'configure android wifi with static ip: 172.16.10.212'

python drone.py

echo 'start android app'
