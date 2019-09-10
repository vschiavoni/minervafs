#! /usr/bin/env python3
"""
A script to clear the backend minervafs folder
"""
import argparse
import json
import os
import shutil


def main():
    """
    Main routine
    """
    parser = argparse.ArgumentParser(__file__, "Clears the backend minervafs folder")
    parser.add_argument("-p", "--path", type=str, help="Path of the directory to delete")
    args = parser.parse_args()
    if args.path:
        path = args.path
    else:
        with open("minervafs.json") as handle:
            configuration = json.load(handle)
        path = configuration.get("root_folder", "~/.minervafs")
    path = os.path.expanduser(path)
    if os.path.isdir(path):
        shutil.rmtree(path)

if __name__ == "__main__":
    main()
