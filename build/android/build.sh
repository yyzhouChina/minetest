#!/bin/bash

if [ -z "$ANDROID_NDK" ]; then
	echo "Please set ANDROID_NDK environment variable to point to your NDK root"
	echo "e.g. \`export ANDROID_NDK=/home/xyz/android-ndk-r8-crystax-1/\`"
	echo "and then restart the script."
	exit 1
fi

if [ -z "$NDK_MODULE_PATH" ]; then
	echo "NDK_MODULE_PATH not set you wont be able to use profiler"
fi

if [ -z "$NDEBUG" ]; then
	NDEBUG=1
fi

pushd `dirname $0` > /dev/null
ROOT=`pwd`
popd > /dev/null

mkdir -p $ROOT/deps
cd $ROOT/deps

if [ ! -d "leveldb" ]; then
	echo ">> Fetching LevelDB"
	git clone https://code.google.com/p/leveldb/ || exit 1
fi

if [ ! -d "irrlicht" ]; then
	echo ">> Checking out Irrlicht ogl-es branch"
	svn co http://svn.code.sf.net/p/irrlicht/code/branches/ogl-es/ irrlicht || exit 1
	echo ">> Applying irrlicht.patch"
	cd irrlicht
	patch -p1 < $ROOT/irrlicht.patch || exit 1
fi

echo ">> Building LevelDB"
cd $ROOT/deps/leveldb
$ROOT/scripts/build_leveldb.sh || exit 1

echo ">> Building Irrlicht"
cd $ROOT/deps/irrlicht/source/Irrlicht/Android/
$ANDROID_NDK/ndk-build NDEBUG=$NDEBUG -j8 || exit 1

echo ">> Generating Assets"
mkdir -p $ROOT/assets
mkdir -p $ROOT/assets/Minetest

cp $ROOT/../../minetest.conf.example $ROOT/assets/Minetest
cp $ROOT/../../README.txt $ROOT/assets/Minetest
echo "builtin"
cp -r $ROOT/../../builtin $ROOT/assets/Minetest
echo "client"
cp -r $ROOT/../../client $ROOT/assets/Minetest
echo "doc"
cp -r $ROOT/../../doc $ROOT/assets/Minetest
echo "fonts"
cp -r $ROOT/../../fonts $ROOT/assets/Minetest
echo "games"
cp -r $ROOT/../../games $ROOT/assets/Minetest
echo "mods"
cp -r $ROOT/../../mods $ROOT/assets/Minetest
echo "po"
cp -r $ROOT/../../po $ROOT/assets/Minetest
echo "textures"
cp -r $ROOT/../../textures $ROOT/assets/Minetest

cd $ROOT/assets
ls -R | grep ":$" | sed -e 's/:$//' -e 's/\.//' -e 's/^\///' > "index.txt"

echo ">> Building Minetest"
cd $ROOT
$ANDROID_NDK/ndk-build NDK_MODULE_PATH=$NDK_MODULE_PATH NDEBUG=$NDEBUG -j8 || exit 1
ant debug || exit 1

echo "++ Success!"
echo "APK: bin/Minetest-debug.apk"
echo "You can install it with \`adb install -r bin/Minetest-debug.apk\`"
echo "or build a release version with \`ant release\`"
