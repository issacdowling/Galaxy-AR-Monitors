echo "====================="
echo "This script will build an AppImage of Galaxy."
echo "It will call the base build script first, which will create the executable."
echo "Don't be alarmed when you see a message similar to this but without the AppImage bit."
echo "Once that step is complete, this script will turn it into an AppImage."
echo "====================="
echo ""

## Check if we're in the right directory by looking for main.cpp
## Get the location of this script and its parent dir, then cd into the repo's root
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
REPOROOTDIR=${HERE%/*}

echo "CD'ing into $REPOROOTDIR"
cd $REPOROOTDIR

# Call the regular build script
echo "Running the regular build script."
echo ""
exec packaging/rawbuild.sh

## Begin making AppImage
echo "CD'ing back into $REPOROOTDIR"
cd $REPOROOTDIR

## Copy over the executable
mkdir -p packaging/galaxy.AppDir/usr/bin
mv src/build/galaxy packaging/galaxy.AppDir/usr/bin

## Copy over the required Raylib, OpenCV imgproc, and OpenCV core libs
mkdir -p packaging/galaxy.AppDir/usr/lib
cp /usr/lib/libopencv_imgproc.so* packaging/galaxy.AppDir/usr/lib
cp /usr/lib/libraylib.so* packaging/galaxy.AppDir/usr/lib

## This removes the plain .so files, and the .so.x.y.z files, leaving the .so.xyz files there,
## which is done to save space, as these are the only ones the program checks for.
rm packaging/galaxy.AppDir/usr/lib/lib*.*.*.*
rm packaging/galaxy.AppDir/usr/lib/*.so

## Copy the metadata
cp packaging/AppRun packaging/galaxy.AppDir/AppRun
cp packaging/galaxy.desktop packaging/galaxy.AppDir/galaxy.desktop
cp packaging/galaxy.png packaging/galaxy.AppDir/galaxy.png

## Build the appimage
appimagetool packaging/galaxy.AppDir/