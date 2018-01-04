
TRAVIS_OS_NAME=centos7
TRAVIS_BUILD_DIR=~/gits/knossos

$TRAVIS_BUILD_DIR/packager/dependency_cp.sh knossos knossos.$TRAVIS_OS_NAME "" "ld-linux|GL" /usr/local/lib 0

# hook from CMakeLists.txt to export Gui plugins directory. since qt can not offer cross-platform tools, use a hammer.
# do not copy the debug libraries (ending with d).
mkdir knossos.$TRAVIS_OS_NAME/plugins; DEPLOY_PLUGINS=`pwd`/knossos.$TRAVIS_OS_NAME/plugins
pushd $(dirname $(dirname $(grep -E '^/.*platforms' platforms.txt | head -n 1)))
find . -regextype posix-extended -regex "^.*[^/]+[^d]\.(so|dylib|dll)$" -exec cp -p --parents {} $DEPLOY_PLUGINS \; ;popd
# move the platforms plugins to exe dir, qt hard-coded on all platforms.
mv knossos.$TRAVIS_OS_NAME/plugins/platforms knossos.$TRAVIS_OS_NAME 
# use dependency copy again to copy dependencies for platform plugins
for f in knossos.$TRAVIS_OS_NAME/platforms/*.*; do $TRAVIS_BUILD_DIR/packager/dependency_cp.sh $f knossos.$TRAVIS_OS_NAME/platforms/deps "" "ld-linux|GL" /usr/local/lib 0; done
# move the modified platform dependencies back to the platform dir
pushd knossos.$TRAVIS_OS_NAME/platforms && find . -maxdepth 1 -type f -exec mv deps/{} . \; && popd
# move the platform dependencies to the package dir
mv knossos.$TRAVIS_OS_NAME/platforms/deps/* knossos.$TRAVIS_OS_NAME && rmdir knossos.$TRAVIS_OS_NAME/platforms/deps
cp -p $TRAVIS_BUILD_DIR/packager/run_knossos.sh knossos.$TRAVIS_OS_NAME
tar czvf knossos.$TRAVIS_OS_NAME.tar.gz knossos.$TRAVIS_OS_NAME

