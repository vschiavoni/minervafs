#! /usr/bin/env python3
"""
A script to clear the backend minervafs folder
"""
import argparse
import os

from tests import utils

__DEFAULT_CONFIG_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)), "minervafs.json")


def main():
    """
    Main routine
    """
    parser = argparse.ArgumentParser(__file__, "Clears the backend minervafs folder")
    parser.add_argument("-p", "--path", type=str,
                        help="Path of the directory to delete (takes precedence)")
    parser.add_argument("-c", "--config-file", type=str,
                        help="Path of the configuration file where the path is located",
                        default=__DEFAULT_CONFIG_PATH)
    args = parser.parse_args()
    utils.clear_backend_directory(path=args.path, config_file=args.config_file)


if __name__ == "__main__":
    main()
