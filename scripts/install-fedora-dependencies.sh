#!/usr/bin/env bash

# Install dependencies for Fedora 32 and 33 
install fedora_33_and_32 () {

    dnf upgrade -y &&\
    dnf install -y -quiet git fuse3 fuse3-devel openssl-devel clang &&\
    git clone git@github.com:nlohmann/json.git &&\
    cp -r json/include/nlohman /usr/local/include &&\
    
    
}

if ["$1" == ""]; then
    echo "usage <fedora version>"
    echo "You must provide a version"
elif [$1 -lt 32 ]; then
    echo "Version must be minimum 32"
else
    install_fedora_33_and_32()
fi
