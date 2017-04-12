#!/usr/bin/python

import subprocess as sub
from datetime import datetime
window  = 40
epsilon = 10

on  = window / 2

victim = "10.104.113.15"
gateway = "10.104.113.14"

filt = "port 22 and src " + victim + " and dst " + gateway + " and tcp[13] & 8 != 0"

p = sub.Popen(('sudo', 'tcpdump', '-l', '-ttt', filt), stdout=sub.PIPE)
for l in iter(p.stdout.readline, b''):
    time = datetime.strptime((l.rstrip().split()[0])[:15], '%H:%M:%S.%f')
    millis = (time.second * 1000) + (time.microsecond / 1000)
    if millis == 0:
        continue
    code = millis % window
    if on - epsilon < code and code < on + epsilon:
        print 1,
    else:
        print 0,
