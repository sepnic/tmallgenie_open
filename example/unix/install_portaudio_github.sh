#!/bin/bash

# This script attempts to install PortAudio, which can grap a live audio stream
# from the soundcard.
#
# On linux systems, we only build with ALSA, so make sure you install it using
# e.g.:
#   sudo apt-get -y install libasound2-dev

echo "Installing portaudio"

if [ ! -e portaudio ]; then
  git clone https://github.com/PortAudio/portaudio.git || exit 1
fi

cd portaudio

cmake -DCMAKE_INSTALL_PREFIX=install
make
make install

cd ..
