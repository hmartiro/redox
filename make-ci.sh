#!/usr/bin/env bash
set -e
sudo apt-get install -y libhiredis-dev libev-dev
mkdir -p build
cd build
cmake -Dexamples=ON -Dlib=ON -Dstatic_lib=ON -Dtests=ON ..
time make
./test_redox
cd ..
