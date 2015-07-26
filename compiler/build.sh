#!/bin/bash
g++ -Wall -std=c++11 -g -rdynamic -c *.cpp
g++ -o bc *.o
rm *.o
