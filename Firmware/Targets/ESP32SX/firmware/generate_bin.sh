#!/bin/sh

set -u
set -e


merge_bin(){
    esptool.py --chip ${IDF_TARGET} merge_bin -o $1 --flash_mode dio --flash_size 4MB \
    0x1000 build/bootloader/bootloader.bin \
    0x10000 build/openffboard.bin \
    0x8000 build/partition_table/partition-table.bin
}

build_and_generate_bin(){
    cd ../
    idf.py fullclean
    rm -f sdkconfig sdkconfig.old
    idf.py build
    merge_bin firmware/openffb-${IDF_TARGET}-hw_${HW_VER}.bin
    cd firmware
}

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $SCRIPT_DIR
rm -f ../firmware/*.bin

export IDF_TARGET=esp32s2
export EXTRA_CFLAGS="-DOPENFFBOARD_ESP32S2_V1_0"
export EXTRA_CXXFLAGS="-DOPENFFBOARD_ESP32S2_V1_0"
HW_VER="v1.0"
build_and_generate_bin

export IDF_TARGET=esp32s2
export EXTRA_CFLAGS="-DOPENFFBOARD_ESP32S2_V1_1"
export EXTRA_CXXFLAGS="-DOPENFFBOARD_ESP32S2_V1_1"
HW_VER="v1.1"
build_and_generate_bin

export IDF_TARGET=esp32s3
export EXTRA_CFLAGS="-DOPENFFBOARD_ESP32S2_V1_1"
export EXTRA_CXXFLAGS="-DOPENFFBOARD_ESP32S2_V1_1"
HW_VER="v1.1"
build_and_generate_bin





