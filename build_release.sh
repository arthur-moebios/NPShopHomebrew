#!/bin/sh

# builds a preset
build_preset() {
    echo Configuring $1 ...
    cmake --preset $1
    echo Building $1 ...
    cmake --build --preset $1
}

build_preset Release

rm -rf out

# --- SWITCH --- #
mkdir -p out/switch/npshop/
cp -r build/Release/*.nro out/switch/npshop/npshop.nro
pushd out
zip -r9 npshop.zip switch
popd
