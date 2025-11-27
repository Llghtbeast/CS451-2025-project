#!/bin/bash

set -e

# Change the current working directory to the location of the present file
cd "$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

# Default build type
BUILD_TYPE="Release"
RUN_TESTS="false"

# Parse flags: -d | --debug for Debug, -r | --release for Release, -t | --test to run tests after build
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
    -t|--test)
      RUN_TESTS="true"
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

# Run tests:
if [[ "${RUN_TESTS}" == "true" ]]; then
  echo "Running unit tests..."

  # Call the separate test script in the project root
  echo "$BUILD_DIR"
  ../test.sh "${BUILD_DIR}" || { echo "Some tests failed"; exit 1; }

  echo "All tests passed."
fi

mv src/da_proc ../bin
