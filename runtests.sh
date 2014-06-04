#!/bin/sh

# code by Preston Hamlin

echo
echo "readstuff and writestuff"
echo "demonstrates persistance of data"
./writestuff
./readstuff

echo
echo "readmore"
echo "demonstrates read blocking on empty buffer - please write to the SORT"
./readmore

echo
echo "writemore"
echo "demonstrates write blocking on full buffer - please write to the SORT"
./writemore

echo
echo "concurrent read/write"
echo "demonstrates concurrent access to scullsort - simpler demo also available"
./readc

echo
echo "Demo Complete
