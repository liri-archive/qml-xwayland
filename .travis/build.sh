#!/bin/bash

set -e

source /usr/local/share/liri-travis/functions

# Install dependencies
travis_start "install_packages"
msg "Install packages..."
dnf install -y \
     libxcb-devel \
     xcb-util-cursor-devel \
     libXcursor-devel \
     wayland-devel
travis_end "install_packages"

# Install artifacts
travis_start "artifacts"
msg "Install artifacts..."
/usr/local/bin/liri-download-artifacts $TRAVIS_BRANCH cmakeshared-artifacts.tar.gz
travis_end "artifacts"

# Configure
travis_start "configure"
msg "Setup CMake..."
mkdir build
cd build
if [ "$CXX" == "clang++" ]; then
    clazy="-DCMAKE_CXX_COMPILER=clazy"
fi
cmake .. $clazy \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DINSTALL_LIBDIR=/usr/lib64 \
    -DINSTALL_QMLDIR=/usr/lib64/qt5/qml \
    -DINSTALL_PLUGINSDIR=/usr/lib64/qt5/plugins
travis_end "configure"

# Build
travis_start "build"
msg "Build..."
make -j $(nproc)
travis_end "build"

# Install
travis_start "install"
msg "Install..."
make install
travis_end "install"
