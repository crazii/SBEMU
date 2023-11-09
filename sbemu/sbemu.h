#ifndef _SBEMU_H_
#define _SBEMU_H_
//Sound blaster emulation
//Sound Blaster Series Hardware Programming Guide: https://www.phatcode.net/articles.php?id=243
//https://github.com/joncampbell123/dosbox-x/wiki/Hardware:Sound-Blaster:DSP-commands

#include <stdint.h>

//address: 2x?. those are only offsets
#define SBEMU_PORT_FM_LADDR     0x00    //left addr(W), left status(R)
#define SBEMU_PORT_FM_LDATA     0x01
#define SBEMU_PORT_FM_RADDR     0x02    //right addr(W), right status(R)
#define SBEMU_PORT_FM_RDATA     0x03
#define SBEMU_PORT_MIXER        0x04
#define SBEMU_PORT_MIXER_DATA   0x05
#define SBEMU_PORT_DSP_RESET    0x06
#define SBEMU_PORT_DSP_READ     0x0A
#define SBEMU_PORT_DSP_WRITE_WS 0x0C //cmd/data (W), write buffer status(R)
#define SBEMU_PORT_DSP_RS       0x0E //read buffer status (R). reading this port also acknowledge 8bit interrupt 
#define SBEMU_PORT_DSP_16ACK    0x0F //acknowledge 16bit interrupt

//register index trhough mixer port
#define SBEMU_MIXERREG_RESET        0x00

// CT1335 mixer (SB 2.0)
#define SBEMU_MIXERREG_MASTERVOL    0x02
#define SBEMU_MIXERREG_MIDIVOL      0x06
#define SBEMU_MIXERREG_CDVOL        0x08
#define SBEMU_MIXERREG_VOICEVOL     0x0A

// CT1345 mixer (SB Pro)
#define SBEMU_MIXERREG_VOICESTEREO  0x04
#define SBEMU_MIXERREG_MODEFILTER   0x0E
#define SBEMU_MIXERREG_MASTERSTEREO 0x22
#define SBEMU_MIXERREG_MIDISTEREO   0x26
#define SBEMU_MIXERREG_CDSTEREO     0x28

// CT1745 mixer (SB 16)
#define SBEMU_MIXRREG_MASTERL       0x30
#define SBEMU_MIXRREG_MASTERR       0x31
#define SBEMU_MIXRREG_VOICEL        0x32
#define SBEMU_MIXRREG_VOICER        0x33
#define SBEMU_MIXRREG_MIDIL         0x34
#define SBEMU_MIXRREG_MIDIR         0x35
#define SBEMU_MIXRREG_CDL           0x36
#define SBEMU_MIXRREG_CDR           0x37

// Special mixer registers (dynamic reconfiguration of INT/DMA)
#define SBEMU_MIXERREG_INT_SETUP    0x80
#define SBEMU_MIXERREG_DMA_SETUP    0x81
#define SBEMU_MIXERREG_INT_STS      0x82

//DSP commands
#define SBEMU_CMD_SET_TIMECONST     0x40
#define SBEMU_CMD_SET_SIZE          0x48 //size-1
#define SBEMU_CMD_SET_SAMPLERATE    0x41 //set sample rate
#define SBEMU_CMD_SET_SAMPLERATE_I  0x42 //intput sample rate
#define SBEMU_CMD_CONTINUE_AUTO     0x45
#define SBEMU_CMD_HALT_DMA          0xD0
#define SBEMU_CMD_DAC_SPEAKER_ON    0xD1
#define SBEMU_CMD_DAC_SPEAKER_OFF   0xD3
#define SBEMU_CMD_CONTINUE_DMA      0xD4
#define SBEMU_CMD_HALT_DMA16        0xD5
#define SBEMU_CMD_CONTINUE_DMA16    0xD6
#define SBEMU_CMD_DSP_ID            0xE0
#define SBEMU_CMD_DSP_GETVER        0xE1 //1st byte major, 2nd byte minor
#define SBEMU_CMD_DSP_DMA_ID        0xE2
#define SBEMU_CMD_DSP_COPYRIGHT     0xE3
#define SBEMU_CMD_DSP_WRITE_TESTREG 0xE4
#define SBEMU_CMD_DSP_READ_TESTREG  0xE8
#define SBEMU_CMD_TRIGGER_IRQ       0xF2
#define SBEMU_CMD_TRIGGER_IRQ16     0xF3

//time constant used
#define SBEMU_CMD_8BIT_DIRECT       0x10
#define SBEMU_CMD_8BIT_OUT_1        0x14 //single cycle
#define SBEMU_CMD_8BIT_OUT_AUTO     0x1C
#define SBEMU_CMD_8BIT_OUT_1_HS     0x91 //high speed mode, need a reset(acutally restore to previous) to exit hs mode
#define SBEMU_CMD_8BIT_OUT_AUTO_HS  0x90
#define SBEMU_CMD_4BIT_OUT_1        0x75 //4bit ADPCM
#define SBEMU_CMD_4BIT_OUT_1_NREF   0x74
#define SBEMU_CMD_4BIT_OUT_AUTO     0x7D
#define SBEMU_CMD_3BIT_OUT_1        0x77 //3bit ADPCM
#define SBEMU_CMD_3BIT_OUT_1_NREF   0x76
#define SBEMU_CMD_3BIT_OUT_AUTO     0x7F
#define SBEMU_CMD_2BIT_OUT_1        0x17 //2bit  ADPCM
#define SBEMU_CMD_2BIT_OUT_1_NREF   0x16
#define SBEMU_CMD_2BIT_OUT_AUTO     0x1F

//sample rate used
//fllowing commands
#define SBEMU_CMD_8OR16_8_OUT_1     0xC0
#define SBEMU_CMD_8OR16_8_OUT_AUTO  0xC6
#define SBEMU_CMD_8OR16_8_OUT_AUTO_NOFIFO 0xC4 //undocumented, FIFO bit(bit 1) off. from DOSBox source comments
#define SBEMU_CMD_8OR16_16_OUT_1    0xB0
#define SBEMU_CMD_8OR16_16_OUT_AUTO 0xB6
#define SBEMU_CMD_8OR16_16_OUT_AUTO_NOFIFO 0xB4
//fllowiing modes
#define SBEMU_CMD_MODE_PCM8_MONO    0x00
#define SBEMU_CMD_MODE_PCM8_STEREO  0x20
#define SBEMU_CMD_MODE_PCM16_MONO   0x10
#define SBEMU_CMD_MODE_PCM16_STEREO 0x30

#define SBEMU_CMD_EXIT_8BIT_AUTO    0xDA
#define SBEMU_CMD_EXIT_16BIT_AUTO   0xD9
#define SBEMU_CMD_PAUSE_8BIT        0xD0
#define SBEMU_CMD_CONTINUE_8BIT     0xD4
#define SBEMU_CMD_PAUSE_16BIT       0xD5
#define SBEMU_CMD_CONTINUE_16BIT    0xD6
#define SBEMU_CMD_PAUSE_DAC         0x80 //pause by samples

typedef struct //external functions
{
    void(*StartPlayback)(void);     //start playing real hardware sound - not used.
    void (*RaiseIRQ)(uint8_t);      //raise virtual IRQ - not used (not working)
    void(*DMA_Write)(int,uint8_t);  //write DMA, (channel, value)
    uint32_t (*DMA_Size)(int);      //Get DMA size (channel)
}SBEMU_EXTFUNS;

#ifdef __cplusplus
extern "C"
{
#endif

//generic IO functions
void SBEMU_Mixer_WriteAddr(int16_t port, uint8_t value);
void SBEMU_Mixer_Write(uint16_t port, uint8_t value);
uint8_t SBEMU_Mixer_Read(uint16_t port);

void SBEMU_DSP_Reset(uint16_t port, uint8_t value);
void SBEMU_DSP_Write(uint16_t port, uint8_t value);
uint8_t SBEMU_DSP_Read(uint16_t port);
uint8_t SBEMU_DSP_WriteStatus(uint16_t port);
uint8_t SBEMU_DSP_ReadStatus(uint16_t port); //read buffer status. 
uint8_t SBEMU_DSP_INT16ACK(uint16_t port);

//used by emulations
void SBEMU_Init(int irq, int dma, int hdma, int DSPVer, int FixTC, SBEMU_EXTFUNS* extfuns); //extfuns must be persistent
uint8_t SBEMU_GetIRQ();
uint8_t SBEMU_GetDMA();
uint8_t SBEMU_GetHDMA();
int SBEMU_HasStarted();
void SBEMU_Stop();

int SBEMU_GetDACSpeaker();
int SBEMU_GetBits();
int SBEMU_GetChannels();
int SBEMU_GetSampleRate();
int SBEMU_GetSampleBytes();
int SBEMU_GetAuto();
int SBEMU_GetPos(); //get pos in bytes
int SBEMU_SetPos(int pos); //set pos in bytes
int SBEMU_IRQTriggered();
void SBEMU_SetIRQTriggered(int triggered);
uint8_t SBEMU_GetMixerReg(uint8_t index);

//for 4/3/2bit
int SBEMU_DecodeADPCM(uint8_t* adpcm, int bytes); //decode in place

//for SBEMU_CMD_8BIT_DIRECT
int SBEMU_GetDirectCount();
void SBEMU_ResetDirect();
const uint8_t* SBEMU_GetDirectPCM8();

//hacks/workarounds
int SBEMU_GetDetectionCounter();
void SBEMU_SetDetectionCounter(int c);

#ifdef __cplusplus
}
#endif

#endif//_SBEMU_H_