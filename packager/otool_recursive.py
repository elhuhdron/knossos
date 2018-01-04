#!/usr/bin/env python3
import sys
import subprocess

#print(subprocess.__file__)

def otool(s):
    out = subprocess.getoutput("otool -L " + s).splitlines()
    for l in out:
        if l[0] == '\t':
            yield l.split(' ', 1)[0][1:]

#need = set(['/Applications/iTunes.app/Contents/MacOS/iTunes'])
need = set([sys.argv[1:][0]])
done = set()

while need:
    needed = set(need)
    need = set()
    for f in needed:
        need.update(otool(f))
    done.update(needed)
    need.difference_update(done)

for f in sorted(done):
    print(f)
