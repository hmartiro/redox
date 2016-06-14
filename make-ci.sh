#!/usr/bin/env bash
# Build script for continuous integration
set -ev
env | sort

# Install packages
sudo apt-get update
sudo apt-get install -y libhiredis-dev libev-dev libgtest-dev redis-server

# Make gtest
git clone https://github.com/google/googletest
cd googletest
mkdir -p build
cd build
cmake ..
make
sudo mv googlemock/gtest/libg* /usr/local/lib/
cd ../..
rm -rf googletest

# Make redox
mkdir -p build
cd build
cmake -Dexamples=ON -Dlib=ON -Dstatic_lib=ON -Dtests=ON ..
time make
./test_redox
cd ..
