#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
binary=$(mktemp)
trap 'rm -f "$binary"' EXIT
"${CXX:-c++}" -std=c++11 -Itests/meter -ISmartEVSE-3/src \
    tests/meter/readiness.cpp -o "$binary"
"$binary"
