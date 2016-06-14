#!/usr/bin/env bash
set -e
sudo apt-get install libhiredis-dev libev-dev
mkdir -p build
cd build
cmake ..
time make
cd ..
