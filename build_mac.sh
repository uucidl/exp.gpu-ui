#!/usr/bin/env bash
set -e

# User Configuration
BUILD_SLOW_DEPS=${BUILD_SLOW_DEPS:-1}
CXXFLAGS=${CXXFLAGS:--g -O1 -Wall}
DEPS=${DEPS:-third-party-deps}
OUTPUT_DIR=${OUTPUT_DIR:-output/macos}
INTERMEDIATE_OUTPUT_DIR=${INTERMEDIATE_OUTPUT_DIR:-"${OUTPUT_DIR}"/o}
CXX=${CXX:-clang++}
BGFX_SHADERC=${BGFX_SHADERC:-"${OUTPUT_DIR}"/shaderc}
BX_BIN2C=${BX_BIN2C:-"${DEPS}"/bx/tools/bin/darwin/bin2c}

# Create output directories
for dir in "${OUTPUT_DIR}" "${INTERMEDIATE_OUTPUT_DIR}"; do
    [ -d "${dir}" ] || mkdir -p "${dir}"
done

# Test pre-requisites can be called
printf "Testing tools\n"
set -x
"${CXX}" --version 1>/dev/null 2>/dev/null || exit
"${BGFX_SHADERC}" -v 1>/dev/null 2>/dev/null || exit
[ -x "${BX_BIN2C}" ] || exit
set +x

# Build dependencies
if [ "${BUILD_SLOW_DEPS}" -eq 1 ]; then
    "${CXX}" -o "${INTERMEDIATE_OUTPUT_DIR}"/bx.o -c \
             unit_bx.cpp \
             -I"${DEPS}"/bx/include/compat/osx \
             -I"${DEPS}"/bx/include \
             ${CXXFLAGS}
    "${CXX}" -o "${INTERMEDIATE_OUTPUT_DIR}"/bimg.o -c \
             unit_bimg.cpp \
             -I"${DEPS}"/bimg/include \
             -I"${DEPS}"/bx/include/compat/osx \
             -I"${DEPS}"/bx/include \
             ${CXXFLAGS}
    "${CXX}" -o "${INTERMEDIATE_OUTPUT_DIR}"/bgfx.o -c \
             unit_bgfx.cpp \
             -DBGFX_SHARED_LIB_BUILD \
             -I"${DEPS}"/bgfx/include \
             -I"${DEPS}"/bgfx/3rdparty \
             -I"${DEPS}"/bgfx/3rdparty/khronos \
             -I"${DEPS}"/bimg/include \
             -I"${DEPS}"/bx/include/compat/osx \
             -I"${DEPS}"/bx/include \
             ${CXXFLAGS}
fi

# Build the host
"${CXX}" -o "${OUTPUT_DIR}"/test.elf \
         -std=c++11 -Wno-c++98-compat \
         win32_unit_host.cpp \
         "${INTERMEDIATE_OUTPUT_DIR}"/bgfx.o \
         "${INTERMEDIATE_OUTPUT_DIR}"/bx.o \
         "${INTERMEDIATE_OUTPUT_DIR}"/bimg.o \
         -I"${DEPS}"/bgfx/include \
         -I"${DEPS}"/bx/include \
         ${CXXFLAGS}
