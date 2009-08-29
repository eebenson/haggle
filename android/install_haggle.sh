#!/bin/sh

# This script pushes built files to the sdcard on the phone, assuming
# that the Haggle build dir is in external/haggle of the Android
# source tree. The 'adb' utility from the Android SDK must
# be in the path.

SCRIPT_DIR=$PWD/`dirname $0`
DEVICE_FILES_DIR=$SCRIPT_DIR/device-files
PUSH_DIR=/sdcard
DATA_DIR=/data/local
ADB=adb
ADB_PARAMS=
ANDROID_DIR=$SCRIPT_DIR/../../../
SOURCE_DIR=$ANDROID_DIR/out/target/product/dream

echo "Looking for Android devices..."

DEVICES=$(adb devices | awk '{ if (match($2,"device")) print $1}')
NUM_DEVICES=$(echo $DEVICES | awk '{print split($0,a, " ")}')

if [ $NUM_DEVICES -lt 1 ]; then
    echo "There are no Android devices connected to the computer."
    echo "Please connect at least one device before installation can proceed."
    echo
    exit
fi 

echo "$NUM_DEVICES Android devices found."
echo
echo "Assuming android source is found in $ANDROID_DIR"
echo "Please make sure this is correct before proceeding."
echo
echo "Press any key to install Haggle on these devices, or ctrl-c to abort"

# Wait for some user input
read

pushd $SCRIPT_DIR

# Depending on how the device is rooted, it might not be possible to
# write directly to the root partition due to lack of permissions. We
# therefore write first to the sdcard, and then we run a script on the
# device as 'root', which copies the files to their correct places.

if [ -f $DEVICE_FILES_DIR/adhoc.sh ]; then
    
    for dev in $DEVICES; do
	echo "Installing configuration files onto device $dev"
	
	# Cleanup data folder if any
	$ADB -s $dev shell su -c rm /data/haggle/*

	# Install scripts and other configuration files
	$ADB -s $dev push $DEVICE_FILES_DIR/adhoc.sh $DATA_DIR/
	$ADB -s $dev push $DEVICE_FILES_DIR/tiwlan.ini $DATA_DIR/
	$ADB -s $dev shell chmod 775 $DATA_DIR/adhoc.sh
	$ADB -s $dev shell su -c mkdir /data/haggle
	$ADB -s $dev push $DEVICE_FILES_DIR/htc-magic-small.jpg /data/haggle/Avatar.jpg
	
	if [ "$dev" = "HT93XKF09536" ]; then 
	    echo "#ffbf2c00" > /tmp/deviceColor
	    $ADB -s $dev push /tmp/deviceColor /data/haggle/
	fi
	if [ "$dev" = "HT93XKF03557" ]; then 
	    echo "#ffa200bf" > /tmp/deviceColor
	    $ADB -s $dev push /tmp/deviceColor /data/haggle/
	fi
	if [ "$dev" = "HT93YKF07043" ]; then 
	    echo "#ff00bf20" > /tmp/deviceColor
	    $ADB -s $dev push /tmp/deviceColor /data/haggle/
	fi
    done
fi

FRAMEWORK_PATH_PREFIX="system/framework"
FRAMEWORK_FILES="haggle.jar"

LIB_PATH_PREFIX="system/lib"
LIBS="libhaggle.so libhaggle_jni.so libhaggle-xml2.so"

BIN_PATH_PREFIX="system/bin"
HAGGLE_BIN="haggle"

pushd $SOURCE_DIR

echo $PWD

for dev in $DEVICES; do
    echo
    echo "Installing files onto device $dev"

    # Remount /system partition in read/write mode
    $ADB -s $dev shell su -c mount -o remount,rw -t yaffs2 /dev/block/mtdblock3 /system

    # Enter directory holding unstripped binaries
    pushd symbols

    # Install Haggle binary
    echo
    echo "Installing Haggle binary"
    echo "    $HAGGLE_BIN"
    $ADB -s $dev push sbin/$HAGGLE_BIN /$BIN_PATH_PREFIX/$HAGGLE_BIN
    $ADB -s $dev shell su -c chmod 4775 /$BIN_PATH_PREFIX/$HAGGLE_BIN
    
    # Install libraries
    echo
    echo "Installing library files"
    for file in $LIBS; do
	echo "    $file"
	$ADB -s $dev push $LIB_PATH_PREFIX/$file /$LIB_PATH_PREFIX/$file
	$ADB -s $dev shell su -c chmod 644 /$LIB_PATH_PREFIX/$file
    done
    
    # Back to source dir
    popd

    # Install framework files
    echo
    echo "Installing framework files"
    for file in $FRAMEWORK_FILES; do
	echo "    $file"
	$ADB -s $dev push $FRAMEWORK_PATH_PREFIX/$file /$FRAMEWORK_PATH_PREFIX/$file
	$ADB -s $dev shell su -c chmod 644 /$FRAMEWORK_PATH_PREFIX/$file
    done

    # Reset /system partition to read-only mode
    $ADB -s $dev shell su -c mount -o remount,ro -t yaffs2 /dev/block/mtdblock3 /system
done

popd
popd
