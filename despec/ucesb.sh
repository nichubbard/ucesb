#!/bin/bash

while true; do
    echo "Killing previous instances, just in case"
    kill $(pgrep -f ${BASH_SOURCE[0]} | grep -v $$)
    pkill despec
    sleep 1
    kill -9 $(pgrep -f ${BASH_SOURCE[0]} | grep -v $$)
    pkill -9 despec
    echo "Starting ucesb..."
    ./despec stream://x86l-8 --server=trans --watcher=TIMEOUT=2 --aida --time-stitch=wr,20000
    tput setaf 7
    tput setab 1
    echo -ne "UCESB stopped/crashed, restarting in 5 seconds"
    tput sgr0
    echo ""
    echo "If you want to stop this, press CTRL-C now"
    sleep 5
done

