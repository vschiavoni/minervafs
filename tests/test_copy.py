#! /usr/bin/env python3
"""
Basic functional tests for minervafs.
"""
import filecmp
import os
import shutil
import time

__WORKSPACE = "./test_workspace"
__MOUNT_POINT = "./test_mountpoint/"

def build():
    os.system("python3 ../waf build")

def mount(directory):
    """
    Mounts a directory.
    Args:
        directory(str): The directory used as the mountpoint
    """
    if not os.path.exists(directory):
        os.makedirs(directory)
    elif not os.path.isdir(directory):
        raise ValueError("directory argument is not an actual directory {:s}".format(directory))
    os.system("python3 ../clear.py")
    os.system("../build/examples/minervafs-example/minervafs_example {:s}".format(directory))

def umount(directory):
    """
    Unmounts a directory.
    Args:
        directory(str): The directory used as the mountpoint
    """
    if not os.path.isdir(directory):
        raise ValueError("directory argument is not an actual directory {:s}".format(directory))
    os.system("fusermount3 -u {:s}".format(directory))

def setup_module(module):
    build()
    os.makedirs(__WORKSPACE, exist_ok=True)
    mount(__MOUNT_POINT)

def teardown_module(module):
    umount(__MOUNT_POINT)

def test_copy_one_file_out_in():
    data = os.urandom(16)
    src = os.path.join(__WORKSPACE, "16B.data")
    with open(src, "wb") as handle:        
        handle.write(data)
    dst = os.path.join(__MOUNT_POINT, os.path.basename(src))
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)

def test_copy_one_file_in_in():
    data = os.urandom(16)
    src = os.path.join(__MOUNT_POINT, "16B.data")
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__MOUNT_POINT, "16B.data.copy")
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)

def test_copy_one_file_in_out():
    data = os.urandom(16)
    src = os.path.join(__MOUNT_POINT, "16B.data")
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__WORKSPACE, "16B.data.out")
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)