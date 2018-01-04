#!/bin/bash

pacman -Syuu --noconfirm
#pacman -S --needed --noconfirm $MINGW_PACKAGE_PREFIX-{boost,cmake,jasper,ninja,python2,qt5-static,snappy,toolchain,hdf5}
pacman -S --needed --noconfirm $MINGW_PACKAGE_PREFIX-{boost,cmake,jasper,ninja,python2,qt5,snappy,toolchain,hdf5,pcre2}
#pacman -S --needed --noconfirm $MINGW_PACKAGE_PREFIX-{boost,cmake,jasper,ninja,python2,snappy,toolchain,hdf5}
pacman -S --needed --noconfirm zip

## xxx - build starting failing at qt5.9.2 with 'AutoUic: Error: uic process failed for "include/ui_aboutdialog.h"'
#curl -L http://repo.msys2.org/mingw/x86_64/mingw-w64-x86_64-qt5-5.9.1-1-any.pkg.tar.xz > qt5.pkg.tar.xz
#pacman -U --noconfirm qt5.pkg.tar.xz

# Download and install static PythonQt and QuaZIP
#curl -L https://al3xst.de/stuff/$MINGW_PACKAGE_PREFIX-pythonqt-static.pkg.tar.xz > pythonqt.pkg.tar.xz
#curl -L https://al3xst.de/stuff/$MINGW_PACKAGE_PREFIX-quazip-static.pkg.tar.xz > quazip.pkg.tar.xz
#pacman -U --noconfirm pythonqt.pkg.tar.xz quazip.pkg.tar.xz

PROJECTPATH=$(cygpath ${APPVEYOR_BUILD_FOLDER})

## build and install PythonQt
## xxx - pythonqt was also not building properly on mac, so disabled it.
##   most likely installing the prebuilt non-static pythonqt above would have also worked.
#git clone https://github.com/knossos-project/PythonQt.git 
#rm -rf PythonQt/build && mkdir -p PythonQt/build
#pushd PythonQt/build
## this cmake step segfaults at the end because cmake is the best build system that ever lived. tried without ninja also
#CXXFLAGS=-Wno-deprecated-declarations cmake -G Ninja -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:PATH=/mingw64 ..
#ninja -j2 && ninja install
#popd

# build and install quazip
git clone https://github.com/mnafees/quazip.git
rm -rf quazip/build && mkdir -p quazip/build
pushd quazip/build
cmake -G Ninja -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:PATH=/mingw64 ..
ninja -j2 && ninja install
popd

## build and install szip
## xxx - neither this nor the installed version from hdf5 dependency works with the static knossos build, gave up.
#curl -L https://support.hdfgroup.org/ftp/lib-external/szip/2.1.1/src/szip-2.1.1.tar.gz > szip-2.1.1.tar.gz
#tar xvf szip-2.1.1.tar.gz
#rm -rf szip-2.1.1/build && mkdir -p szip-2.1.1/build
#rm -rf /mingw64/lib/libszip.a /mingw64/lib/libszip.dll.a
#pushd szip-2.1.1/build
#cmake -G Ninja -DCMAKE_INSTALL_PREFIX=/mingw64 -DCMAKE_BUILD_TYPE:STRING=Release -DBUILD_SHARED_LIBS=ON ..
#ninja -j2 && ninja install
#popd

# build knossos
mkdir knossos-build
pushd knossos-build
cmake -G Ninja -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=RELEASE -DWITH_PYTHON_QT=OFF $PROJECTPATH
ninja -j2

# Package
export TRAVIS_OS_NAME=win64
$PROJECTPATH/packager/dependency_cp.sh knossos.exe knossos.$TRAVIS_OS_NAME "" "windows/system32|comctl|gdiplus" /mingw64/lib 2
# hook from CMakeLists.txt to export Gui plugins directory. since qt can not offer cross-platform tools, use a hammer.
# do not copy the debug libraries (ending with d).
mkdir knossos.$TRAVIS_OS_NAME/plugins; DEPLOY_PLUGINS=`pwd`/knossos.$TRAVIS_OS_NAME/plugins
pushd $(dirname $(dirname $(grep -E '^?:/.*platforms' platforms.txt | head -n 1)))
find . -regextype posix-extended -regex "^.*[^/]+[^d]\.(so|dylib|dll)$" -exec cp -p --parents {} $DEPLOY_PLUGINS \; ;popd
# move the platforms plugins to exe dir, qt hard-coded on all platforms.
mv knossos.$TRAVIS_OS_NAME/plugins/platforms knossos.$TRAVIS_OS_NAME
# use dependency copy again to copy dependencies for platform plugins
for f in knossos.$TRAVIS_OS_NAME/platforms/*.*; do $PROJECTPATH/packager/dependency_cp.sh $f knossos.$TRAVIS_OS_NAME/platforms/deps "" "windows/system32|comctl|gdiplus" /mingw64/lib 2; done
# move the modified platform dependencies back to the platform dir
pushd knossos.$TRAVIS_OS_NAME/platforms && find . -maxdepth 1 -type f -exec mv deps/{} . \; && popd
# move the platform dependencies to the package dir
mv knossos.$TRAVIS_OS_NAME/platforms/deps/* knossos.$TRAVIS_OS_NAME && rmdir knossos.$TRAVIS_OS_NAME/platforms/deps
zip -r knossos.$TRAVIS_OS_NAME.zip knossos.$TRAVIS_OS_NAME
mv knossos.$TRAVIS_OS_NAME.zip ${APPVEYOR_BUILD_FOLDER}
popd

