#! /usr/bin/env python3
"""
Basic functional tests for minervafs.
"""
import filecmp
import os
import random
import shutil
import subprocess
import string

import pytest

from utils import build, mount, umount


__WORKSPACE = "./test_workspace"
__MOUNT_POINT = "./test_mountpoint/"
__DEFAULT_FILE_SIZE_IN_BYTES = 4096


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
    os.system("echo 3 | sudo tee /proc/sys/vm/drop_caches")
    proc = mount(__MOUNT_POINT)

    def fin():
        umount(__MOUNT_POINT, proc=proc)

    request.addfinalizer(fin)


def test_read_non_existing_file(before_every_test):
    """
    Tests whether trying to read a non existing file raises an error.
    """
    src = os.path.join(__MOUNT_POINT, "non_existing")
    with pytest.raises(FileNotFoundError):
        open(src)

def test_read_an_empty_file(before_every_test):
    """
    Tests whether we can create an empty file and find it afterwards.
    """
    src = os.path.join(__MOUNT_POINT, "empty")
    with open(src, "w") as _:
        pass
    assert os.path.isfile(src)
    assert 0 == os.path.getsize(src)
    with open(src, "r") as handle:
        data = handle.read()
    assert 0 == len(data)

def test_read_a_text_file(before_every_test):
    """
    Tests whether we can read a text file we previously wrote to.
    """
    data = "".join(random.choice(string.ascii_letters) for i in range(__DEFAULT_FILE_SIZE_IN_BYTES))
    src = os.path.join(__MOUNT_POINT, "data.txt")
    with open(src, "w") as handle:
        handle.write(data)
    assert os.path.isfile(src)
    assert __DEFAULT_FILE_SIZE_IN_BYTES == os.path.getsize(src)
    with open(src, "r") as handle:
        recovered = handle.read()
    assert data == recovered

def test_read_a_binary_file(before_every_test):
    """
    Tests whether we can read a binary file we previously wrote to.
    """
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__MOUNT_POINT, "data.bin")
    with open(src, "wb") as handle:
        handle.write(data)
    assert os.path.isfile(src)
    assert __DEFAULT_FILE_SIZE_IN_BYTES == os.path.getsize(src)
    with open(src, "rb") as handle:
        recovered = handle.read()
    assert data == recovered


def test_copy_one_file_in_out(before_every_test):
    """
    Tests whether we can copy a file from the mounpoint to a folder outside.
    """
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__MOUNT_POINT, "in_out.bin")
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__WORKSPACE, os.path.basename(src))
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)


def test_copy_one_file_in_in(before_every_test):
    """
    Tests whether we can copy a file withtin the mountpoint.
    """
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__MOUNT_POINT, "in_in.bin")
    print(os.listdir(__MOUNT_POINT))
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__MOUNT_POINT, "in_in_copy.bin")
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)


def test_copy_one_file_out_in(before_every_test):
    """
    Tests whether we can copy a file from a folder outside to the mounpoint.
    """
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__WORKSPACE, "out_in.bin")
    with open(src, "wb") as handle:
        handle.write(data)
    dst = os.path.join(__MOUNT_POINT, os.path.basename(src))
    shutil.copyfile(src, dst)
    assert filecmp.cmp(src, dst, shallow=False)

def test_delete_a_non_existing_file(before_every_test):
    """
    Tests whether we can delete a file we previously created
    """
    src = os.path.join(__WORKSPACE, "non_existing.delete")
    with pytest.raises(FileNotFoundError):
        os.unlink(src)


def test_delete_a_file(before_every_test):
    """
    Tests whether we can delete a file we previously created
    """
    data = os.urandom(__DEFAULT_FILE_SIZE_IN_BYTES)
    src = os.path.join(__WORKSPACE, "data.delete")
    with open(src, "wb") as handle:
        handle.write(data)
    assert os.path.isfile(src)
    assert __DEFAULT_FILE_SIZE_IN_BYTES == os.path.getsize(src)
    os.unlink(src)
    assert not os.path.isfile(src)
