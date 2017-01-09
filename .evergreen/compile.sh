#!/bin/sh

# Runs cmake and compiles the standard build targets (all, install, examples).
#
# This following environment variables will change the behavior of this script:
# - PATH_TO_CMAKE: full path to cmake (defaults to searching $PATH)
# - PATH_TO_BUILD_TOOL: full path to make / msbuild.exe (defaults to searching $PATH)
# - CMAKE_FLAGS: additional flags to pass to cmake

set -o xtrace
set -o errexit

OS=$(uname -s | tr '[:upper:]' '[:lower:]')

PATH_TO_CMAKE="${PATH_TO_CMAKE:-cmake}"

case "$OS" in
	darwin|linux)
        "$PATH_TO_CMAKE" $CMAKE_FLAGS ..
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
        "$PATH_TO_CMAKE" -G "Visual Studio 14 2015 Win64" $CMAKE_FLAGS ..
        PATH_TO_BUILD_TOOL="${PATH_TO_BUILD_TOOL:-msbuild.exe}"
        "$PATH_TO_BUILD_TOOL" /m ALL_BUILD.vcxproj
        "$PATH_TO_BUILD_TOOL" INSTALL.vcxproj
        "$PATH_TO_BUILD_TOOL" /m examples/examples.vcxproj
        ;;
esac
