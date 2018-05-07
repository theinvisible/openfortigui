#!/bin/bash

# Caution - This script is not guaranteed to work properly, if any complications occur. 
# In case of doubt, execute these steps one after one and watch the output.

make clean
qmake
make -j4

cp krunner_openfortigui.so ~/.local/lib/qt/plugins
cp plasma-runner-openfortigui.desktop ~/.local/share/kservices5

# Restart krunner for the changes to take effect
kquitapp5 krunner 2> /dev/null
#kstart5 --windowclass krunner krunner > /dev/null 2>&1 & 
kstart5 --windowclass krunner krunner
