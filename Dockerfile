FROM ubuntu:20.04

# install base dependencies
COPY scripts/install-dependencies.sh .
RUN chmod +x ./install-dependencies.sh && \
    ./install-dependencies.sh

RUN mkdir -p ~/.ssh/
COPY ssh ~/.ssh
RUN ssh-keyscan -t rsa github.com >> ~/.ssh/known_hosts && \
    git config --global url."git@github.com:".insteadOf "https://github.com/" 
COPY . /tmp/minerva-safefs-layer/
COPY ssh /root/.ssh
WORKDIR  /tmp/minerva-safefs-layer
RUN ldconfig
RUN ./waf configure
RUN ./waf build
