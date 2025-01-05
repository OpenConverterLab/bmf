#!/bin/bash
# set the environment variables for bmf
export C_INCLUDE_PATH=${C_INCLUDE_PATH}:$(pwd)/output/bmf/include
export CPLUS_INCLUDE_PATH=${CPLUS_INCLUDE_PATH}:$(pwd)/output/bmf/include
export LIBRARY_PATH=${LIBRARY_PATH}:$(pwd)/output/bmf/lib
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$(pwd)/output/bmf/lib

# only set if you want to use BMF in python
export PYTHONPATH=$(pwd)/output/bmf/lib:$(pwd)/output
export DYLD_LIBRARY_PATH=~/Documents/Programs/Git/Github/bmf/output/bmf/lib
