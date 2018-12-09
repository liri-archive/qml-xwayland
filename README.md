QML XWayland
============

[![License](https://img.shields.io/badge/license-LGPLv3.0-blue.svg)](http://www.gnu.org/licenses/lgpl.txt)
[![GitHub release](https://img.shields.io/github/release/lirios/qml-xwayland.svg)](https://github.com/lirios/qml-xwayland)
[![Build Status](https://travis-ci.org/lirios/qml-xwayland.svg?branch=develop)](https://travis-ci.org/lirios/qml-xwayland)
[![GitHub issues](https://img.shields.io/github/issues/lirios/qml-xwayland.svg)](https://github.com/lirios/qml-xwayland/issues)
[![Maintained](https://img.shields.io/maintenance/yes/2018.svg)](https://github.com/lirios/qml-xwayland/commits/develop)

QML plugin with an XWayland implementation for QML compositors
such as the one from Liri Shell.

## Dependencies

Qt >= 5.9.0 with at least the following modules is required:

* [qtbase](http://code.qt.io/cgit/qt/qtbase.git)
* [qtdeclarative](http://code.qt.io/cgit/qt/qtdeclarative.git)
* [qtwayland](http://code.qt.io/cgit/qt/qtwayland.git)

And the following modules:

 * [cmake](https://gitlab.kitware.com/cmake/cmake) >= 3.10.0
 * [cmake-shared](https://github.com/lirios/cmake-shared.git) >= 1.0.0
 * [xcb-util-cursor](http://cgit.freedesktop.org/xcb/util-cursor)

## Installation

```sh
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/path/to/prefix ..
make
make install # use sudo if necessary
```

Replace `/path/to/prefix` to your installation prefix.
Default is `/usr/local`.

### Logging categories

Qt 5.2 introduced logging categories and Liri takes advantage of
them to make debugging easier.

Please refer to the [Qt](http://doc.qt.io/qt-5/qloggingcategory.html) documentation
to learn how to enable them.

### Available categories

* **liri.xwayland:** xwayland
* **liri.xwayland.trace:** xwayland protocol trace

## Licensing

Licensed under the terms of the GNU Lesser General Public License version 3 or,
at your option, any later version.
