#!/bin/bash

# for better readability, rather than the console, use a file:
# ./tests.sh &> out.txt

target="tpcc"

make -f makefile -C ../ ./bin/$target --silent

exec="../bin/$target -O"

printf "Current directory: $(pwd)\n\n"

GOODVAL=0
SYNVAL=1
SEMVAL=2
MISSVAL=3

TOTALGOOD=0
TOTALBAD=0
TOTAL=0
GOOD=0
BAD=0
GOODTREE=0
RESULT=0

printf "Testing syntax errors bad files\n"
for file in ./syn-err/*.TPC ./syn-err/*.tpc; do \
    if [ ! -f $file ]; then continue; fi;
    TOTALBAD=$(($TOTALBAD + 1))
    printf "$(basename "$file") "
    $exec -d < $file
    RESULT=$?
    if [ ! $RESULT == $SYNVAL ]; then printf "oops\n"; else BAD=$(($BAD + 1)); fi;
done

printf "\nTesting semantic errors bad files\n"
for file in ./sem-err/*.TPC ./sem-err/*.tpc; do \
    if [ ! -f $file ]; then continue; fi;
    TOTALBAD=$(($TOTALBAD + 1))
    printf "$(basename "$file") "
    $exec < $file
    RESULT=$?
    # echo $RESULT
    if [ ! $RESULT == $SEMVAL ]; then printf "oops\n"; else BAD=$(($BAD + 1)); fi;
done

printf "\nTesting good files\n"
for file in ./good/*.TPC ./good/*.tpc; do \
    if [ ! -f $file ]; then continue; fi;
    TOTALGOOD=$(($TOTALGOOD + 1))
    printf "$(basename "$file") "
    $exec < $file
    RESULT=$?
    if [ ! $RESULT == $GOODVAL ]; then printf "oops"; else GOOD=$(($GOOD + 1)); fi;
    printf "\n"
done

printf "\nTesting good files with trees\n"
for file in ./good/*.TPC ./good/*.tpc; do \
    if [ ! -f $file ]; then continue; fi;
    echo $(basename "$file")
    $exec -td < $file
    RESULT=$?
    if [ ! $RESULT == $GOODVAL ]; then printf "oops"; else GOODTREE=$(($GOODTREE + 1)); fi;
    printf "\n"
done

TOTAL=$(($TOTALGOOD + $TOTALBAD))

printf "\n"
printf "$BAD/$TOTALBAD bad files passed\n"
printf "$GOOD/$TOTALGOOD good files passed with no trees\n"
printf "$GOODTREE/$TOTALGOOD good files passed with trees\n"

min=$GOOD
if [ $GOODTREE -lt $min ]; then min=$GOODTREE; fi;

# TMP=$(($min + $BAD))

printf "\n"
printf "$((($min + $BAD) * 100 / $TOTAL))%% of files passed\n"

rm _anonymous.asm
