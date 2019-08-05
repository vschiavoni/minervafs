FROM ubuntu:19.04


# install base dependencies
COPY scripts/install-dependencies.sh .
RUN chmod +x ./install-dependencies.sh && \
    ./install-dependencies.sh

RUN mkdir -p ~/.ssh/
COPY ssh/* ~/.ssh/
RUN ssh-keyscan -t rsa github.com >> ~/.ssh/known_hosts && \
    git config --global url."git@github.com:".insteadOf "https://github.com/" 
COPY . /tmp/minerva-safefs-layer/
COPY scripts/config scripts/sshkey scripts/sshkey.pub /root/.ssh/
WORKDIR  /tmp/minerva-safefs-layer
RUN ./waf configure build


