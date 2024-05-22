#!/usr/bin/env bash
set -e

PATH_TO_SBEMU_EXE=${1?param 1 missing - path to SBEMU.EXE}
test -f "$PATH_TO_SBEMU_EXE" || (echo "File $PATH_TO_SBEMU_EXE does not exit"; exit 1)
FULL_PATH_TO_SBEMU_EXE=$(readlink -f "$PATH_TO_SBEMU_EXE")
PATH_TO_RELEASE_NOTES=${2?param 2 missing - path to RELEASE_NOTES.md}
test -f "$PATH_TO_RELEASE_NOTES" || (echo "File $PATH_TO_RELEASE_NOTES does not exit"; exit 1)
FULL_PATH_TO_RELEASE_NOTES=$(readlink -f "$PATH_TO_RELEASE_NOTES")

PATH_TO_OUTPUT_ARTIFACTS=${3?param 3 missing - path to output directory}
test -d "$PATH_TO_OUTPUT_ARTIFACTS" || (echo "Directory $PATH_TO_OUTPUT_ARTIFACTS does not exit"; exit 1)
FULL_PATH_TO_OUTPUT_ARTIFACTS=$(readlink -f "$PATH_TO_OUTPUT_ARTIFACTS")

mkdir -p /tmp/sbemu_usb_img
rm -rf /tmp/sbemu_usb_img/*
pushd /tmp/sbemu_usb_img
wget https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/distributions/1.3/official/FD13-LiteUSB.zip
wget https://www.freedos.org/download/verify.txt
grep -q "64a934585087ccd91a18c55e20ee01f5f6762be712eeaa5f456be543778f9f7e  FD13-LiteUSB.zip" verify.txt
echo "64a934585087ccd91a18c55e20ee01f5f6762be712eeaa5f456be543778f9f7e  FD13-LiteUSB.zip" | shasum -a 256 --check
unzip FD13-LiteUSB.zip
rm FD13-LiteUSB.zip
wget https://github.com/Baron-von-Riedesel/Jemm/releases/download/v5.84/JemmB_v584.zip
echo "80bee162c9574066112a3204af6f72666428f7c139836f4418d92aae7bfb5056  JemmB_v584.zip" | shasum -a 256 --check
wget https://github.com/crazii/HX/releases/download/v0.1-beta4fix2/HDPMI32i.zip
echo "69a559f02a954afa2550bf8840adfaa27dad7eeeab93a12b2642c283bf3d5bcb  HDPMI32i.zip" | shasum -a 256 --check
wget https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3/base/ctmouse.zip
echo "a891124cd5b13e8141778fcae718f3b2667b0a49727ce92b782ab11a8c4bb63a  ctmouse.zip" | shasum -a 256 --check
mkdir -p /tmp/SBEMU
mkdir -p /tmp/mnt
sudo mount FD13LITE.img /tmp/mnt -t vfat -o loop,offset=$((63*512)),rw,uid="$(id -u)",gid="$(id -g)"
mkdir /tmp/mnt/sbemu
cp "$FULL_PATH_TO_SBEMU_EXE" /tmp/mnt/sbemu
cp "$FULL_PATH_TO_RELEASE_NOTES" /tmp/mnt/sbemu
cp "$FULL_PATH_TO_SBEMU_EXE" /tmp/SBEMU
cp "$FULL_PATH_TO_RELEASE_NOTES" /tmp/SBEMU
pushd /tmp/mnt
mkdir jemm
cd jemm
unzip /tmp/sbemu_usb_img/JemmB_v584.zip
cp JEMMEX.EXE /tmp/SBEMU
cp JLOAD.EXE /tmp/SBEMU
cp QPIEMU.DLL /tmp/SBEMU
cd ..
mkdir hdpmi
cd hdpmi
unzip /tmp/sbemu_usb_img/HDPMI32i.zip
cp HDPMI32i.EXE /tmp/SBEMU
cd ..
mkdir ctmouse
unzip -j "/tmp/sbemu_usb_img/ctmouse.zip" "BIN/CTMOUSE.EXE" -d "ctmouse"
sed -i 's/DEVICE=\\FREEDOS\\BIN\\HIMEMX.EXE/DEVICE=\\JEMM\\JEMMEX.EXE MAXEXT=2097152\nDEVICE=\\JEMM\\JLOAD.EXE \\JEMM\\QPIEMU.DLL/g' fdconfig.sys
mv setup.bat setup.bak
echo 'LH \HDPMI\HDPMI32I.EXE' > setup.bat
echo 'LH \SBEMU\SBEMU.EXE' >> setup.bat
echo 'LH \CTMOUSE\CTMOUSE.EXE' >> setup.bat
popd
rm JemmB_v584.zip
rm HDPMI32i.zip
rm ctmouse.zip
sudo umount /tmp/mnt
pushd /tmp
zip -9 SBEMU.zip SBEMU/*
mv SBEMU.zip "$FULL_PATH_TO_OUTPUT_ARTIFACTS"
ls -lh SBEMU
rm -rf SBEMU
popd
mv FD13LITE.img SBEMU-FD13-USB.img
xz -k -9e SBEMU-FD13-USB.img
mv SBEMU-FD13-USB.img.xz "$FULL_PATH_TO_OUTPUT_ARTIFACTS"
popd
rm -rf /tmp/sbemu_usb_img
rm -rf /tmp/mnt
