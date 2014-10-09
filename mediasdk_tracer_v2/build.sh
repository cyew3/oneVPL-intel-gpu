#!/bin/bash

rm -rf __cmake

mkdir __cmake
cd __cmake

mkdir Release
cd Release

mkdir lib
cd lib
#Build Release tracer lib
cmake -DCMAKE_BUILD_TYPE=Release ../../..
make
cd ..

#Build Release config tool
mkdir bin
cd bin
cmake -DCMAKE_BUILD_TYPE=Release ../../../tools/configure
make
cd ../..


mkdir Debug
cd Debug

mkdir lib
cd lib
#Build Debug tracer lib
cmake -DCMAKE_BUILD_TYPE=Debug ../../..
make
cd ..

#Build Debug config tool
mkdir bin
cd bin
cmake -DCMAKE_BUILD_TYPE=Debug ../../../tools/configure
make

echo "Success"

