FROM ubuntu:19.04


# install base dependencies
RUN apt-get update && \
    apt-get dist-upgrade --yes && \
    apt-get install build-essential cmake curl git g++ libfuse2 libfuse-dev python wget xz-utils --yes --quiet

# clang

RUN echo "deb http://apt.llvm.org/disco/ llvm-toolchain-disco-8 main" | tee /etc/apt/sources.list.d/llvm.list && \
    echo "deb-src http://apt.llvm.org/disco/ llvm-toolchain-disco-8 main" | tee --append /etc/apt/sources.list.d/llvm.list && \
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    apt-get update && \
    apt-get install clang-8 libclang-8-dev libclang-common-8-dev --yes --quiet && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-8 100 &&\
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100


# nlohmann-json
RUN git clone https://github.com/nlohmann/json /tmp/nlohmann-json --quiet
WORKDIR /tmp/nlohmann-json
RUN git checkout v3.6.1 --quiet && \
    mkdir build && cd build && \
    cmake .. && \
    make && \
    make install && \
    ldconfig
# cryptopp
RUN git clone https://github.com/weidai11/cryptopp /tmp/cryptopp --quiet
WORKDIR /tmp/cryptopp
RUN git checkout CRYPTOPP_8_2_0 --quiet && \
    make && \
    make install && \
    ldconfig

RUN mkdir -p ~/.ssh/
COPY ssh/* ~/.ssh/
RUN ssh-keyscan -t rsa github.com >> ~/.ssh/known_hosts && \
    git config --global url."git@github.com:".insteadOf "https://github.com/" 
COPY . /tmp/minerva-safefs-layer/
COPY scripts/config scripts/sshkey scripts/sshkey.pub /root/.ssh/
WORKDIR  /tmp/minerva-safefs-layer
RUN ./waf configure build


