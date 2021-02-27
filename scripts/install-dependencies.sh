#! /usr/bin/env bash
###############################################################################
## Install the dependencies needed to run this project
##
## Tested on Ubuntu 18.04 installed using Ubuntu MAAS and Docker
###############################################################################
install_ubuntu_1804 () {
    # Update an install base dependencies
    apt-get update && \
    apt-get dist-upgrade --yes && \
    apt-get install build-essential cmake curl git g++ libssl-dev meson pkg-config python udev wget xz-utils zlib1g-dev --yes --quiet

    # Install Fuse 3.9.1
    local cwd="${pwd}"
    cd /tmp/
    wget https://github.com/libfuse/libfuse/releases/download/fuse-3.9.1/fuse-3.9.1.tar.xz --quiet
    tar xaf /tmp/fuse-3.9.1.tar.xz
    cd fuse-3.9.1
    mkdir -p build
    cd build
    meson ..
    ninja && ninja install || exit
    cd "${cwd}"

    # Install specific version of clang
    echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main" | tee /etc/apt/sources.list.d/llvm.list && \
    echo "deb-src http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main" | tee --append /etc/apt/sources.list.d/llvm.list && \
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    apt-get update && \
    apt-get install clang-8 libclang-8-dev libclang-common-8-dev libstdc++-8-dev --yes --quiet && \
    update-alternatives --install /usr/bin/c++     c++     /usr/bin/clang++-8 100 &&\
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100 &&\
    update-alternatives --install /usr/bin/cc      cc      /usr/bin/clang-8 100 &&\
    update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-8 100

    # nlohmann-json
    if [ ! -d /tmp/nlohmann-json ]; then
        git clone https://github.com/nlohmann/json /tmp/nlohmann-json --quiet
    fi
    cd /tmp/nlohmann-json || exit
    git checkout v3.6.1 --quiet
    mkdir -p build
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

    # libcorrect
    if [ ! -d /tmp/libcorrect ]; then
        git clone https://github.com/quiet/libcorrect /tmp/libcorrect --quiet
    fi
    cd /tmp/libcorrect || exit
    git pull --all --quiet
    mkdir -p build &&\
    cd build &&\
    cmake .. &&\
    make &&
    make install
    ldconfig
}

install_ubuntu_2004() {
    echo "Installing on 20.04"
    export ENV DEBIAN_FRONTEND=noninteractive
    # Update an install base dependencies
    apt-get update && \
    apt-get dist-upgrade --yes && \
    apt-get install build-essential cmake curl git g++ libfuse3-dev libssl-dev lsb-release meson pkg-config python software-properties-common udev wget xz-utils zlib1g-dev --yes --quiet
    
    # Install clang
    # Install specific version of clang
    echo "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-10 main" | tee /etc/apt/sources.list.d/llvm.list && \
    echo "deb-src http://apt.llvm.org/focal/ llvm-toolchain-focal-10 main" | tee --append /etc/apt/sources.list.d/llvm.list && \
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    apt-get update && \
    apt-get install clang-10 libclang-10-dev libclang-common-10-dev libstdc++-10-dev --yes --quiet && \
    update-alternatives --install /usr/bin/c++     c++     /usr/bin/clang++-10 100 &&\
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-10 100 &&\
    update-alternatives --install /usr/bin/cc      cc      /usr/bin/clang-10 100 &&\
    update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-10 100

    

    # nlohmann-json
    apt-get install nlohmann-json3-dev
    ldconfig

    # libcorrect
    if [ ! -d /tmp/libcorrect ]; then
        git clone https://github.com/quiet/libcorrect /tmp/libcorrect --quiet
    fi
    cd /tmp/libcorrect || exit
    git pull --all --quiet
    mkdir -p build &&\
    cd build &&\
    cmake .. &&\
    make &&
    make install
    ldconfig
}

main () {
    # Check if root
    if [[ "$(id -u)" -ne 0 ]]; then
        echo "This script must run by root or a sudoer" >&2
        exit 0
    fi

    if [[ ! -f /etc/lsb-release || "$(grep --count 'DISTRIB_ID=Ubuntu' /etc/lsb-release)" -eq 0 ]]; then
        echo "This script is meant to run on Ubuntu" >&2
        exit 0
    fi
    if [[ "$(grep --count 'DISTRIB_RELEASE=18.04' /etc/lsb-release)" -eq 1 ]]; then
        install_ubuntu_1804
    fi
    if [[ "$(grep --count 'DISTRIB_RELEASE=20.04' /etc/lsb-release)" -eq 1  ]]; then
        install_ubuntu_2004
    fi
}

main "${@}"
