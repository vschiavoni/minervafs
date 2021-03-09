# MinervaFS 

This repository contains the generalised deduplication FUSE based file system developed by researchers at Aarhus University Department of Electrical and Computer Engineering and Neuchatel University Department of Computer Science. 

# Dependencies

- Git 
- Clang 8+ 
- [nlohmann's JSON](https://github.com/nlohmann/json) 
- [libcorrect](https://github.com/quiet/libcorrect)
- openssl
- Python
- FUSE 3.9

To install these dependencies run the install dependencies script locate in `scripts`. 

- For Ubuntu run `sh scripts/install-dependencies.sh`
- For Fedora run `sh scripts/install-fedora-dependencies.sh`

# Build 

For build system we use `waf` which is include in the project and it require python3+.

## Configure - Before first build

To setup external dependencies not include by the install dependencies script and generally setting up the project, we need to execute a configuration command. 


```bash
python waf configure
```

## Build 

To build the project run

```bash
python waf build

```

## Build after dependency upgrade

If a dependency in `resolve.json` has been update. Run 

```bash 
python waf configure build
```

# Run MinervaFS

## Configure FUSE 

To enable mounting and umount for all use you need to enable `allow other users` in `/etc/fuse.conf`

## Configure MinervaFS

**TBW**

## Mount mienrvaFS 

The application is compiled into `./build/examples/minervafs-example/minervafs_example`

A configuration file is need in the directory from where you call the mount and it must be called `minervafs.json`.
The folder `./configs` contains some example configuration files. 

To mount minervaFS run `~/build/examples/minervafs-example/minervafs_example <MNT_POINT>`. 
Where `<MNT_POINT>` is a folder or device configured with EXT4 as underlying filesystem. 

# Run Experiments 

## Physical Storage Usage

The script for this is located in `./experiment-scripts/python/run_physical_experiment.py`.
The script takes a JSON configuration file as input. 

The configuration file follows this format:

```json
{
    "file_list": <PATH_TO_FILE_LIST_OF_FILE>,
    "output_dir": <MNT_POINT>,
    "minerva_dir": <MINERVA_FS_DIR>,
    "interval": <INTERVAL>,
    "result_file": <PATH_TO_RESULT_FILE>
}
```

- `file_list` is a file contain a list of files to be stored in the minervaFS mount point. The file should be seperated by `\n`
- `output_dir` is the mount point of minervaFS 
- `minerva_dir` is the root folder configured in the `minervafs.json` file before mounting minervaFS
- `interval` is the number of files that will be run before the `du` command will be execture 
- `result_file` is the path to the result file in CSV format



# OLD README
# minerva-safefs-layer
This repository contains a layer for the safeFS file system developed at neuchatel University

# SafeFS 

SafeFS is a software defined file system which is highly modular for reference see:  [github.com/safecloud-project/safefs](https://github.com/safecloud-project/safefs)

# Dependencies

* git
* Clang 8+
* [nlohmann's JSON](https://github.com/nlohmann/json) 
* [libcorrect](https://github.com/quiet/libcorrect)
* openssl
* Python
* FUSE 3.9

A dependency install script is available in `scripts/install-dependencies.sh` for Ubuntu 18.04.

# Build 

## First build

Update the submodules to build the layer.

```bash
git submodule init
git submodule update
```

Ensure you have all the necessary dependencies to build by running:
```bash
python waf configure
```

We build both a shared and static version of this library, just build using the command:

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
