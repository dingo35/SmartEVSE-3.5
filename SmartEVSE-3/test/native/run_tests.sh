#!/bin/bash
# SmartEVSE-3 Native Test Runner
# Builds and runs all state machine tests
#
# Usage: ./run_tests.sh

set -e
cd "$(dirname "$0")"

echo "Building tests..."
make clean
make all

echo ""
echo "Running tests..."
make test
