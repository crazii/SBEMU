$ErrorActionPreference = "Stop"

$GCC = "C:\djgpp\bin\i586-pc-msdosdjgpp-gcc.exe"
$GXX = "C:\djgpp\bin\i586-pc-msdosdjgpp-g++.exe"
$TARGET = "output\sbemu.exe"

$INCLUDES = "-I.\mpxplay", "-I.\sbemu", "-I.\drivers\include"
$DEFINES = "-D__DOS__", "-DSBEMU", "-DDEBUG=0", "-DMAIN_SBEMU_VER=`"1.0`"", "-DNDEBUG"
$CFLAGS = "-fcommon", "-march=i386", "-ffast-math", "-O2", "-flto"
$LDFLAGS = "-lstdc++", "-lm", "-Wno-attributes", "-s"

$CARDS_SRC = @(
    "mpxplay\au_cards\ac97_def.c", "mpxplay\au_cards\au_base.c", "mpxplay\au_cards\au_cards.c",
    "mpxplay\au_cards\au_linux.c", "mpxplay\au_cards\dmairq.c", "mpxplay\au_cards\pcibios.c",
    "mpxplay\au_cards\ioport.c", "mpxplay\au_cards\sc_e1371.c", "mpxplay\au_cards\sc_ich.c",
    "mpxplay\au_cards\sc_cmi.c", "mpxplay\au_cards\sc_inthd.c", "mpxplay\au_cards\sc_sbl24.c",
    "mpxplay\au_cards\sc_sbliv.c", "mpxplay\au_cards\sc_via82.c", "mpxplay\au_cards\sc_null.c",
    "mpxplay\au_cards\sc_ymf.c"
)

$CTXFI_SRC = @(
    "drivers\ctxfi\ctsrc.c", "drivers\ctxfi\ctresource.c", "drivers\ctxfi\ctmixer.c",
    "drivers\ctxfi\ctimap.c", "drivers\ctxfi\ctamixer.c", "drivers\ctxfi\ctatc.c",
    "drivers\ctxfi\cttimer.c", "drivers\ctxfi\ctdaio.c", "drivers\ctxfi\ctpcm.c",
    "drivers\ctxfi\cthardware.c", "drivers\ctxfi\ctvmem.c", "drivers\ctxfi\cthw20k1.c",
    "drivers\ctxfi\cthw20k2.c", "mpxplay\au_cards\sc_ctxfi.c"
)

$EMU10K1_SRC = @("drivers\emu10k1\emu10k1x.c", "mpxplay\au_cards\sc_emu10k1x.c")
$TRIDENT_SRC = @("drivers\trident\trident_main.c", "drivers\trident\trident_memory.c", "mpxplay\au_cards\sc_trident.c")
$ALS4000_SRC = @("drivers\als4000\als4000.c", "drivers\als4000\sb_common.c", "drivers\als4000\sb_mixer.c", "mpxplay\au_cards\sc_als4000.c")
$OXYGEN_SRC = @(
    "drivers\oxygen\xonar_dg.c", "drivers\oxygen\xonar_dg_mixer.c", "drivers\oxygen\xonar_lib.c",
    "drivers\oxygen\oxygen.c", "drivers\oxygen\oxygen_io.c", "drivers\oxygen\oxygen_lib.c",
    "drivers\oxygen\oxygen_pcm.c", "drivers\oxygen\oxygen_mixer.c", "mpxplay\au_cards\sc_oxygen.c"
)
$ALLEGRO_SRC = @("drivers\maestro3\maestro3.c", "mpxplay\au_cards\sc_allegro.c")

$SBEMU_SRC = @(
    "sbemu\dbopl.cpp", "sbemu\opl3emu.cpp", "sbemu\pic.c", "sbemu\sbemu.c", "sbemu\iotrap.c",
    "sbemu\vdma.c", "sbemu\serial.c", "sbemu\vgus.c", "sbemu\dpmi\xms.c", "sbemu\dpmi\dpmi.c",
    "sbemu\dpmi\dbgutil.c", "sbemu\dpmi\dpmi_dj2.c", "sbemu\dpmi\dpmi_tsr.c",
    "sbemu\dpmi\djgpp\gormcb.c", "sbemu\dpmi\djgpp\gopint.c", "main.c", "vdpmi.c", "utility.c", "sbemu\vmpu.c", "sbemu\vdisney.c", "sbemu\vpcspeaker.c"
)

$ALL_SRC = $CARDS_SRC + $CTXFI_SRC + $EMU10K1_SRC + $TRIDENT_SRC + $ALS4000_SRC + $OXYGEN_SRC + $ALLEGRO_SRC + $SBEMU_SRC

$OBJS = @()

if (!(Test-Path "output")) { New-Item -ItemType Directory -Path "output" | Out-Null }

$env:DJGPP="C:\djgpp\djgpp.env"
$env:PATH="C:\djgpp\bin;"+$env:PATH

foreach ($file in $ALL_SRC) {
    if ($file -match '\.c$') {
        $obj = "output\" + ($file -replace '^.*\\', '') -replace '\.c$', '.o'
        $OBJS += $obj
        Write-Host "CC  $file"
        & $GCC $CFLAGS $INCLUDES $DEFINES -c $file -o $obj
    }
    elseif ($file -match '\.cpp$') {
        $obj = "output\" + ($file -replace '^.*\\', '') -replace '\.cpp$', '.o'
        $OBJS += $obj
        Write-Host "CXX $file"
        & $GXX $CFLAGS $INCLUDES $DEFINES -c $file -o $obj
    }
}

Write-Host "LINK $TARGET"
& $GCC $CFLAGS -o $TARGET $OBJS $LDFLAGS

Write-Host "Build complete: $TARGET"
