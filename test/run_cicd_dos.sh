#!/bin/sh
(qemu-system-i386 \
-machine pc,accel=kvm:tcg,hpet=off \
-smp cpus=1,cores=1 \
-m 256M \
-rtc base=localtime \
-nographic \
-net none \
-blockdev driver=file,node-name=fd0,filename=/media/x86BOOT.img -device floppy,drive=fd0 \
-drive if=virtio,format=raw,file=fat:rw:"$(pwd)" \
-boot order=a \
-audiodev wav,id=snd0,path="$(pwd)"/hda_out.wav -device intel-hda -device hda-output,audiodev=snd0 \
-audiodev wav,id=snd1,path="$(pwd)"/virtio_out.wav -device virtio-sound-pci,audiodev=snd1 \
-audiodev wav,id=snd2,path="$(pwd)"/pcspk_out.wav -machine pcspk-audiodev=snd2 \
| tee "$(pwd)"/qemu_stdout.log) 3>&1 1>&2 2>&3 | tee "$(pwd)"/qemu_stderr.log
