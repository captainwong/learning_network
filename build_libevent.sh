#/bin/bash

mkdir -p build/libevent
cd build/libevent

CC=g++
CFLAGS='-std=c++11 '
CFLAGS+=`pkg-config --cflags --libs libevent`
FOLDER='../../libevent'

name=000.discard
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=000.discard_client
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=001.daytime
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=002.time
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=002.time_client
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=002.time_client_libevent
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=003.echo
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=004.chargen
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=005.pingpong
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=005.pingpong_client
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=006.chat
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=007.chat_with_codec
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 

name=007.chat_with_codec_client
$CC ${FOLDER}/${name}/${name}.cpp $CFLAGS -o $name 
