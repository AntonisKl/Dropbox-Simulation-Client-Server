#!/bin/bash

if [[ "$#" -ne 5 ]]; then
    echo "Illegal number of parameters";  exit 1
fi

clientsNum=$1
delay=$2
workerThreadsNum=$3
bufferSize=$4
serverPort=$5

make clean && make

sleep 2

portNum=141
for ((i=1; i<=$clientsNum; i++));
do
    inputDirNum=$((($i - 1)%2 + 1))
    gnome-terminal --window-with-profile=test --working-directory=/home/antonis/Documents/syspro3/ -e "./exe/dropbox_client -d inputDir$inputDirNum -p $portNum -w $workerThreadsNum -b $bufferSize -sp $serverPort -sip 192.168.1.20"
    echo "port $portNum";
    ((portNum++))
    sleep $delay
done