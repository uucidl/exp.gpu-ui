#!/bin/sh
set -e
HERE="$PWD"/`dirname "$0"`
BUILD_BAT=${HERE}/build.bat
echo ${BUILD_BAT}
exec cmd //c "${BUILD_BAT}"
