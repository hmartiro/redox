#!/usr/bin/env bash
# Build script for continuous integration
set -ev
env | sort

# Install packages
sudo apt-get update
sudo apt-get install -y libhiredis-dev libev-dev libgtest-dev redis-server

# Clean
rm -rf build
mkdir build
cd build

# Make gtest
git clone https://github.com/google/googletest
cd googletest
mkdir build
cd build
cmake -DBUILD_GMOCK=OFF -DBUILD_GTEST=ON -DBUILD_SHARED_LIBS=ON ..
time make -j2
cd ../..
sudo cp googletest/build/googletest/libg* /usr/local/lib/
sudo cp -r googletest/googletest/include/gtest /usr/local/include

# Make redox
cmake -Dexamples=ON -Dlib=ON -Dstatic_lib=ON -Dtests=ON ..
time make -j2 VERBOSE=1

# Test redox
sudo service redis-server restart
./test_redox
cd ..
