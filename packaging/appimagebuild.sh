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

./packaging/rawbuild.sh

## Begin making AppImage
echo "CD'ing back into $REPOROOTDIR"
cd $REPOROOTDIR

DIR=packaging/galaxy.AppDir/
if test -d "$DIR"; then
  read -p "The AppImage packaging dir ($DIR) already exists. To continue, I'd like to remove it. Can I? [Y/n] " yn
  case $yn in
    [yY] ) rm -rf $DIR; break;;
    [nN] ) echo "Exiting due to user cancellation"; exit;;
    * ) rm -rf $DIR; break;;
  esac

else
  echo "$DIR dir doesn't exist, making it."
fi

## Copy over the executables
mkdir -p packaging/galaxy.AppDir/usr/bin/
mv src/build/galaxy packaging/galaxy.AppDir/usr/bin/
mv src/build/xrealAirLinuxDriver packaging/galaxy.AppDir/usr/bin/

## Copy over the required Raylib, OpenCV imgproc, and OpenCV core libs
mkdir -p packaging/galaxy.AppDir/usr/lib
cp /usr/lib/libopencv_imgproc.so* packaging/galaxy.AppDir/usr/lib/
cp /usr/lib/libopencv_core.so* packaging/galaxy.AppDir/usr/lib/
cp /usr/lib/libraylib.so* packaging/galaxy.AppDir/usr/lib/

cp /usr/lib/liblapack.so* packaging/galaxy.AppDir/usr/lib
cp /usr/lib/libcblas.so* packaging/galaxy.AppDir/usr/lib
cp /usr/lib/libblas.so* packaging/galaxy.AppDir/usr/lib

cp /usr/lib/libtbb.so* packaging/galaxy.AppDir/usr/lib

cp /usr/lib/libgfortran.so* packaging/galaxy.AppDir/usr/lib

## This removes the plain .so files, and the .so.x.y.z files, leaving the .so.xyz files there,
## which is done to save space, as these are the only ones the program checks for.
rm packaging/galaxy.AppDir/usr/lib/lib*.*.*.*
rm packaging/galaxy.AppDir/usr/lib/*.so

## Copy the metadata
cp packaging/AppRun packaging/galaxy.AppDir/AppRun
cp packaging/galaxy.desktop packaging/galaxy.AppDir/galaxy.desktop
cp packaging/galaxy.png packaging/galaxy.AppDir/galaxy.png

## Build the appimage
appimagetool packaging/galaxy.AppDir