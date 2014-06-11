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
./readstuff

echo
echo "writemore"
echo "demonstrates write blocking on full buffer - please read from the SORT"
./writestuff
./writestuff
./writestuff
./writestuff
./writestuff
./writestuff
./writestuff
./writestuff

#echo
#echo "concurrent read/write"
#echo "demonstrates concurrent access to scullsort - simpler demo also available"
#readc

echo
echo "Demo Complete"
