#!/bin/bash
max=300
ip=localhost
filename=a.gz
port=5000
tempo=2


for (( i=1; i<=max; ++i )) do
    echo "Request $i of $max"
    curl -s "$ip:$port/$filename" > /dev/null

done
