#!/usr/bin/env bash
mkdir -p build &&
cd build &&
cmake ..  &&
time make &&
cd ..
