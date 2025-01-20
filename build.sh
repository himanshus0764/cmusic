#!/bin/bash
#
set -xe
PROJECT_DIR=`pwd`
APP_NAME="music_player"
CC="gcc"

CFLAGS="-lGL -lm -lpthread -ldl -lrt -lX11"
$CC  -o $PROJECT_DIR/$APP_NAME $PROJECT_DIR/main.c ./libraylib.a ./libfftw3.a ./libfftw3f.a -I. $CFLAGS
# ./music_player /home/laptop_asus/Music/music
./music_player /home/laptop_asus/Downloads
