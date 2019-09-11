#! /usr/bin/env bash
###############################################################################
## Install the dependencies needed to run this project
##
## Tested on Ubuntu 19.04 installed using Ubuntu MAAS
###############################################################################

main () {
    # Check if root
    if [[ "$(id -u)" -ne 0 ]]; then
        echo "This script must run by root or a sudoer" >&2
        exit 0
    fi
    # Update an install base dependencies
    apt-get update && \
    apt-get dist-upgrade --yes && \
    apt-get install build-essential cmake curl git g++ libfuse2 libfuse-dev libssl-dev python wget xz-utils --yes --quiet

    # Install specific version of clang
    echo "deb http://apt.llvm.org/disco/ llvm-toolchain-disco-8 main" | tee /etc/apt/sources.list.d/llvm.list && \
    echo "deb-src http://apt.llvm.org/disco/ llvm-toolchain-disco-8 main" | tee --append /etc/apt/sources.list.d/llvm.list && \
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    apt-get update && \
    apt-get install clang-8 libclang-8-dev libclang-common-8-dev --yes --quiet && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-8 100 &&\
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100

    # nlohmann-json
    if [ ! -d /tmp/nlohmann-json ]; then
        git clone https://github.com/nlohmann/json /tmp/nlohmann-json --quiet
    fi
    cd /tmp/nlohmann-json || exit
    git checkout v3.6.1 --quiet
    mkdir build
    cd build || exit
    cmake ..
    make
    make install
    ldconfig
    # cryptopp
    if [ ! -d /tmp/cryptopp ]; then
        git clone https://github.com/weidai11/cryptopp /tmp/cryptopp --quiet
    fi
    cd /tmp/cryptopp || exit
    git checkout CRYPTOPP_8_2_0 --quiet
    make
    make install
    ldconfig
}

main "${@}"
