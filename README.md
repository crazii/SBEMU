# SBEMU

Emulate Sound Blaster and OPL3 in pure DOS using modern
PCI-based (onboard and add-in card) sound cards.

## Supported sound cards

Source code from [MPXPlay](https://mpxplay.sourceforge.net/)
is used to support the following sound cards/chips.

Enabled and working:

 * `sc_ich`: Intel ICH / nForce / SIS 7012
 * `sc_inthd`: Intel High Definition Audio (HDA)
 * `sc_via82`: VIA VT82C686, VT8233/37
 * `sc_sbliv`: SB Live! / Audigy
 * `sc_sbl24`: SB Audigy LS (CA0106)
 * `sc_es1371`: Ensoniq ES1371/1373
 * `sc_cmi`: C-Media CMI8338/8738

Support compiled-in, but untested:

 * `sc_via82`: VIA VT8235

Source code exists, but "doesn't work yet":

 * `sc_sbxfi`: Creative X-Fi EMU20KX

 Additional Linux drivers ported by [jiyunomegami](https://github.com/jiyunomegami)
 * SB X-Fi (EMU20K1 & EMU20K2)
 * YAMAHA YMF7x4
 * ALS4000
 * OXYGEN(CMI8788)
 * ESS Allegro-1 (ES1988S/ES1989S)
 * Trident 4D Wave

## Emulated modes

 * 8-bit and 16-bit DMA (mono, stereo, high-speed)
 * Sound Blaster 1.0, 2.0, Pro, Pro2, 16
 * OPL3 FM via [DOSBox' OPL3 FM implementation](https://www.dosbox.com/)
 * OPL3 passthrough to Hardware FM if it's present on the PCI sound card.
 * MPU401 UART emulation, or passthrough to PCI sound card if supported.


## Requirements

 * [HDPMI32i](https://github.com/crazii/HX) (HDPMI with IOPL0)
 * Optional, for real-mode game support (I/O trapping):
   * [JEMM](https://github.com/Baron-von-Riedesel/Jemm) with QPIEMU.DLL loaded
   * or [QEMM](https://en.wikipedia.org/wiki/QEMM), commercial software

For memory management, use either:

 * `JEMMEX` only: Provides both HIMEM + EMM
 * `HIMEMX` and `JEMM386`: Separate HIMEM + EMM

In both cases, use `JLOAD` (from the Jemm distribution)
to load `QPIEMU.DLL` before starting `SBEMU`,
so that real-mode support is enabled. If you don't load
JEMM+QPIEMU (or QEMM), only protected mode applications
will be supported.

## README for End Users
If your want to use SBEMU without building it, please read [README.txt](./README.txt) for setup and a list of command line options.

## Building from source

macOS, Linux and Windows is supported. For Windows, consider using
WSL2 + Linux binaries. If you need to frequently debug/test on your
 local DOS, there's a makefile.dos for you.

### Installing a cross-compiler (DJGPP)

Scripts to build a recent GCC toolchain for DJGPP are available here:

* https://github.com/andrewwutw/build-djgpp

There's also prebuilt releases for the toolchain if you don't want
to build DJGPP yourself. The current version (October 2023) is using
GCC 12.2.0, but in the future newer GCC versions might become available:

* https://github.com/andrewwutw/build-djgpp/releases

### Installing make

This assumes a Debian/Ubuntu installation. If you are using any other
distro, I'm assuming you know your way around and can translate those
instructions to your specific distribution.

To get `make` and other tools, it's easiest to install host build tools:

   sudo apt install -y build-essential

On MacOS, install the Xcode command-line tools, which should give you
`make` and other host utilities.

If you are planning on building DJGPP from source, some additional build
tools are needed. Refer to the `build-djgpp` README file for details.

### Installing DJGPP on DOS

With the source code increasing, it's not recommended to build from DOS.
Also the DJGPP DOS build doesn't use -O2 and -flto, because the GCC version is old and buggy with -O2.

If building the project on DOS is needed, download the original DJGPP 
from here: https://www.delorie.com/djgpp/zip-picker.html It has make utility too.  
* Select `MS-DOS,OpenDOS,PC-DOS` in the `Which operating system will you be using?` drop down,
* Check `C++` checkbox on `Which programming languages will you be using?`
* Click `Tell me which files I need`
* Unpack all the zip files into a same folder, and put it on your DOS partition (i.e. C:\DJGPP)

[DOSLFN](http://www.adoxa.altervista.org/doslfn/) is also need to perform build.
The PATH env needs to be set properly before building.
`set PATH=%PATH%;C:\DJGPP\BIN` is recommended to be put in autoexec.bat,
and then

`make -f makefile.dos`  

You can also uses RHIDE to perform editing & building on the fly:
add `SET DJGPP=C:\DJGPP\DJGPP.ENV` to autoexec.bat
and then just run `rhide` in the project root via command line.
Use `Alt+C` to active `Compile` menu and select `Make` for dependency build
or `Build all` for a clean build.

### Building the project

The `bin` folder of your DJGPP toolchain needs to be in your `$PATH`,
so that the following command works and outputs your DJGPP GCC version:

    i586-pc-msdosdjgpp-gcc --version

If this works, building the project is as simple as:

    make

Because you are on a modern machine with multi-core CPUs, do a parallel
build, which is faster, for example, for a quad-core CPU, use 8 parallel
processes to speed up building:

    make -j8

After the build is done, you'll find the build result in a folder called
`output`, i.e. `output/sbemu.exe`.

## Feature usage

### CD Audio

CD audio support in DOS requires two parts:

1. Audio control (play/pause/seek/...) via `MSCDEX` (or `SHSUCDX`)
2. Volume control via the mixer

For part one, you need to have a CD-ROM drive with analog audio out
and an MSCDEX-compatible CD-ROM driver set up.

Part two (volume control) is taken care of by SBEMU on startup.

To adjust the volume of CD-Audio (by default it's 100% volume),
you can use any Sound Blaster-compatible program, such as "SBMIX",
as SBEMU does emulate and forward CD-Audio mixer settings.

Don't forget that to actually hear anything, you need to connect
an analog audio cable from your CD-ROM drive to the 4-pin CD-IN
header on your soundcard (or motherboard for onboard sound).


### Debug output on serial

You can configure SBEMU to output its debug messages to the serial
port instead of the console. This also works in the background when
games are full-screen, and so is really useful for debugging.

To build SBEMU with debug output, use:

    make DEBUG=1

Then, launch SBEMU with this command for debug output (9600, 8N1)
on COM1 (use `/DBG2` for COM2):

    sbemu /DBG1

To disable serial port debug output at runtime, use:

    sbemu /DBG0

Serial debug output is disabled by default.
