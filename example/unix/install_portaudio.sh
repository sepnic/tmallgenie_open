#!/bin/bash

# This script attempts to install PortAudio, which can grap a live audio stream
# from the soundcard.
#
# On linux systems, we only build with ALSA, so make sure you install it using
# e.g.:
#   sudo apt-get -y install libasound2-dev

echo "Installing portaudio"

if [ ! -e pa_stable_v190700_20210406.tgz ]; then
  wget -T 10 -t 3 \
    http://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz || exit 1;
fi

if [ ! -e portaudio ]; then
  tar -xovzf pa_stable_v190700_20210406.tgz || exit 1
fi

cd portaudio

MACOS=`uname 2>/dev/null | grep Darwin`
if [ -z "$MACOS" ]; then
  ./configure --without-jack --without-oss \
    --with-alsa --prefix=`pwd`/install --with-pic || exit 1;
else
  # People may have changed OSX's default configuration -- we use clang++.
  CC=clang CXX=clang++ ./configure --prefix=`pwd`/install --with-pic
fi

make
make install

cd ..
