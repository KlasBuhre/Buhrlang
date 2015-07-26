#!/bin/bash
cd compiler
./build.sh
mv bc ..
cd ..
./bc -o test test.b stdlib/*.b
./test
