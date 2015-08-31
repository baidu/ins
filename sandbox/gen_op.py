#!/usr/bin/python
import itertools, string

f = open('op1.txt', 'w')
for i in xrange(1000):
    key = '%03d' % i
    value = ''.join(key for j in range(3))
    f.write('put %s %s\n' % (key, value))

f.write('logout\n')
f.close()

f = open('op2.txt', 'w')
for word in map(''.join, itertools.product(string.ascii_lowercase, repeat=3)):
    key = word
    value = ''.join(word for j in range(3))
    f.write('put %s %s\n' % (key, value))

f.write('logout\n')
f.close()

