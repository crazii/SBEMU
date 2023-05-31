#!/bin/sh

mkdir -p output

gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_cards/ac97_def.c -o output/ac97_def.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_cards/au_cards.c -o output/au_cards.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_mixer/cv_bits.c -o output/cv_bits.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_mixer/cv_chan.c -o output/cv_chan.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_mixer/cv_freq.c -o output/cv_freq.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/dpmi/dbgutil.c -o output/dbgutil.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/dbopl.cpp -o output/dbopl.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_cards/dmairq.c -o output/dmairq.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/dpmi/dpmi.c -o output/dpmi.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/dpmi/dpmi_dj2.c -o output/dpmi_dj2.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/dpmi/dpmi_tsr.c -o output/dpmi_tsr.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/newfunc/fpu.c -o output/fpu.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/dpmi/gormcb.c -o output/gormcb.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c hdpmipt.c -o output/hdpmipt.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c main.c -o output/main.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/newfunc/memory.c -o output/memory.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/newfunc/nf_dpmi.c -o output/nf_dpmi.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/opl3emu.cpp -o output/opl3emu.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_cards/pcibios.c -o output/pcibios.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/pic.c -o output/pic.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c qemm.c -o output/qemm.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/sbemu.c -o output/sbemu.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_cards/sc_e1371.c -o output/sc_e1371.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_cards/sc_ich.c -o output/sc_ich.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_cards/sc_inthd.c -o output/sc_inthd.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_cards/sc_sbl24.c -o output/sc_sbl24.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_cards/sc_sbliv.c -o output/sc_sbliv.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/au_cards/sc_via82.c -o output/sc_via82.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/newfunc/string.c -o output/string.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c test.c -o output/test.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/newfunc/threads.c -o output/threads.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/newfunc/time.c -o output/time.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c mpxplay/newfunc/timer.c -o output/timer.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/untrapio.c -o output/untrapio.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c utility.c -o output/utility.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/vdma.c -o output/vdma.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/virq.c -o output/virq.o
gcc -I./mpxplay -I./sbemu -Os -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -c sbemu/dpmi/xms.c -o output/xms.o

gcc -march=i386 -D__DOS__ -DSBEMU -DDEBUG=0 -o output/sbemu.exe \
output/ac97_def.o \
output/au_cards.o \
output/cv_bits.o \
output/cv_chan.o \
output/cv_freq.o \
output/dbgutil.o \
output/dbopl.o \
output/dmairq.o \
output/dpmi.o \
output/dpmi_dj2.o \
output/dpmi_tsr.o \
output/fpu.o \
output/gormcb.o \
output/hdpmipt.o \
output/main.o \
output/memory.o \
output/nf_dpmi.o \
output/opl3emu.o \
output/pcibios.o \
output/pic.o \
output/qemm.o \
output/sbemu.o \
output/sc_e1371.o \
output/sc_ich.o \
output/sc_inthd.o \
output/sc_sbl24.o \
output/sc_sbliv.o \
output/sc_via82.o \
output/string.o \
output/test.o \
output/threads.o \
output/time.o \
output/timer.o \
output/untrapio.o \
output/utility.o \
output/vdma.o \
output/virq.o \
output/xms.o \
-lstdc++ -lm
