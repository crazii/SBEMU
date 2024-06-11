README of SBEMU

1. Introduction

SBEMU is a protected mode TSR program/driver that aims to emulate the legacy ISA Sound Blaster hardware on PCI sound cards,
it uses the port trapping features of a v86 monitor & DPMI host to trap SB ports (i.e. 220h), and ISA DMA controller ports to
provide a 'virtual' SB devices for DOS programs.
It works both for realmode & protected mode programs.

Supported sound blaster emulation: SB, SBpro, SBpro2 (SBpro with OPL3), SB16.

SBEMU gives special thanks to DOSBox and MPXPlay.
The OPL emulation code is from DOSBox (https://www.dosbox.com/)
and the sound card drivers are from MPXPlay (https://mpxplay.sourceforge.net/)
Additional Linux drivers ported by jiyunomegami (https://github.com/jiyunomegami)

Please visit: https://github.com/crazii/SBEMU for a list of supported PCI sound cards.
and check https://github.com/crazii/SBEMU/releases for new releases.

2. Requirements

    Hardware:
        one legacy PC with a PCI sound card, or more modern UEFI PC with CSM that can boot from DOS.
    Software:
        a) MSDOS 6.22 or higher, or FreeDOS.
        b) a v86 monitor - normally an Expanded Memory Manager(EMM like EMM386) that support port trapping, currently EMMs
        that support port trapping and recognized by SBEMU:
            Quarterdeck QEMM
            JEMM386/JEMMEX with QEMM port trap emulation (QPIEMU.DLL)
        c) a DPMI host that support port trapping, currently "HDPMI32i"

        NOTE: EMM386.EXE also support port trapping, but the IO port cannot be below 100h, which makes SBEMU unable to
        trap the DMA.

    NOTE: HDPMI and JEMMEX are used by SBEMU, but not part of SBEMU, nor maintained by the same author,
    Although the HDPMI used by SBEMU is a modified edition.
    For more details:
    https://github.com/crazii/HX
    https://github.com/Baron-von-Riedesel/HX
    https://github.com/Baron-von-Riedesel/Jemm

3. Typical Setup

CONFIG.SYS:
`REM this will load JEMMEX, the EMM for DOS, turning off EMS`
`DEVICE=JEMMEX.EXE X2MAX=8192 NOEMS`

AUTOEXEX.BAT:
`REM load the QEMM QPI's port trap emulation module of JEMMEX`
`JLOAD.EXE QPIEMU.DLL`
`REM this will install HDPMI, the DPMI host. -x with memory limit, check for 'Memory problem' for more details`
`HDPMI32i -r -x`
`REM install SBEMU with default options`
`REM read README.txt to learn more about options`
`SBEMU`

4. Command line options

The command line options format is `/OPTION[VALUE]`, there's NO space, or ':' or '='.
If 'VALUE' is not present, then VALUE equals 1, to make it easy on simple switches.

NOTE: after SBEMU installs, you can change them by a another run of SBEMU with newly changed options.

/?: Show a brief description on the command line options.

/A: Specify the SB base IO address, usually 220 will work, i.e. /A220, default: 220

/I: SB interrupt request number, valid values: /I5, /I7 /I9. if one is not working properly due to hardware conflicts,
    you may try to use another one. Default: 7

/D: DMA channel of SB used, valid value: 0,1,3. usually /D1 works fine. 
    To get maximum compatibility, /D2 is not recommended because
    DMA channel 2 is for legacy floppy disk controller, and should be avoided.

/H: 16 bit "high" DMA channel for SB16/AWE, usually 5, 6, or 7. default: /H5.
    Used for SB16 emulation only.

    IBM ATs have 2 DMA controller cascaded, each have 4 channels, 
    one controller (channel 0-3) uses 8 bit transfer and another (channel 4-7) is 16 bit.
    The 16 bit channels are so called "high" DMA channels.

    /H4 (or /D4) is not valid because DMA channel 4 is used for cascading the other DMA controller.

    Some game will use, or be configured to use 16 bit DMA
    transfer, on such case, this value is used. 
       
    SB16 also support 16 bit PCM via 8 bit low DMA channel, on such case, use /H1 /H3 may achieve the emulation,
    but it is no recommended for compatibility reason. And if H0 through H3 is set, 
    then both the /H and /D parameter will be forced to be the same value.

/T: Set sound blaster emulation type (1-6), type has the same meaning as in BLASTER env.
    1: SB 1.0/1.5
    2: SB Pro 1 (or old)
    3: SB 2.0
    4: SB Pro 2 (or Pro new, with OPL3)
    5: SB Pro 2 (MCA - microchannel)
    6: SB16
    NOTE:   For compatibility reason, SBEMU won't set 'Tx' in BLASTER environment variable, except T6 for /T6.
            Some games will ignore the 'T' settings in BLASTER, especially for auto-detection. SBEMU should
            work for such cases, because no matter what '/T' is set, the full emulation is not stripped.

/OPL: Enable the OPL FM emulation, usually for music. default: 1.

/PM: Enable SB for protected mode programs, default: 1. /PM0 to turn it off.

/RM: Enable SB for real mode programs, default: 1. /RM0 to turn if off.
    /RM require an EMM that support port trapping, if it doesn't loaded, /RM will be turned off automatically with a warning.

/O: This option is only for Intel HDA. 
    /O0: output to headphone.
    /O1: output to speaker.
    default: /O1.
    Try changing it if SBEMU doesn't have any sounds for your HDA, or use /O0 if you're using a headphone.

/VOL: Set the volume of sound card, it will set to the real sound card directly. Range: 0-9, default: /VOL7
    NOTE: This is the volume for the real PCI sound card, 
    if you change the emulated SB volume via SB utility like SBMIX, this volume won't change, unless you run 'SBEMU /VOLx' again.

/K: Set the sample rate for the real sound card. Default: /K22050
    It's not recommended to set value above 44100 for games, as most DOS games output 11025/22050 Hz.
    But you may try to set it higher if your want, or for music players.

/FIXTC: Use fixed time constant. Default: /FIXTC0 (disabled)
    Old SB hardware doesn't set sample rate directly, but using a byte of 'time-constant' to calculate sample rate indirectly,
    when 'time-constant' is used, there's some inevitable PRECISION LOSS: if a game set sample rate to 22050 through time-constant,
    the real sample rate calculated back from the time constant is not exactly 22050, and SBEMU will try to fix it using
    the most near and commonly used sample rates (11025/22050 etc.)

    LONG STORY SHORT: 22050 => time constant => NEWVALUE where NEWVALUE is not essentially 22050, 
    It is designed this way and there's little can be done. SBEMU tries to fix it to 22050 with /FIXTC

    With this switch on, it makes SBEMU works more efficiently because the PCM resample will be skipped or simplified.
    If you have faced any sound pops/noises/stuttering, you can try using /FIXTC

/SCL: List the PCI sound cards installed on your PC. Each will be shown with a number before it.
    NOTE: this option only works BEFORE SBEMU installs,
    after installation SBEMU will work on the chosen sound card and not suitable to probe/test sound cards anymore.

/SC: Select specific sound card if you have more sound cards installed. The number is taken from '/SCL' output.
    Example: '/SCL' output:
    1: Intel HDA: blah blah...
    2: CMI 8338/8738: blah blah..
    By default SBEMU will use the number 1 card it detects, but if you want to use the 2nd one, use
    '/SCL2' to select the 2nd card for SB emulation.

/SCFM: if a PCI sound card has hardware FM (like YMF 7x4 or some CMI cards), /SCFM is used to choose the card for hardware FM.
    Example: like /SC above, because the 1st sound card 'Intel HDA' has no HW FM, but assume the 2nd one (CMI) has HW FM,
    then you can use /SCFM2 to use it for FM output, while keep /SC1 to use Intel HDA for the SB digital SFX emulation.
    NOTE: for CMI cards, this option is not effective if it is not specified in command line, 
    i.e. you have only 1 CMI card, and by default hardware FM is not enabled unless you set /SCFM (or /SCFM1).
    for YMF cards, hardware FM will be enabled by default.

/SCMPU: like /SCFM but for MPU401 General MIDI support.

/R: Reset sound card driver. Normally it's not needed but if your game sound is not working suddenly, due to
    hardware/software compatibility reason, or bugs, you can try to reset/re-init the sound card driver.
    SBEMU will try to reset the sound card to its initial states as when SBEMU installs.
    This options is only useful `AFTER` SBEMU installs.   


5. Trouble Shooting

a) VCPI issues. Known games: NFS, FastDoom

Some games/DOS Extenders (like DOS32A) won't get any sounds because it choose to use VCPI but not DPMI. Because VCPI mode cannot be io-trapped,
SBEMU cannot work on it. To force the games to use DPMI, turn off VPCI for JEMM386/JEMMEX `AFTER` SBEMU installs:
'JEMMEX NOVCPI'


b) Memory problem.

Some protected mode games refuse to work if there're too much DPMI memory, so the command line parameter '-x' for HDPMI is
used to limit the DPMI memory. it is always recommended to get max compatibility for gaming.
'HDPMI32i -r -x'
NOTE: -r will make HDPMI resident (TSR).
-x will limit the DPMI memory to 256M, -x1 will make it 128M, -x2 to 64M, and so on.

Some real mode games won't work either if XMS is too much, so it is recommended to set XMS for JEMMEX to 8192K in CONFIG.SYS.
Some real games won't work properly (i.e. no SFX) if the XMS is more than 8192.
`DEVICE=JEMMEX.EXE X2MAX=8192`
NOTE: this option is not tested for all rm games, you can find out a value that works for you.


c) Other

Usually you don't need EMS, use 'NOEMS' for JEMMEX
`DEVICE=JEMMEX.EXE X2MAX=8192 NOEMS`

Some games require EMS to work (i.e. Aladdin), on such case the 'NOEMS' needs to be removed.

And some games may not work properly with EMS enabled on modern PC, you need check the manual to see if 
there's option to disable EMS for the game.
i.e. WOLF3D won't work properly on Thinkpad T540p with EMS enabled, use 
'WOLF3D NOEMS' will work, or create a .BAT file for convenience.


FAQ:


Q1: Do I need to set BLASTER environment variable before running SBEMU?

Usually it is not needed. After SBEMU installs, it will set the BLASTER for you.
But if you set it before running SBEMU, SBEMU will read it and use as options,
and if the command line options conflicts with BLASTER env, the command line options are used.
Example1:
`
SET BLASTER=A220 I5 D1
SBEMU
`
is equivalent to `SBEMU /A220 /I5 /D1` without setting BLASTER.

Example2:
`
SET BLASTER=A220 I5 D1
SBEMU /I7
`
There're IRQ in both BLASTER (I5) and commandline (/I7), then IRQ7 will be the working one.

NOTE: For compatibility reasons, SBEMU won't set T in BLASTER unless /T6 is used, if you need T in BLASTER,
you can set it manually AFTER SBEMU installs.


Q2: DO I need LH for SBEMU to load it into UMB?

No. SBEMU is a DPMI client and most memory used by SBEMU are DPMI memories located above 1M.
There're some tiny pieces of DOS resident memory (below 1M) that SBEMU will try to load it to UMB automatically,
Currently the PSP's still resident in low memory, but that will be optimized later.


Q3: Some games aren't running with SBEMU, or SBEMU freezes on startup, or I ain't getting any sounds, where to report the issue?

Please create issues here: https://github.com/crazii/SBEMU/issues
It helps if you post additional details:
    the spec of your PC
    the DOS edition & version
    the Game title and edition & version, and where/when the problem appears
    the build/release of SBEMU, if you don't know, you can check the startup message (the first line) of running SBEMU
    what's the output of `SBEMU /SCL`
    what's your command line options
    have you tried other options, especially other IRQs
    what's the result if only with HDPMI32i, and without SBEMU

Because SBEMU is a hobby project, there's no guarantee that the problem will be fixed immediately or very soon enough,
and the contributors might not response in time, please be patient.


Q4: I want help with SBEMU's code, i.e. I've fixed a bug or want to add new features/drivers, is it OK to create a Pull-Request on Github?

Yes. Any PRs that makes SBEMU better are welcomed.
