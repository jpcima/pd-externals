#!/bin/bash -e

package=jpcex

###
scriptdir=`dirname "$0"`
sourcedir="$scriptdir/.."
cd "$sourcedir"

function build() {
    local target="$1" floatsize="$2" prefix="" build="build"
    if test ! -z "$target"; then
        prefix="$target-"; build="build-$target"
    fi

    local cmakeflags="-DCMAKE_BUILD_TYPE=Release"
    case "$floatsize" in
        32) ;;
        64) cmakeflags="$cmakeflags -DPDEX_DOUBLE=ON" ;;
        *) echo "invalid float size \"$floatsize\"" ; exit 1 ;;
    esac
    build="$build-$floatsize"

    rm -rf "$build"; mkdir -p "$build"; pushd "$build"
    "${prefix}cmake" $cmakeflags ..
    cmake --build . --target "$package"; cmake --build . --target clean
    popd
}

for floatsize in 32 64; do
    for arch in "" "i686-w64-mingw32" "x86_64-apple-darwin15"; do
        build "${arch}" "${floatsize}"
    done
done
