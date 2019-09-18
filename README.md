# minerva-safefs-layer
This repository contains a layer for the safeFS file system developed at neuchatel University

# SafeFS 

SafeFS is a software defined file system which is highly modular for reference see:  [github.com/safecloud-project/safefs](https://github.com/safecloud-project/safefs)

# Dependencies

* Clang 8+
* [nlohmann's JSON](https://github.com/nlohmann/json) 
* [crypto++](https://github.com/weidai11/cryptopp)
* openssl
* Python
* FUSE 2

A dependency install script is available in `scripts/install-dependencies.sh` for Ubuntu 18.04.

# Build 

## First build

Update the submodules to build the layer.

```bash
git submodule update
```

We build both a shared and static version of this library, just build using the command 

```bash 
python waf configure build 
```

after first configuration 

```bash 
python waf build 
```

## Build after a dependency upgrade

1. Delete the `build` and the `.waf*` directories
2. Run `waf configure build`

## Configuration 

allow other users in `/etc/fuse.conf`

## Docker

The system can be built using [Docker](https://www.docker.com/) by running the following command in the top directory.
```bash
docker build --tag minervafs .
```
Please note that in order to pull the dependencies, some of them private, you will need to provide the right ssh keys and configuration.
To achieve this, create a `ssh` directory at the top level of this folder containing a `config` file as well as the public and private key they refer to.
This directory will overwrite the `/root/.ssh` folder inside of the container and enable the build process to pull the dependencies.
