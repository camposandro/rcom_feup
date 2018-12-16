#!/bin/bash

ifconfig eth0 up
ifconfig eth0 172.16.21.1
route add -net 172.16.20.0/24 gw 172.16.21.253
route add default gw 172.16.21.254
route -n
echo 1 > /proc/sys/net/ipv4/ip_forward
echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts
echo 'search netlab.fe.up.pt' > /etc/resolv.conf
echo 'nameserver 172.16.1.1' >> /etc/resolv.conf