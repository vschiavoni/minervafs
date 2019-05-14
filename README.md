# minerva-safefs-layer
This repository contains a layer for the safeFS file system developed at neuchatel Univeristy

# SafeFS 

SafeFS is a software defined file system which is highly modular for reference see:  [github.com/safecloud-project/safefs](https://github.com/safecloud-project/safefs)

# Build 

We build both a shared and static version of this library, just build using the command 

```bash 
python waf configure build 
```

after first configuration 

```bash 
python waf build 
```

## Configuration 

allow other users in `/etc/fuse.conf`
