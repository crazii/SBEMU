#ifndef _SBEMUCFG_H_
#define _SBEMUCFG_H_

#define SBEMU_SAMPLERATE    22050 //not used anymore.
//#define SBEMU_SAMPLERATE    44100 //not used anymore.

#define SBEMU_CHANNELS 2

#define SBEMU_BITS 16

//enable Virtual MPU by using TinySoundFont
#define SBEMU_VMPU 1

//enable Gravis UltraSound (GUS) emulation
#define SBEMU_GUS 1

//amplify OPL volume by 1.5. should be 0 or 1
//NOTE: the DBOPL emulation has lower volume, and DOSBox will set the volume to 1.5x too
//reference: https://github.com/dosbox-staging/dosbox-staging/issues/278 : OPL audio 1.5x scaling
//reference: https://github.com/dosbox-staging/dosbox-staging/blob/main/src/hardware/audio/opl.cpp
#define SBEMU_OPL_VOLUME_AMPLICATION 1

//threshold for fixtc. if sample rate difference larger than this, fixtc will be disabled
//set to 0 to alwasy use fixtc ignoring the difference
//this is the default threshold for fixtc, can be overridded by command line.
#define SBEMU_FIXTC_THRESHOLD 1000

//swap left/right chanel (SFX only)
//it has historical reasons related to the legacy SB cards so previously I was reluctant to do it.
#define SBEMU_SWAP_STEREO 1

//mixing method 2: (SFX*a+PCM*b)
#define SBEMU_LINEAR_MIX 1

//static volume balancing (because DBOPL volume is lower than real HW, even with SBEMU_OPL_VOLUME_AMPLICATION=1)
#define SBEMU_LINEAR_MIX_FRACTION 16
#define SBEMU_LINEAR_MIX_SFX_RATIO 4

#if SBEMU_LINEAR_MIX
#define SBEMU_SFX_RATIO SBEMU_LINEAR_MIX_SFX_RATIO/SBEMU_LINEAR_MIX_FRACTION //don't use () for int math, careful on use
#define SBEMU_OPL_RATIO (SBEMU_LINEAR_MIX_FRACTION-SBEMU_LINEAR_MIX_SFX_RATIO)/SBEMU_LINEAR_MIX_FRACTION //don't use () for int math, careful on use
#else
#define SBEMU_SFX_RATIO 1
#define SBEMU_OPL_RATIO 1
#endif

//master volume
#define SBEMU_VOLUME_MAX 100

#endif//_SBEMUCFG_H_