//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: VIA VT82C686, VT8233 (VT8235?) low level routines (onboard chips on AMD Athlon mainboards)
//some routines are based on the ALSA (http://www.alsa-project.org)

//#define MPXPLAY_USE_DEBUGF 1
#define VIA_DEBUG_OUTPUT stdout

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_VIA82XX

#include <string.h>
#include "dmairq.h"
#include "pcibios.h"
#include "ac97_def.h"

// ac97
#define VIA_REG_AC97_CTRL               0x80
#define VIA_REG_AC97_CODEC_ID_PRIMARY   0x00000000
#define VIA_REG_AC97_PRIMARY_VALID    (1<<25)
#define VIA_REG_AC97_BUSY        (1<<24)
#define VIA_REG_AC97_READ        (1<<23)
#define VIA_REG_AC97_WRITE              0
#define VIA_REG_AC97_CMD_SHIFT            16
#define VIA_REG_AC97_CMD_MASK           0x7F
#define VIA_REG_AC97_DATA_MASK            0xffff

#define VIA_REG_OFFSET_STATUS         0x00    /* byte - channel status */
#define VIA_REG_STATUS_FLAG 0x01
#define VIA_REG_STATUS_EOL  0x02

#define VIA_REG_OFFSET_CONTROL         0x01    /* byte - channel control */
#define  VIA_REG_CTRL_START         0x80    /* WO */
#define  VIA_REG_CTRL_TERMINATE         0x40    /* WO */
#define  VIA_REG_CTRL_PAUSE         0x08    /* RW */
#define  VIA_REG_CTRL_RESET         0x01    /* RW - probably reset? undocumented */

#define VIA_REG_OFFSET_TYPE          0x02    /* byte - channel type */
#define  VIA_REG_TYPE_INT_LSAMPLE    0x04    /* interrupt on last sample sent */
#define  VIA_REG_TYPE_INT_EOL         0x02   /* interrupt on end of link */
#define  VIA_REG_TYPE_INT_FLAG         0x01  /* interrupt on flag */

#define VIA_REG_OFFSET_TABLE_PTR     0x04    /* dword - channel table pointer (W) */
#define VIA_REG_OFFSET_CURR_PTR      0x04    /* dword - channel current pointer (R) */
#define VIA_REG_PLAYBACK_CURR_COUNT  0x0C    /* dword - channel current count */

// VT8233
#define VIA_REG_CTRL_AUTOSTART         0x20
#define VIA_REG_OFFSET_STOP_IDX      0x08    /* dword - stop index, channel type, sample rate */
#define VIA_REG_TYPE_AUTOSTART         0x80    /* RW - autostart at EOL */
#define VIA_REG_TYPE_16BIT         0x20    /* RW */
#define VIA_REG_TYPE_STEREO         0x10    /* RW */
#define VIA8233_REG_TYPE_16BIT         0x00200000    /* RW */
#define VIA8233_REG_TYPE_STEREO         0x00100000    /* RW */
#define VIA_REG_OFFSET_CURR_INDEX    0x0f    /* byte - channel current index */
#define VIA_REG_OFS_PLAYBACK_VOLUME_L 0x02
#define VIA_REG_OFS_PLAYBACK_VOLUME_R 0x03
#define VIA_TBL_BIT_EOL        0x80000000
#define VIA_TBL_BIT_FLAG       0x40000000
#define VIA_TBL_BIT_STOP       0x20000000

#define VIA_ACLINK_STAT        0x40
#define  VIA_ACLINK_C11_READY    0x20
#define  VIA_ACLINK_C10_READY    0x10
#define  VIA_ACLINK_C01_READY    0x04 /* secondary codec ready */
#define  VIA_ACLINK_LOWPOWER    0x02 /* low-power state */
#define  VIA_ACLINK_C00_READY    0x01 /* primary codec ready */
#define VIA_ACLINK_CTRL        0x41
#define  VIA_ACLINK_CTRL_ENABLE    0x80 /* 0: disable, 1: enable */
#define  VIA_ACLINK_CTRL_RESET    0x40 /* 0: assert, 1: de-assert */
#define  VIA_ACLINK_CTRL_SYNC    0x20 /* 0: release SYNC, 1: force SYNC hi */
#define  VIA_ACLINK_CTRL_SDO    0x10 /* 0: release SDO, 1: force SDO hi */
#define  VIA_ACLINK_CTRL_VRA    0x08 /* 0: disable VRA, 1: enable VRA */
#define  VIA_ACLINK_CTRL_PCM    0x04 /* 0: disable PCM, 1: enable PCM */
#define  VIA_ACLINK_CTRL_FM    0x02 /* via686 only */
#define  VIA_ACLINK_CTRL_SB    0x01 /* via686 only */

#define  VIA_ACLINK_CTRL_INIT    (VIA_ACLINK_CTRL_ENABLE|\
                 VIA_ACLINK_CTRL_RESET|\
                 VIA_ACLINK_CTRL_PCM|\
                 VIA_ACLINK_CTRL_VRA)

#define PCI_VENDOR_ID_VIA        0x1106
#define PCI_DEVICE_ID_VT82C686   0x3058
#define PCI_DEVICE_ID_VT8233     0x3059

#define VIRTUALPAGETABLESIZE   4096
#define PCMBUFFERPAGESIZE      512//4096 //page size determines the interrupt interval

#define VIA_INT_INTERVAL 1

struct via82xx_card
{
 unsigned long   iobase;
 unsigned short     model;
 unsigned int    irq;
 unsigned char   chiprev;
 struct pci_config_s  *pci_dev;

 cardmem_t *dm;
 unsigned long *virtualpagetable;
 char *pcmout_buffer;
 long pcmout_bufsize;
 int pcmout_pages;
};

static void via82xx_AC97Codec_ready(unsigned int baseport);
static void via82xx_ac97_write(unsigned int baseport,unsigned int reg, unsigned int value);
static unsigned int via82xx_ac97_read(unsigned int baseport, unsigned int reg);
static void via82xx_dxs_write(unsigned int baseport,unsigned int reg, unsigned int val);

extern unsigned int intsoundconfig,intsoundcontrol;

static unsigned int via8233_dxs_volume=0x02;

static void via82xx_channel_reset(struct via82xx_card *card)
{
 unsigned int baseport = card->iobase;

 outb(baseport+VIA_REG_OFFSET_CONTROL, VIA_REG_CTRL_PAUSE|VIA_REG_CTRL_TERMINATE|VIA_REG_CTRL_RESET);
 pds_delay_10us(5);
 outb(baseport + VIA_REG_OFFSET_CONTROL, 0x00);
 outb(baseport + VIA_REG_OFFSET_STATUS, 0xFF);
 if(card->pci_dev->device_id==PCI_DEVICE_ID_VT82C686)
  outb(baseport + VIA_REG_OFFSET_TYPE, 0x00);
 outl(baseport + VIA_REG_OFFSET_CURR_PTR, 0);
}

static void via82xx_chip_init(struct via82xx_card *card)
{
 unsigned int data,retry;

 /* deassert ACLink reset, force SYNC */
 pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, VIA_ACLINK_CTRL_ENABLE|VIA_ACLINK_CTRL_RESET|VIA_ACLINK_CTRL_SYNC); // 0xe0
 pds_delay_10us(10);
 if(card->pci_dev->device_id==PCI_DEVICE_ID_VT82C686){
  // full reset
  pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, 0x00);
  pds_delay_10us(10);
  /* ACLink on, deassert ACLink reset, VSR, SGD data out */
  /* note - FM data out has trouble with non VRA codecs !! */
  pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, VIA_ACLINK_CTRL_INIT);// ??? SB   (0xCD)
  pds_delay_10us(10);
 }else{
  /* deassert ACLink reset, force SYNC (warm AC'97 reset) */
  pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, VIA_ACLINK_CTRL_RESET|VIA_ACLINK_CTRL_SYNC); // 0x60
  pds_delay_10us(1);
  /* ACLink on, deassert ACLink reset, VSR, SGD data out */
  pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, VIA_ACLINK_CTRL_INIT); // 0xcc
  pds_delay_10us(10);
 }

 // Make sure VRA is enabled, in case we didn't do a complete codec reset, above
 data=pcibios_ReadConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL);
 if((data&VIA_ACLINK_CTRL_INIT)!=VIA_ACLINK_CTRL_INIT){
  pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, VIA_ACLINK_CTRL_INIT);
  pds_delay_10us(10);
 }

 // wait until codec ready
 retry = 65536;
 do{
  data=pcibios_ReadConfig_Byte(card->pci_dev, VIA_ACLINK_STAT);
  if(data & VIA_ACLINK_C00_READY) /* primary codec ready */
   break;
  pds_delay_10us(1);
 }while(--retry);

 //reset ac97
 via82xx_AC97Codec_ready(card->iobase);
 via82xx_ac97_write(card->iobase,AC97_RESET,0);
 via82xx_ac97_read(card->iobase,AC97_RESET);

 via82xx_channel_reset(card);

 if(card->pci_dev->device_id!=PCI_DEVICE_ID_VT82C686){
  // Workaround for Award BIOS bug:
  // DXS channels don't work properly with VRA if MC97 is disabled.
  struct pci_config_s pci;
  if(pcibios_FindDevice(0x1106, 0x3068, &pci)==PCI_SUCCESSFUL){ /* MC97 */
   data=pcibios_ReadConfig_Byte(&pci, 0x44);
   pcibios_WriteConfig_Byte(&pci, 0x44, data | 0x40);
  }
 }

 // initial ac97 volumes (and clear mute flag)
 via82xx_ac97_write(card->iobase, AC97_MASTER_VOL_STEREO, 0x0202);
 via82xx_ac97_write(card->iobase, AC97_PCMOUT_VOL,        0x0202);
 via82xx_ac97_write(card->iobase, AC97_HEADPHONE_VOL,     0x0202);
 via82xx_ac97_write(card->iobase, AC97_EXTENDED_STATUS,AC97_EA_SPDIF);
}

static void via82xx_chip_close(struct via82xx_card *card)
{
 via82xx_channel_reset(card);
}

static void via82xx_set_table_ptr(struct via82xx_card *card)
{
 via82xx_AC97Codec_ready(card->iobase);
 outl(card->iobase + VIA_REG_OFFSET_TABLE_PTR,(unsigned long)pds_cardmem_physicalptr(card->dm,card->virtualpagetable));
 pds_delay_10us(2);
 via82xx_AC97Codec_ready(card->iobase);
}

//-------------------------------------------------------------------------
static pci_device_s via_devices[]={
 {"VT82C686",PCI_VENDOR_ID_VIA,PCI_DEVICE_ID_VT82C686},
 {"VT8233"  ,PCI_VENDOR_ID_VIA,PCI_DEVICE_ID_VT8233},
 {NULL,0,0}
};

static void VIA82XX_close(struct mpxplay_audioout_info_s *aui);

static void VIA82XX_card_info(struct mpxplay_audioout_info_s *aui)
{
 struct via82xx_card *card=aui->card_private_data;
 char sout[100];
 sprintf(sout,"VIA : %s soundcard found on port:%4.4X irq:%d chiprev:%2.2X model:%4.4X",
     card->pci_dev->device_name,card->iobase,card->irq,card->chiprev,card->model);
 pds_textdisplay_printf(sout);
}

static int VIA82XX_adetect(struct mpxplay_audioout_info_s *aui)
{
 struct via82xx_card *card;

 card=(struct via82xx_card *)pds_calloc(1,sizeof(struct via82xx_card));
 if(!card)
  return 0;
 aui->card_private_data=card;

 card->pci_dev=(struct pci_config_s *)pds_calloc(1,sizeof(struct pci_config_s));
 if(!card->pci_dev)
  goto err_adetect;

 if(pcibios_search_devices(via_devices,card->pci_dev)!=PCI_SUCCESSFUL)
  goto err_adetect;
 pcibios_set_master(card->pci_dev);

 card->iobase = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR)&0xfff0;
 if(!card->iobase)
  goto err_adetect;
 card->irq    = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
 card->chiprev= pcibios_ReadConfig_Byte(card->pci_dev, PCIR_RID);
 card->model  = pcibios_ReadConfig_Word(card->pci_dev, PCIR_SSID);
 #ifdef SBEMU
 aui->card_irq = card->irq;
 printf("VT82 irq: %d\n",aui->card_irq);
 if(aui->card_irq == 0 || aui->card_irq == 0xFF)
 {
     aui->card_irq = card->irq = 10;
     pcibios_WriteConfig_Byte(card->pci_dev, PCIR_INTR_LN, aui->card_irq); //RW
 }
 #endif

 // alloc buffers
 card->pcmout_bufsize=MDma_get_max_pcmoutbufsize(aui,0,PCMBUFFERPAGESIZE,2,0);

 card->dm=MDma_alloc_cardmem( VIRTUALPAGETABLESIZE   // virtualpagetable
                +card->pcmout_bufsize   // pcm output
                +4096 );                // to round

 card->virtualpagetable=(void *)(((uint32_t)card->dm->linearptr+4095)&(~4095));
 card->pcmout_buffer=(char *)card->virtualpagetable+VIRTUALPAGETABLESIZE;

 #ifdef SBEMU
 memset(card->virtualpagetable, 0, VIRTUALPAGETABLESIZE);
 memset(card->pcmout_buffer, 0, card->pcmout_bufsize);
 #endif

 aui->card_DMABUFF=card->pcmout_buffer;

 // init chip
 via82xx_chip_init(card);

 return 1;

err_adetect:
 VIA82XX_close(aui);
 return 0;
}

static void VIA82XX_close(struct mpxplay_audioout_info_s *aui)
{
 struct via82xx_card *card=aui->card_private_data;
 if(card){
  if(card->iobase)
   via82xx_chip_close(card);
  MDma_free_cardmem(card->dm);
  if(card->pci_dev)
   pds_free(card->pci_dev);
  pds_free(card);
  aui->card_private_data=NULL;
 }
}

static void VIA82XX_setrate(struct mpxplay_audioout_info_s *aui)
{
 struct via82xx_card *card=aui->card_private_data;
 unsigned int dmabufsize,pagecount,spdif_rate;
 unsigned long pcmbufp;

 if(aui->freq_card<4000)
  aui->freq_card=4000;
 else{
  if(aui->freq_card>48000)
   aui->freq_card=48000;
 }

 aui->chan_card=2;
 aui->bits_card=16;
 aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;

 dmabufsize=MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,PCMBUFFERPAGESIZE,0);

 // page tables
 card->pcmout_pages=dmabufsize/PCMBUFFERPAGESIZE;
 pcmbufp=(unsigned long)card->pcmout_buffer;
 mpxplay_debugf(VIA_DEBUG_OUTPUT,"VIA PCM pages: %d", card->pcmout_pages);
 
 for(pagecount=0;pagecount<card->pcmout_pages;pagecount++){
  card->virtualpagetable[pagecount*2]=(unsigned long)pds_cardmem_physicalptr(card->dm,pcmbufp);
  if(pagecount<(card->pcmout_pages-1))
   #ifdef SBEMU
   card->virtualpagetable[pagecount*2+1]=((pagecount%VIA_INT_INTERVAL)==VIA_INT_INTERVAL-1) ? VIA_TBL_BIT_FLAG|PCMBUFFERPAGESIZE : PCMBUFFERPAGESIZE;
   #else
   card->virtualpagetable[pagecount*2+1]=PCMBUFFERPAGESIZE; // 0x00001000; // period continues to the next
   #endif
  else
   card->virtualpagetable[pagecount*2+1]=VIA_TBL_BIT_EOL|PCMBUFFERPAGESIZE; // 0x80001000; // buffer boundary
  pcmbufp+=PCMBUFFERPAGESIZE;
 }

 // ac97 config
 via82xx_ac97_write(card->iobase,AC97_EXTENDED_STATUS,AC97_EA_VRA); //this is a bug so SBEMU macro not added
 via82xx_ac97_write(card->iobase,AC97_PCM_FRONT_DAC_RATE, aui->freq_card);

 switch(aui->freq_card){
  case 32000:spdif_rate=AC97_SC_SPSR_32K;break;
  case 44100:spdif_rate=AC97_SC_SPSR_44K;break;
  default:spdif_rate=AC97_SC_SPSR_48K;break;
 }
 via82xx_ac97_write(card->iobase,AC97_SPDIF_CONTROL,spdif_rate); // ???
 pds_delay_10us(10);

 // via hw config
 via82xx_channel_reset(card);
 via82xx_set_table_ptr(card);

 if(card->pci_dev->device_id==PCI_DEVICE_ID_VT82C686){
  outb(card->iobase+VIA_REG_OFFSET_TYPE,
  #ifndef SBEMU
    VIA_REG_TYPE_AUTOSTART |
    VIA_REG_TYPE_16BIT | VIA_REG_TYPE_STEREO ); // old: 0xB0
  #else
    VIA_REG_TYPE_AUTOSTART |
    VIA_REG_TYPE_16BIT | VIA_REG_TYPE_STEREO | VIA_REG_TYPE_INT_LSAMPLE | VIA_REG_TYPE_INT_EOL | VIA_REG_TYPE_INT_FLAG);
  #endif
    //VIA_REG_TYPE_INT_LSAMPLE |                     // ?????
    //VIA_REG_TYPE_INT_EOL | VIA_REG_TYPE_INT_FLAG); // ?????
    // new: 0xB7
 }else{ // VT8233
  unsigned int rbits;
  // init dxs volume (??? here?)
  via82xx_dxs_write(card->iobase,VIA_REG_OFS_PLAYBACK_VOLUME_L, via8233_dxs_volume);
  via82xx_dxs_write(card->iobase,VIA_REG_OFS_PLAYBACK_VOLUME_R, via8233_dxs_volume);
  // freq
  if(aui->freq_card==48000)
   rbits = 0xfffff;
  else
   rbits = (0x100000 / 48000) * aui->freq_card + ((0x100000 % 48000) * aui->freq_card) / 48000;
  outl(card->iobase+VIA_REG_OFFSET_STOP_IDX, VIA8233_REG_TYPE_16BIT | VIA8233_REG_TYPE_STEREO | rbits | 0xFF000000);
 }
 pds_delay_10us(2);
 via82xx_AC97Codec_ready(card->iobase);
}

static void VIA82XX_start(struct mpxplay_audioout_info_s *aui)
{
 struct via82xx_card *card=aui->card_private_data;
 if(card->pci_dev->device_id==PCI_DEVICE_ID_VT82C686)
  outb(card->iobase + VIA_REG_OFFSET_CONTROL, VIA_REG_CTRL_START);
 else
  outb(card->iobase + VIA_REG_OFFSET_CONTROL, VIA_REG_CTRL_START | VIA_REG_CTRL_AUTOSTART);
}

static void VIA82XX_stop(struct mpxplay_audioout_info_s *aui)
{
 struct via82xx_card *card=aui->card_private_data;
 outb(card->iobase + VIA_REG_OFFSET_CONTROL, VIA_REG_CTRL_PAUSE);
}

static long VIA82XX_getbufpos(struct mpxplay_audioout_info_s *aui)
{
 struct via82xx_card *card=aui->card_private_data;
 unsigned int baseport=card->iobase;
 unsigned long idx,count,bufpos;

 if(card->pci_dev->device_id==PCI_DEVICE_ID_VT82C686){
  count = inl(baseport + VIA_REG_PLAYBACK_CURR_COUNT);
  idx   = inl(baseport + VIA_REG_OFFSET_CURR_PTR);
  if(idx<=(unsigned long)card->virtualpagetable)
   idx=0;
  else{
   idx = idx - (unsigned long)card->virtualpagetable;
   idx = idx >> 3; // 2 * 4 bytes
   idx = idx - 1;
   idx = idx % card->pcmout_pages;
  }
 }else{ // VT8233/8235
  count = inl(baseport + VIA_REG_PLAYBACK_CURR_COUNT);
  idx   = count >> 24;
 }
 count &= 0xffffff;

 if(count && (count<=PCMBUFFERPAGESIZE)){

  bufpos = (idx * PCMBUFFERPAGESIZE) + PCMBUFFERPAGESIZE - count;

  if(bufpos<aui->card_dmasize)
   aui->card_dma_lastgoodpos=bufpos;
 }

 return aui->card_dma_lastgoodpos;
}

static void VIA82XX_clearbuf(struct mpxplay_audioout_info_s *aui)
{
 MDma_clearbuf(aui);
}

//--------------------------------------------------------------------------
//mixer

static unsigned long via82xx_ReadAC97Codec_sub(unsigned int baseport)
{
 unsigned long d0;
 int retry = 2048;

 do{
  d0 = inl(baseport + VIA_REG_AC97_CTRL);
  if( (d0 & VIA_REG_AC97_PRIMARY_VALID) != 0 )
   break;
  pds_delay_10us(1);
 }while(--retry);

 d0 = inl(baseport + VIA_REG_AC97_CTRL);
 return d0;
}

static void via82xx_AC97Codec_ready(unsigned int baseport)
{
 unsigned long d0;
 int retry = 2048;

 do{
  d0 = inl(baseport + VIA_REG_AC97_CTRL);
  if( (d0 & VIA_REG_AC97_BUSY) == 0 )
   break;
  pds_delay_10us(1);
 }while(--retry);
}

static void via82xx_WriteAC97Codec_sub(unsigned int baseport,unsigned long value)
{
 via82xx_AC97Codec_ready(baseport);
 outl(baseport + VIA_REG_AC97_CTRL, value);
 via82xx_AC97Codec_ready(baseport);
}

static void via82xx_ac97_write(unsigned int baseport,unsigned int reg, unsigned int value)
{
 unsigned long d0;

 reg   &= VIA_REG_AC97_CMD_MASK;
 value &= VIA_REG_AC97_DATA_MASK;
 d0 = VIA_REG_AC97_CODEC_ID_PRIMARY | VIA_REG_AC97_WRITE | (reg << VIA_REG_AC97_CMD_SHIFT) | value;
 via82xx_WriteAC97Codec_sub(baseport,d0);
 via82xx_WriteAC97Codec_sub(baseport,d0);
}

static unsigned int via82xx_ac97_read(unsigned int baseport, unsigned int reg)
{
 long d0;

 reg &= VIA_REG_AC97_CMD_MASK;
 d0 = VIA_REG_AC97_CODEC_ID_PRIMARY | VIA_REG_AC97_READ | (reg << VIA_REG_AC97_CMD_SHIFT);
 via82xx_WriteAC97Codec_sub(baseport,d0);
 via82xx_WriteAC97Codec_sub(baseport,d0);

 return via82xx_ReadAC97Codec_sub(baseport);
}

static void via82xx_dxs_write(unsigned int baseport,unsigned int reg, unsigned int val)
{
 outb(baseport+reg+0x00,val);
 //outb(baseport+reg+0x10,val);
 //outb(baseport+reg+0x20,val);
 //outb(baseport+reg+0x30,val);
 via8233_dxs_volume=val;
}

static unsigned int via82xx_dxs_read(unsigned int baseport,unsigned int reg)
{
 return via8233_dxs_volume; // is the dxs write-only?
}

static void VIA82XX_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 struct via82xx_card *card=aui->card_private_data;

 //if((reg==VIA_REG_OFS_PLAYBACK_VOLUME_L) || (reg==VIA_REG_OFS_PLAYBACK_VOLUME_R)){
 if(reg>=256){ // VIA_REG_OFS_PLAYBACK_VOLUME_X
  if(card->pci_dev->device_id!=PCI_DEVICE_ID_VT82C686)
   via82xx_dxs_write(card->iobase,(reg>>8),val);
 }else
  via82xx_ac97_write(card->iobase,reg,val);
}

static unsigned long VIA82XX_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 struct via82xx_card *card=aui->card_private_data;
 unsigned int retval=0;

 //if((reg==VIA_REG_OFS_PLAYBACK_VOLUME_L) || (reg==VIA_REG_OFS_PLAYBACK_VOLUME_R)){
 if(reg>=256){ // VIA_REG_OFS_PLAYBACK_VOLUME_X
  if(card->pci_dev->device_id!=PCI_DEVICE_ID_VT82C686)
   retval=via82xx_dxs_read(card->iobase,(reg>>8));
 }else
  retval=via82xx_ac97_read(card->iobase,reg);

 return retval;
}

#ifdef SBEMU
static int VIA82XX_IRQRoutine(mpxplay_audioout_info_s* aui)
{
  struct via82xx_card *card=aui->card_private_data;

  int status = inb(card->iobase + VIA_REG_OFFSET_STATUS)&(VIA_REG_STATUS_FLAG|VIA_REG_STATUS_EOL);
  if(status)
    outb(card->iobase+VIA_REG_OFFSET_STATUS, status);
  return status != 0;
}
#endif

static aucards_onemixerchan_s via82xx_master_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),2,{
  {AC97_MASTER_VOL_STEREO,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // left
  {AC97_MASTER_VOL_STEREO,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // right
  //{(VIA_REG_OFS_PLAYBACK_VOLUME_L<<8),0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  //{(VIA_REG_OFS_PLAYBACK_VOLUME_R<<8),0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}
 }
};

static aucards_onemixerchan_s via82xx_pcm_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME),4,{
  {AC97_PCMOUT_VOL,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AC97_PCMOUT_VOL,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {(VIA_REG_OFS_PLAYBACK_VOLUME_L<<8),0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // DXS channels
  {(VIA_REG_OFS_PLAYBACK_VOLUME_R<<8),0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}
 }
};

static aucards_onemixerchan_s via82xx_headphone_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_HEADPHONE,AU_MIXCHANFUNC_VOLUME),2,{
  {AC97_HEADPHONE_VOL,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {AC97_HEADPHONE_VOL,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}
 }
};

static aucards_allmixerchan_s via82xx_mixerset[]={
 &via82xx_master_vol,
 &via82xx_pcm_vol,
 &via82xx_headphone_vol,
 NULL
};

one_sndcard_info VIA82XX_sndcard_info={
 "VIA VT82XX AC97",
 SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

 NULL,
 NULL,                  // no init
 &VIA82XX_adetect,      // only autodetect
 &VIA82XX_card_info,
 &VIA82XX_start,
 &VIA82XX_stop,
 &VIA82XX_close,
 &VIA82XX_setrate,

 &MDma_writedata,
 &VIA82XX_getbufpos,
 &VIA82XX_clearbuf,
 &MDma_interrupt_monitor,
 #ifdef SBEMU
 &VIA82XX_IRQRoutine,
 #else
 NULL,
 #endif

 &VIA82XX_writeMIXER,
 &VIA82XX_readMIXER,
 &via82xx_mixerset[0]
};

#endif
