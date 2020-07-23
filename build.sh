#/bin/bash

mkdir -p build
cd build

CC=g++
CFLAGS='-std=c++11 '
CFLAGS+=`pkg-config --cflags --libs libevent`

name=000.discard
$CC ../${name}/${name}.cpp $CFLAGS -o $name 


name=001.daytime
$CC ../${name}/${name}.cpp $CFLAGS -o $name 


name=002.time
$CC ../${name}/${name}.cpp $CFLAGS -o $name 


name=002.time_client
$CC ../${name}/${name}.cpp $CFLAGS -o $name 

