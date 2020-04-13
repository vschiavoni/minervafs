#! /usr/bin/env python3
"""
Basic functional tests for minervafs.
"""
import filecmp
import json
import os
import shutil
import subprocess
import time

import pytest

__WORKSPACE = "./test_workspace"
__MOUNT_POINT = "./test_mountpoint/"
__DEFAULT_FILE_SIZE_IN_BYTES = 4096


def build():
    """
    Compile minervafs code.
    """
    command = "python3 ../waf build".split()
    subprocess.run(command, check=True)


def get_backend_directory():
    """
    Recovers the path to the backend directory.
    Returns:
        str: path to the backend directory
    """
    with open("../minervafs.json") as handle:
        configuration = json.load(handle)
    path = configuration.get("root_folder", "~/.minervafs")
    return os.path.expanduser(path)


def clear_backend_directory():
    """
    Deletes the backend directory.
    """
    path = get_backend_directory()
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

def is_filesystem_ready(directory):
    """
    Tests whether the filesystem is mounted at a given directory and the backend is ready to operate.
    Args:
        directory(str): Path to the directory used as mount point
    Returns:
        bool: True if the file system is ready to operate, false otherwise
    """
    if not os.path.ismount(directory):
        return False
    backend_directory = get_backend_directory()
    indexing_directory = os.path.join(backend_directory, ".indexing")
    backend_configuration = os.path.join(backend_directory, ".minervafs_config")
    return os.path.isdir(backend_directory) and os.path.isdir(indexing_directory) and os.path.isfile(backend_configuration)

def mount(directory):
    """
    Mounts a directory.
    Args:
        directory(str): The directory used as the mountpoint
    Returns:
        subprocess.Popen: Process running the file system daemon
    """
    if not os.path.exists(directory):
        os.makedirs(directory, exist_ok=True)
    elif not os.path.isdir(directory):
        raise ValueError("directory argument is not an actual directory {:s}".format(directory))
    clear_backend_directory()
    proc = subprocess.Popen("../build/examples/minervafs-example/minervafs_example {:s}".format(directory).split())
    proc.wait(timeout=30)
    assert proc.returncode == 0
    while not is_filesystem_ready(directory):
        time.sleep(1)
    time.sleep(1)
    return proc


def umount(directory, proc=None):
    """
    Unmounts a directory.
    Args:
        directory(str): The directory used as the mountpoint
        proc(subprocess.Popen): Process running the file system daemon (default=None)
    """
    if not os.path.isdir(directory):
        raise ValueError("directory argument is not an actual directory {:s}".format(directory))
    if is_filesystem_ready(directory):
        subprocess.run("sync".split(), check=True)
        subprocess.run("fusermount3 -u {:s}".format(directory).split(), check=True)
    if proc:
        proc.kill()


def setup_module():
    build()
    os.makedirs(__WORKSPACE, exist_ok=True)
    os.makedirs(__MOUNT_POINT, exist_ok=True)
    subprocess.run("fusermount3 -u {:s}".format(__MOUNT_POINT).split(), check=False)


def teardown_module():
    shutil.rmtree(__WORKSPACE)
    umount(__MOUNT_POINT)
    shutil.rmtree(__MOUNT_POINT)


@pytest.fixture(scope="function")
def before_every_test(request):
    proc = mount(__MOUNT_POINT)

    def fin():
        umount(__MOUNT_POINT, proc=proc)

    request.addfinalizer(fin)


def test_copy_one_file_in_out(before_every_test):
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__MOUNT_POINT, "in_out.bin")
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__WORKSPACE, os.path.basename(src))
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)


def test_copy_one_file_out_in(before_every_test):
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__WORKSPACE, "out_in.bin")
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__MOUNT_POINT, os.path.basename(src))
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)


def test_copy_one_file_in_in(before_every_test):
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__MOUNT_POINT, "in_in.bin")
    print(os.listdir(__MOUNT_POINT))
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__MOUNT_POINT, "in_in_copy.bin")
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)
