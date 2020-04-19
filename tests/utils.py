"""
Utility functions that can be reused for tests or other instrumentations
"""
import json
import os
import shutil
import subprocess

__CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
__BASE_DIRECTORY = os.path.realpath(os.path.join(__CURRENT_DIRECTORY, ".."))

def build():
    """
    Compile minervafs code.
    """
    cwd = os.getcwd()
    os.chdir(__BASE_DIRECTORY)
    command = "./waf build".split()
    try:
        subprocess.run(command, check=True)
    finally:
        os.chdir(cwd)


def get_backend_directory(config_file=None):
    """
    Recovers the path to the backend directory.
    Args:
        config_file(str): Path to the config file containing the path to the backend directory (default=None)
    Returns:
        str: path to the backend directory
    """
    if not config_file:
        config_file = os.path.join(__BASE_DIRECTORY, "minervafs.json")
    with open(config_file) as handle:
        configuration = json.load(handle)
    path = configuration.get("root_folder", "~/.minervafs")
    return os.path.expanduser(path)


def clear_backend_directory(path=None, config_file=None):
    """
    Deletes the backend directory.
    Args:
        path(str): Path to the backend directory (default=None)
        config_file(str): Path to the config file containing the path to the backend directory (default=None)
    """
    if not path:
        path = get_backend_directory(config_file)
    if not os.path.isdir(path):
        return
    files = [os.path.join(path, f) for f in os.listdir(path)]
    files = [f for f in files if os.path.isfile(f)]
    for leftover in files:
        os.unlink(leftover)
    if os.path.isdir(path):
        shutil.rmtree(path)
        while os.path.isdir(path):
            pass


def is_filesystem_ready(mountpoint):
    """
    Tests whether the filesystem is mounted at a given directory and
    the backend is ready to operate.
    Args:
        directory(str): Path to the directory used as mount point
    Returns:
        bool: True if the file system is ready to operate, false otherwise
    """
    if not os.path.ismount(mountpoint):
        return False
    backend_directory = get_backend_directory()
    indexing_directory = os.path.join(backend_directory, ".indexing")
    backend_configuration = os.path.join(backend_directory, ".minervafs_config")
    return os.path.isdir(backend_directory) and os.path.isdir(indexing_directory) and \
           os.path.isfile(backend_configuration)


def mount(mountpoint):
    """
    Mounts a directory.
    Args:
        mountpoint(str): The directory used as the mountpoint
    Returns:
        subprocess.Popen: Process running the file system daemon
    """
    if not os.path.exists(mountpoint):
        os.makedirs(mountpoint, exist_ok=True)
    elif not os.path.isdir(mountpoint):
        raise ValueError("directory argument is not an actual directory {:s}".format(mountpoint))
    clear_backend_directory()
    binary = os.path.join(__BASE_DIRECTORY, "build/examples/minervafs-example/minervafs_example")
    if not os.path.isfile(binary):
        raise ValueError("Could not find minervafs binary {:s}".format(binary))
    proc = subprocess.Popen("{:s} {:s}".format(binary, mountpoint).split())
    proc.wait(timeout=30)
    assert proc.returncode == 0
    while not is_filesystem_ready(mountpoint):
        pass
    return proc


def umount(mountpoint, proc=None):
    """
    Unmounts a directory.
    Args:
        mountpoint(str): The directory used as the mountpoint
        proc(subprocess.Popen): Process running the file system daemon (default=None)
    """
    if not os.path.isdir(mountpoint):
        raise ValueError("directory argument is not an actual directory {:s}".format(mountpoint))
    if is_filesystem_ready(mountpoint):
        subprocess.run("sync".split(), check=True)
        subprocess.run("fusermount3 -u {:s}".format(mountpoint).split(), check=True)
    if proc:
        proc.kill()
