#! /usr/bin/env python3
"""
Basic functional tests for minervafs.
"""
import filecmp
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


def mount(directory):
    """
    Mounts a directory.
    Args:
        directory(str): The directory used as the mountpoint
    """
    if not os.path.exists(directory):
        os.makedirs(directory, exist_ok=True)
    elif not os.path.isdir(directory):
        raise ValueError("directory argument is not an actual directory {:s}".format(directory))
    subprocess.run("python3 ../clear.py".split(), check=True)
    subprocess.run("../build/examples/minervafs-example/minervafs_example {:s}".format(directory).split(), check=True)
    while not os.path.ismount(directory):
        time.sleep(1)
    time.sleep(1)


def umount(directory):
    """
    Unmounts a directory.
    Args:
        directory(str): The directory used as the mountpoint
    """
    if not os.path.isdir(directory):
        raise ValueError("directory argument is not an actual directory {:s}".format(directory))
    if os.path.ismount(directory):
        subprocess.run("fusermount3 -u {:s}".format(directory).split(), check=True)


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
    mount(__MOUNT_POINT)

    def fin():
        umount(__MOUNT_POINT)
    request.addfinalizer(fin)


def test_copy_one_file_out_in(before_every_test):
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__WORKSPACE, "16B.data")
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__MOUNT_POINT, os.path.basename(src))
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)


def test_copy_one_file_in_in(before_every_test):
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__MOUNT_POINT, "16B.data")
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__MOUNT_POINT, "16B.data.copy")
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)


def test_copy_one_file_in_out(before_every_test):
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__MOUNT_POINT, "16B.data")
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__WORKSPACE, "16B.data.out")
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)
