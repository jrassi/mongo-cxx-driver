#!/bin/sh

# TODO explain where this should be run from
# PATH_TO_CMAKE
# PATH_TO_BUILD_TOOL

# CMAKE_FLAGS

set -o xtrace
set -o errexit
#set -o nounset

OS=$(uname -s | tr '[:upper:]' '[:lower:]')

PATH_TO_CMAKE="${PATH_TO_CMAKE:-cmake}"
"$PATH_TO_CMAKE" $CMAKE_FLAGS ..

case "$OS" in
	darwin|linux)
        if [ -f /proc/cpuinfo ]; then
            CONCURRENCY=$(grep -c ^processor /proc/cpuinfo)
        elif which sysctl; then
            CONCURRENCY=$(sysctl -n hw.logicalcpu)
        else
            echo "$0: can't figure out what value of -j to pass to 'make'" >&2
            exit 1
        fi
        PATH_TO_BUILD_TOOL="${PATH_TO_BUILD_TOOL:-make}"
        "$PATH_TO_BUILD_TOOL" "-j$CONCURRENCY" all
        "$PATH_TO_BUILD_TOOL" install
        "$PATH_TO_BUILD_TOOL" "-j$CONCURRENCY" examples
        ;;
    cygwin*)
        PATH_TO_BUILD_TOOL="${PATH_TO_BUILD_TOOL:-msbuild.exe}"
        "$PATH_TO_BUILD_TOOL" /m ALL_BUILD.vcxproj
        "$PATH_TO_BUILD_TOOL" INSTALL.vcxproj
        "$PATH_TO_BUILD_TOOL" /m examples/examples.vcxproj
        ;;
esac
