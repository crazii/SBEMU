#ifndef _SBEMUCFG_H_
#define _SBEMUCFG_H_

#define SBEMU_SAMPLERATE    22050 //not used anymore.
//#define SBEMU_SAMPLERATE    44100 //not used anymore.

#define SBEMU_CHANNELS 2

#define SBEMU_BITS 16

//enable Virtual MPU by using TinySoundFont
#define SBEMU_VMPU 1

//amplify OPL volume by 1.5. should be 0 or 1
//NOTE: the DBOPL emulation has lower volume, and DOSBox will set the volume to 1.5x too
//reference: https://github.com/dosbox-staging/dosbox-staging/issues/278 : OPL audio 1.5x scaling
//reference: https://github.com/dosbox-staging/dosbox-staging/blob/main/src/hardware/audio/opl.cpp
#define SBEMU_OPL_VOLUME_AMPLICATION 1

//threshold for fixtc. if sample rate difference larger than this, fixtc will be disabled
//set to 0 to alwasy use fixtc ignoring the difference
//this is the default threshold for fixtc, can be overridded by command line.
#define SBEMU_FIXTC_THRESHOLD 1000

#endif//_SBEMUCFG_H_