#!/bin/bash

set -e
ROOT_PATH=${PWD}
BUILD_DIR=${ROOT_PATH}/openwrt
TARGET=${1}

if [ -z "$1" ]; then
	echo "Error: please specify TARGET"
	echo "For example: IPQ40XX, ECW5410, AP2220, ECW5211"
	exit 1
fi

if [ ! "$(ls -A $BUILD_DIR)" ]; then
	python3 setup.py --setup || exit 1
    
else
	python3 setup.py --rebase
	echo "### OpenWrt repo already setup"
fi

case "${TARGET}" in
EA8300|\
IPQ40XX)
	TARGET=ea8300
	;;
ECW5211)
	TARGET=ecw5211
	;;
ECW5410)
	TARGET=ecw5410
	;;
AP2220)
	TARGET=ap2200
	;;
*)
	echo "${TARGET} is unknown"
	exit 1
	;;
esac
cd ${BUILD_DIR}
./scripts/gen_config.py ${TARGET} wlan-ap || exit 1
cd -

echo "### Building image ..."
cd $BUILD_DIR
make -j$(nproc) V=s 2>&1 | tee build.log
echo "Done"
