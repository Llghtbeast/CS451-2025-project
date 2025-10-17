#!/bin/bash

set -e

# Change the current working directory to the location of the present file
cd "$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Default build type
BUILD_TYPE="Release"

# Parse flags: -d | --debug for Debug, -r | --release for Release
while [[ $# -gt 0 ]]; do
  case "$1" in
    -d|--debug)
      BUILD_TYPE="Debug"
      shift
      ;;
    -r|--release)
      BUILD_TYPE="Release"
      shift
      ;;
    *)
      echo "Warning: unknown option '$1' (ignoring)"
      shift
      ;;
  esac
done

rm -rf target
mkdir target
cd target
cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ..
cmake --build .
mv src/da_proc ../bin
