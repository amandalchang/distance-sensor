#!/bin/bash
pushd .
cd ~/esp/esp-idf
source export.sh
popd
idf.py set-target esp32
idf.py build