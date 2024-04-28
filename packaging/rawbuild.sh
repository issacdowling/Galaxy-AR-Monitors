#! /bin/sh

echo "====================="
echo "This script should just build the Galaxy executable and do nothing else to it."
echo "If you're looking for an AppImage or Flatpak, use their respective scripts."
echo "Those will make use of this script, so you'll still be seeing this message, but other ones first."
echo "====================="
echo ""

## Check if we're in the right directory by looking for main.cpp
## Get the location of this script and its parent dir, then cd into the src/ directory
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
SRCDIR=${HERE%/*}/src

echo "CD'ing into $SRCDIR"
cd $SRCDIR

## Check whether there's a build directory. If not, make and build. If yes, ask permission to delete
DIR=build
if test -d "$DIR"; then
  read -p "The $DIR dir already exists. To continue, I'd like to remove it. Can I? [Y/n] " yn
  case $yn in
    [yY] ) rm -rf build; break;;
    [nN] ) echo "Exiting due to lack of build directory"; exit;;
    * ) rm -rf build; break;;
  esac

else
  echo "$DIR dir doesn't exist, making it."
fi

mkdir build
cd build

## Build
cmake ..
make

echo "====================="
echo "Successfully built. Your executable can be found in build/galaxy"
echo "====================="
