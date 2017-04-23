#!/bin/bash

export PATH=$HOME/os161/tools/bin:$PATH
mkdir -p $HOME/os161/root

# make sure $HOME/os161/src pints to your os161 source code tree
# mv <path-to-oslight-source-tree> $HOME/os161/src
# cd $HOME/os161/src

# configure userland tools
./configure --ostree=$HOME/os161/root

cd userland
bmake includes
bmake depend
bmake
bmake install

# uncomment the following if you need to update the includes and dependencies
cd ..
bmake includes
bmake depend
bmake
bmake install

# configure kernel with the sample DUMBVM configuration
cd kern/conf/
./config ASST2

# compile kernel with the DUMBVM configuration
cd ../compile/ASST2
bmake depend
bmake
bmake install
