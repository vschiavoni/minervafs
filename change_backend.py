#! /usr/bin/env python3
"""
Change the backend in the config file
"""
import argparse
import os
import json

__DEFAULT_BACKEND = "/mnt/backend"
__DEFAULT_CONFIG_PATH = os.path.join(os.path.dirname(os.path.realpath(__file__)), "minervafs.json")

def main():
    parser = argparse.ArgumentParser(__file__, "Changes the backend directory")
    parser.add_argument("-c", "--config-file", type=str,
        help="Path to the configuration file",
        default=__DEFAULT_CONFIG_PATH)
    parser.add_argument("-b", "--backend", type=str,
        help="Path to the directory that should be used as the backend",
        default=__DEFAULT_BACKEND)
    args = parser.parse_args()
    with open(args.config_file, "r") as handle:
        config = json.load(handle)
    config["root_folder"] = args.backend
    print(json.dumps(config, indent=4))
    with open(args.config_file, "w") as handle:
        json.dump(config, handle, indent=4)

if __name__ == "__main__":
    main()
