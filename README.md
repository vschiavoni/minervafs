# minerva-safefs-layer
This repository contains a layer for the safeFS file system developed at neuchatel University

# SafeFS 

SafeFS is a software defined file system which is highly modular for reference see:  [github.com/safecloud-project/safefs](https://github.com/safecloud-project/safefs)

# Dependencies

* Clang 8+
* [nlohmann's JSON](https://github.com/nlohmann/json) 
* Python
* FUSE 2

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
