# MinervaFS 

This repository contains the generalised deduplication FUSE based file system developed by researchers at Aarhus University Department of Electrical and Computer Engineering and Neuchatel University Department of Computer Science. 

# License 

This project is open source under the MIT open source software license, see the LICENSE file for more information.  

# Dependencies

- Git 
- Clang 8+ 
- [nlohmann's JSON](https://github.com/nlohmann/json) 
- [libcorrect](https://github.com/quiet/libcorrect)
- openssl
- Python
- FUSE 3.9

To install these dependencies run the install dependencies script locate in `scripts`. 

- For Ubuntu run `./scripts/install-dependencies.sh`
- For Fedora run `./scripts/install-fedora-dependencies.sh`

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

To configure MinervaFS you will need a `minervafs.json`.
The basic version of this file looks like this: 

```json
{
    "root_folder": <PATH_TO_ROOT_OF_PERSISTENT_MFS>,
    "code": {
        <CODE_CONFIG>
    }
}
```

- `root_folder` is the path to were the persistent path of minervaFS should be place. 
The root folder should be an empty folder in your system. A none existing root folder will allow minervaFS to be mounted but not work. 
- `code` is the configuration for the transformation configuration. 

`code` can contain many options and varies depending on the transformation function.
Basic configuration for hamming code with chunks of `4kB` looks like this 

```json
    "code": {
        "m": 15,
        "code_type": 1
    }
```

Additionally minervaFS can be configured with compression capabilities by adding a `"compression": {<compression_config>}` to the configuration format. 
For Gzip with compression level 9 the configuration would look like this 

```json
    "compression": {
        "algorithm": 0,
        "level": 9
    }
```

Example configurations can be found in `./configs`

## Mount mienrvaFS 

The application is compiled into `./build/minervafs`

**A configuration file is need in the directory from where you call the mount and it must be called `minervafs.json`.
The folder `./configs` contains some example configuration files.** 

To mount minervaFS run `~/build/minervafs <MNT_POINT>`. 
Where `<MNT_POINT>` is a folder or device configured with EXT4 as underlying filesystem. 

# Docker 

The system can be built using [Docker](https://www.docker.com/) by running the following command in the top directory.

Please note that in order to pull the dependencies, some of them private, you will need to provide the right ssh keys and configuration.
To achieve this, create a `ssh` directory at the top level of this folder containing a `config` file as well as the public and private key they refer to.
This directory will overwrite the `/root/.ssh` folder inside of the container and enable the build process to pull the dependencies.
The keys must be generated without a key-phrase (a.k.a password) 

The `config` file contains 

```bash
host github.com
    Hostname        github.com
    User            git
    IdentityFile    ~/.ssh/<PRIVATE_KEY>
```

Where `<PRIVATE_KEY>` is the private key you added to the `ssh` folder earlier. 

Then to build the docker image, run: 

```bash
docker build --tag minervafs .
```

This builds a Ubuntu 18.04 image and builds minervaFS as part of the process with the minervaFS project being located in `/tmp/minerva-safefs-layer`. 

To run the container 

```bash
docker run -it --privileged minervafs:latest
```

and if you want to add an external volume: 

```bash
docker run -it --privileged --volume <HOST_PATH>:<CLIENT_PATH> minervafs:latest
```

Where `<HOST_PATH>` is the path to the volume/directory on your host system and `<CLIENT_PATH>` is the path the volume will have in the container.


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



