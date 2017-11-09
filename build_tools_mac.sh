#!/usr/bin/env bash
OUTPUT_DIR=${OUTPUT_DIR:-output/macos}
DEPS=${DEPS:-third-party-deps}

set -e
Deps=third-party-deps

pushd "$Deps"/bgfx
../bx/tools/bin/darwin/genie --gcc=osx --with-tools ninja
popd

[ -d "${OUTPUT_DIR}" ] || mkdir -p "${OUTPUT_DIR}"
cp "${DEPS}"/bgfx/.build/osx64_clang/bin/shadercRelease "${OUTPUT_DIR}"/shaderc
