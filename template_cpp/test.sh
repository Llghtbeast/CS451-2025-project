#!/bin/bash
set -e

# Usage: test.sh [build_dir]
# If build_dir is omitted or ".", the script will attempt to use ./target next to this script.
# Otherwise, it will try to cd into the provided build_dir and fail if it does not exist.

echo "WARNING: This test script does not rebuild the project. If you want to test a fresh build, run the build with testing enabled: ./build.sh -t"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
REQUESTED_BUILD_DIR="${1:-.}"

# If the caller explicitly passed ".", interpret this as "use the script's target folder"
if [[ "${REQUESTED_BUILD_DIR}" == "." || -z "${REQUESTED_BUILD_DIR}" ]]; then
  DEFAULT_TARGET="${SCRIPT_DIR}/target"
  if [[ -d "${DEFAULT_TARGET}" ]]; then
    BUILD_DIR="${DEFAULT_TARGET}"
  else
    echo "Error: No build directory specified and target folder not found at ${DEFAULT_TARGET}."
    echo "Run the build script first or pass the build directory to this script: ./test.sh /path/to/target"
    exit 1
  fi
else
  BUILD_DIR="${REQUESTED_BUILD_DIR}"
fi

# Ensure the build directory exists and cd into it
if [[ ! -d "${BUILD_DIR}" ]]; then
  echo "Error: Build directory does not exist: ${BUILD_DIR}"
  exit 1
fi

cd "${BUILD_DIR}"

echo "Running unit tests in ${BUILD_DIR}"

if command -v ctest >/dev/null 2>&1; then
  ctest -N
  # Use verbose mode so stdout/stderr is shown even for passing tests
  ctest -V --output-on-failure || { echo "Some tests failed"; exit 1; }
else
  # Fallback: run any executables with 'test' in their name
  mapfile -t tests < <(find . -type f -executable -name '*test*' ! -path './CMakeFiles/*' || true)

  if [[ ${#tests[@]} -eq 0 ]]; then
    echo "No test executables found to run."
    exit 1
  else
    for t in "${tests[@]}"; do
      echo "Running test executable: $t"
      "$t" || { echo "Test $t failed"; exit 1; }
    done
  fi
fi

echo "All tests passed."