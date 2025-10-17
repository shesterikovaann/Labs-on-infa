#!/bin/bash
for num in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do
echo $num
done
for n in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do
num=$((n % 2))
if [ $num -eq 0 ]; then
    echo $n
fi
done
