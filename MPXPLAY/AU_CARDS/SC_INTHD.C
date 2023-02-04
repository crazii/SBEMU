//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2014 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: Intel HD audio driver for Mpxplay
//based on ALSA (http://www.alsa-project.org) and WSS libs

//#define MPXPLAY_USE_DEBUGF 1
#define IHD_DEBUG_OUTPUT stdout

#include "mpxplay.h"

#ifdef AU_CARDS_LINK_IHD

#include <string.h>
#include "dmairq.h"
#include "pcibios.h"
#include "sc_inthd.h"

//#define INTHD_CODEC_EXTRA_DELAY_US 100 // 100 us

#define AUCARDSCONFIG_IHD_USE_SPEAKEROUT (1<<0) // use speaker output (instead of hp/line)
#define AUCARDSCONFIG_IHD_USE_FIXED_SDO  (1<<1) // don't read stream offset (for sd_addr) from GCAP (use 0x100)

#define INTHD_MAX_CHANNELS 8
#ifdef SBEMU
#define AZX_PERIOD_SIZE 512
#else
#define AZX_PERIOD_SIZE 4096
#endif

struct intelhd_card_s
{
 unsigned long  iobase;
 struct pci_config_s  *pci_dev;
 unsigned int  board_driver_type;
 long          codec_vendor_id;
 unsigned long codec_mask;
 unsigned int  codec_index;
 hda_nid_t afg_root_nodenum;
 int afg_num_nodes;
 struct hda_gnode *afg_nodes;
 unsigned int def_amp_out_caps;
 unsigned int def_amp_in_caps;
 struct hda_gnode *dac_node[2];            // DAC nodes
 struct hda_gnode *out_pin_node[MAX_PCM_VOLS];    // Output pin (Line-Out) nodes
 unsigned int pcm_num_vols;            // number of PCM volumes
 struct pcm_vol_s pcm_vols[MAX_PCM_VOLS]; // PCM volume nodes

 cardmem_t *dm;
 uint32_t *table_buffer;
 char *pcmout_buffer;
 long pcmout_bufsize;
 unsigned long* corb_buffer;
 unsigned long long* rirb_buffer;
 unsigned long pcmout_dmasize;
 unsigned int  pcmout_num_periods;
 unsigned long pcmout_period_size;
 unsigned long sd_addr;    // stream io address (one playback stream only)
 unsigned int  format_val; // stream type
 unsigned int  dacout_num_bits;
 unsigned long supported_formats;
 unsigned long supported_max_freq;
 unsigned int  supported_max_bits;
 unsigned int  config_select;
};

struct codec_vendor_list_s{
 unsigned short vendor_id;
 char *vendor_name;
};

struct hda_rate_tbl {
 unsigned int hz;
 unsigned int hda_fmt;
};

static struct hda_rate_tbl rate_bits[] = {
 {  8000, 0x0500 }, // 1/6 x 48
 { 11025, 0x4300 }, // 1/4 x 44
 { 16000, 0x0200 }, // 1/3 x 48
 { 22050, 0x4100 }, // 1/2 x 44
 { 32000, 0x0a00 }, // 2/3 x 48
 { 44100, 0x4000 }, // 44
 { 48000, 0x0000 }, // 48
 { 88200, 0x4800 }, // 2 x 44
 { 96000, 0x0800 }, // 2 x 48
 {176400, 0x5800 }, // 4 x 44
 {192000, 0x1800 }, // 4 x 48
 {0xffffffff,0x1800}, // 192000
 {0,0}
};

static aucards_onemixerchan_s ihd_master_vol={
 AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME),MAX_PCM_VOLS,{
  {0,0x00,0,0}, // card->pcm_vols[0]
  {0,0x00,0,0}, // card->pcm_vols[1]
 }
};

//Intel HDA codec has memory mapping only (by specification)

#define azx_writel(chip,reg,value) PDS_PUTB_LE32((char *)((chip)->iobase + ICH6_REG_##reg),value)
#define azx_readl(chip,reg) PDS_GETB_LE32((char *)((chip)->iobase + ICH6_REG_##reg))
#define azx_writew(chip,reg,value) PDS_PUTB_LE16((char *)((chip)->iobase + ICH6_REG_##reg), value)
#define azx_readw(chip,reg) PDS_GETB_LE16((char *)((chip)->iobase + ICH6_REG_##reg))
#define azx_writeb(chip,reg,value) *((unsigned char *)((chip)->iobase + ICH6_REG_##reg))=value
#define azx_readb(chip,reg) PDS_GETB_8U((char *)((chip)->iobase + ICH6_REG_##reg))

#define azx_sd_writel(dev,reg,value) PDS_PUTB_LE32((char *)((dev)->sd_addr + ICH6_REG_##reg),value)
#define azx_sd_readl(dev,reg) PDS_GETB_LE32((char *)((dev)->sd_addr + ICH6_REG_##reg))
#define azx_sd_writew(dev,reg,value) PDS_PUTB_LE16((char*)((dev)->sd_addr + ICH6_REG_##reg),value)
#define azx_sd_readw(dev,reg) PDS_GETB_LE16((char *)((dev)->sd_addr + ICH6_REG_##reg))
#define azx_sd_writeb(dev,reg,value) *((unsigned char *)((dev)->sd_addr + ICH6_REG_##reg))=value
#define azx_sd_readb(dev,reg) PDS_GETB_8U((char *)((dev)->sd_addr + ICH6_REG_##reg))

//-------------------------------------------------------------------------
static void update_pci_byte(pci_config_s *pci, unsigned int reg,
                unsigned char mask, unsigned char val)
{
 unsigned char data;

 data=pcibios_ReadConfig_Byte(pci, reg);
 data &= ~mask;
 data |= (val & mask);
 pcibios_WriteConfig_Byte(pci, reg, data);
}

static void azx_init_pci(struct intelhd_card_s *card)
{
 unsigned int tmp;

 switch(card->board_driver_type) {
  case AZX_DRIVER_ATI:
   update_pci_byte(card->pci_dev,ATI_SB450_HDAUDIO_MISC_CNTR2_ADDR,
                   0x07, ATI_SB450_HDAUDIO_ENABLE_SNOOP); // enable snoop
   break;
  case AZX_DRIVER_ATIHDMI_NS:
   update_pci_byte(card->pci_dev,ATI_SB450_HDAUDIO_MISC_CNTR2_ADDR,
                   ATI_SB450_HDAUDIO_ENABLE_SNOOP, 0); // disable snoop
   break;
  case AZX_DRIVER_NVIDIA:
   update_pci_byte(card->pci_dev,NVIDIA_HDA_TRANSREG_ADDR,
                   0x0f, NVIDIA_HDA_ENABLE_COHBITS);
   update_pci_byte(card->pci_dev,NVIDIA_HDA_ISTRM_COH,
                   0x01, NVIDIA_HDA_ENABLE_COHBIT);
   update_pci_byte(card->pci_dev,NVIDIA_HDA_OSTRM_COH,
                   0x01, NVIDIA_HDA_ENABLE_COHBIT);
   break;
  case AZX_DRIVER_SCH:
  case AZX_DRIVER_PCH:
  case AZX_DRIVER_SKL:
  case AZX_DRIVER_HDMI:
   tmp = pcibios_ReadConfig_Word(card->pci_dev, INTEL_SCH_HDA_DEVC);
   if(tmp&INTEL_SCH_HDA_DEVC_NOSNOOP)
    pcibios_WriteConfig_Word(card->pci_dev, INTEL_SCH_HDA_DEVC, tmp & (~INTEL_SCH_HDA_DEVC_NOSNOOP));
   break;
  case AZX_DRIVER_ULI:
   tmp = pcibios_ReadConfig_Word(card->pci_dev, INTEL_HDA_HDCTL);
   pcibios_WriteConfig_Word(card->pci_dev, INTEL_HDA_HDCTL, tmp | 0x10);
   pcibios_WriteConfig_Dword(card->pci_dev, INTEL_HDA_HDBARU, 0);
   break;
 }

 pcibios_enable_memmap_set_master(card->pci_dev); // Intel HDA chips uses memory mapping only

 if(card->pci_dev->vendor_id != 0x1002) // != ATI
  update_pci_byte(card->pci_dev, ICH6_PCIREG_TCSEL, 0x07, 0);
}

//-------------------------------------------------------------------------
static void azx_single_send_cmd(struct intelhd_card_s *chip,uint32_t val)
{
 int timeout = 2000; // 200 ms

#if !defined(SBEMU) //Immediate Commands are optional, some devices don't have it, use CORB
 while((azx_readw(chip, IRS) & ICH6_IRS_BUSY) && (--timeout))
  pds_delay_10us(10);

#ifdef MPXPLAY_USE_DEBUGF
 if(!timeout)
  mpxplay_debugf(IHD_DEBUG_OUTPUT,"send cmd timeout %d", timeout);
#endif

#ifdef INTHD_CODEC_EXTRA_DELAY_US
 pds_delay_10us(INTHD_CODEC_EXTRA_DELAY_US/10); // 0.1 ms
#endif

 azx_writel(chip, IC, val);
 azx_writew(chip, IRS, azx_readw(chip, IRS) | (ICH6_IRS_VALID|ICH6_IRS_BUSY));
#else
 static int corbsizes[4] = {2, 16, 256, 0};
 int corbsize = corbsizes[(azx_readw(chip, CORBSIZE)&0x3)];
 int corbindex = azx_readw(chip, CORBWP)&0xFF;
 do
 {
   int corbread = azx_readw(chip, CORBRP);
   if(((corbindex+1)%corbsize) != corbread)
    break;
  pds_delay_10us(10);
 }while(--timeout);

#ifdef MPXPLAY_USE_DEBUGF
 if(!timeout)
  mpxplay_debugf(IHD_DEBUG_OUTPUT,"send cmd timeout %d", timeout);
#endif

 corbindex = (corbindex+1) % corbsize;
 chip->corb_buffer[corbindex] = val;
 azx_writew(chip, CORBWP, corbindex);
 azx_writeb(chip, CORBCTL, 0x2); //start
#endif
}

static void snd_hda_codec_write(struct intelhd_card_s *chip, hda_nid_t nid,
                         uint32_t direct,
             unsigned int verb, unsigned int parm)
{
 uint32_t val;

 val = (uint32_t)(chip->codec_index & 0x0f) << 28;
 val|= (uint32_t)direct << 27;
 val|= (uint32_t)nid << 20;
 val|= verb << 8;
 val|= parm;

 azx_single_send_cmd(chip, val);
}

static unsigned int azx_get_response(struct intelhd_card_s *chip)
{
 int timeout = 2000; // 200 ms
#if !defined(SBEMU) //Immediate Commands are optional, some devices don't have it, use CORB
 do{
  uint16_t irs = azx_readw(chip, IRS);
  if((irs&ICH6_IRS_VALID) && !(irs&ICH6_IRS_BUSY))
   break;
  pds_delay_10us(10);
 }while(--timeout);

#ifdef MPXPLAY_USE_DEBUGF
 if(!timeout)
  mpxplay_debugf(IHD_DEBUG_OUTPUT,"read response timeout %d", timeout);
#endif

#ifdef INTHD_CODEC_EXTRA_DELAY_US
 pds_delay_10us(INTHD_CODEC_EXTRA_DELAY_US/10); // 0.1 ms
#endif
 return azx_readl(chip, IR);
#else
 do{
  azx_writeb(chip, RIRBCTL, 0x3);
  if(azx_readb(chip, RIRBSTS)&1)
    break;
  pds_delay_10us(10);
  }while(--timeout);
 if(!timeout)
  mpxplay_debugf(IHD_DEBUG_OUTPUT,"read response timeout %d", timeout);
  int rirbindex = azx_readw(chip, RIRBWP);
  long long data = chip->rirb_buffer[rirbindex];
  azx_writeb(chip, RIRBSTS, 1);
  return (unsigned int)(data);
#endif
}

static unsigned int snd_hda_codec_read(struct intelhd_card_s *chip, hda_nid_t nid,
                         uint32_t direct,
             unsigned int verb, unsigned int parm)
{
 snd_hda_codec_write(chip,nid,direct,verb,parm);
 return azx_get_response(chip);
}

#define snd_hda_param_read(codec,nid,param) snd_hda_codec_read(codec,nid,0,AC_VERB_PARAMETERS,param)

static void snd_hda_codec_setup_stream(struct intelhd_card_s *chip, hda_nid_t nid,
                uint32_t stream_tag,
                int channel_id, int format)
{
 snd_hda_codec_write(chip, nid, 0, AC_VERB_SET_CHANNEL_STREAMID,
                    (stream_tag << 4) | channel_id);
 pds_delay_10us(100);
 snd_hda_codec_write(chip, nid, 0, AC_VERB_SET_STREAM_FORMAT, format);
 pds_delay_10us(100);
}

//------------------------------------------------------------------------
static unsigned int snd_hda_get_sub_nodes(struct intelhd_card_s *card, hda_nid_t nid,
              hda_nid_t *start_id)
{
 int parm;

 parm = snd_hda_param_read(card, nid, AC_PAR_NODE_COUNT);
 if(parm<0)
  return 0;
 *start_id = (parm >> 16) & 0xff;
 return (parm & 0xff);
}

static void snd_hda_search_audio_node(struct intelhd_card_s *card)
{
 int i, total_nodes;
 hda_nid_t nid;

 total_nodes = snd_hda_get_sub_nodes(card, AC_NODE_ROOT, &nid);
 for(i=0;i<total_nodes;i++,nid++){
  if((snd_hda_param_read(card,nid,AC_PAR_FUNCTION_TYPE)&0xff)==AC_GRP_AUDIO_FUNCTION){
   card->afg_root_nodenum=nid;
   break;
  }
 }

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"total_nodes:%d afg_nodenum:%d",total_nodes,(int)card->afg_root_nodenum);
}

static int snd_hda_get_connections(struct intelhd_card_s *card, hda_nid_t nid,
                hda_nid_t *conn_list, int max_conns)
{
 unsigned int parm;
 int i, conn_len, conns;
 unsigned int shift, num_elems, mask;
 hda_nid_t prev_nid;

 parm = snd_hda_param_read(card, nid, AC_PAR_CONNLIST_LEN);
 if (parm & AC_CLIST_LONG) {
  shift = 16;
  num_elems = 2;
 } else {
  shift = 8;
  num_elems = 4;
 }

 conn_len = parm & AC_CLIST_LENGTH;
 if(!conn_len)
  return 0;

 mask = (1 << (shift-1)) - 1;

 if(conn_len == 1){
  parm = snd_hda_codec_read(card, nid, 0, AC_VERB_GET_CONNECT_LIST, 0);
  conn_list[0] = parm & mask;
  return 1;
 }

 conns = 0;
 prev_nid = 0;
 for (i = 0; i < conn_len; i++) {
  int range_val;
  hda_nid_t val, n;

  if (i % num_elems == 0)
   parm = snd_hda_codec_read(card, nid, 0,AC_VERB_GET_CONNECT_LIST, i);

  range_val = !!(parm & (1 << (shift-1)));
  val = parm & mask;
  parm >>= shift;
  if(range_val) {
   if(!prev_nid || prev_nid >= val)
    continue;
   for(n = prev_nid + 1; n <= val; n++) {
    if(conns >= max_conns)
     return -1;
    conn_list[conns++] = n;
   }
  }else{
   if (conns >= max_conns)
    return -1;
   conn_list[conns++] = val;
  }
  prev_nid = val;
 }
 return conns;
}

static int snd_hda_add_new_node(struct intelhd_card_s *card, struct hda_gnode *node, hda_nid_t nid)
{
 int nconns = 0;

 node->nid = nid;
 node->wid_caps = snd_hda_param_read(card, nid, AC_PAR_AUDIO_WIDGET_CAP);
 node->type = (node->wid_caps & AC_WCAP_TYPE) >> AC_WCAP_TYPE_SHIFT;

 if(node->wid_caps&AC_WCAP_CONN_LIST)
  nconns = snd_hda_get_connections(card, nid,&node->conn_list[0],HDA_MAX_CONNECTIONS);

 if(nconns>=0){
  node->nconns = nconns;

  if(node->type == AC_WID_PIN){
   node->pin_caps = snd_hda_param_read(card, node->nid, AC_PAR_PIN_CAP);
   node->pin_ctl = snd_hda_codec_read(card, node->nid, 0, AC_VERB_GET_PIN_WIDGET_CONTROL, 0);
   node->def_cfg = snd_hda_codec_read(card, node->nid, 0, AC_VERB_GET_CONFIG_DEFAULT, 0);
  }

  if(node->wid_caps&AC_WCAP_OUT_AMP){
   if(node->wid_caps&AC_WCAP_AMP_OVRD)
    node->amp_out_caps = snd_hda_param_read(card, node->nid, AC_PAR_AMP_OUT_CAP);
   if(!node->amp_out_caps)
    node->amp_out_caps = card->def_amp_out_caps;
  }

  if(node->wid_caps&AC_WCAP_IN_AMP){
   if(node->wid_caps&AC_WCAP_AMP_OVRD)
    node->amp_in_caps = snd_hda_param_read(card, node->nid, AC_PAR_AMP_IN_CAP);
   if(!node->amp_in_caps)
    node->amp_in_caps = card->def_amp_in_caps;
  }

  if(node->wid_caps&AC_WCAP_FORMAT_OVRD)
   node->supported_formats=snd_hda_param_read(card, node->nid, AC_PAR_PCM);
 }

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"n:%2d c:%2d w:%8.8X t:%2d p:%2.2X %8.8X d:%8.8X i:%8.8X o:%8.8X s:%8.8X",
   (int)nid,nconns,node->wid_caps,(int)node->type,(int)node->pin_ctl,node->pin_caps,node->def_cfg,
   node->amp_in_caps,node->amp_out_caps,node->supported_formats);

 /*mpxplay_debugf(IHD_DEBUG_OUTPUT,"node:%2d cons:%2d wc:%8.8X t:%2d aoc:%8.8X ot:%2d sf:%8.8X st:%2d of:%2d",
   (int)nid,nconns,node->wid_caps,
   node->type,node->amp_out_caps,(node->wid_caps&AC_WCAP_OUT_AMP),
   node->supported_formats,((node->amp_out_caps>>8)&0x7f),
   (node->amp_out_caps&AC_AMPCAP_OFFSET));*/

 return nconns;
}

static struct hda_gnode *hda_get_node(struct intelhd_card_s *card, hda_nid_t nid)
{
 struct hda_gnode *node=card->afg_nodes;
 unsigned int i;

 for(i=0;i<card->afg_num_nodes;i++,node++)
  if(node->nid==nid)
   return node;

 return NULL;
}

static void snd_hda_put_vol_mute(struct intelhd_card_s *card,hda_nid_t nid,
                         int ch, int direction, int index,int val)
{
 uint32_t parm;

 parm  = (ch)? AC_AMP_SET_RIGHT : AC_AMP_SET_LEFT;
 parm |= (direction == HDA_OUTPUT) ? AC_AMP_SET_OUTPUT : AC_AMP_SET_INPUT;
 parm |= index << AC_AMP_SET_INDEX_SHIFT;
 parm |= val;
 snd_hda_codec_write(card, nid, 0, AC_VERB_SET_AMP_GAIN_MUTE, parm);
}

static unsigned int snd_hda_get_vol_mute(struct intelhd_card_s *card,hda_nid_t nid,
                 int ch, int direction, int index)
{
 uint32_t val, parm;

 parm  = (ch)? AC_AMP_GET_RIGHT:AC_AMP_GET_LEFT;
 parm |= (direction==HDA_OUTPUT)? AC_AMP_GET_OUTPUT : AC_AMP_GET_INPUT;
 parm |= index;
 val = snd_hda_codec_read(card, nid, 0,AC_VERB_GET_AMP_GAIN_MUTE, parm);
 return (val&0xff);
}

static int snd_hda_codec_amp_update(struct intelhd_card_s *card, hda_nid_t nid, int ch,
                 int direction, int idx, int mask, int val)
{
 val &= mask;
 val |= snd_hda_get_vol_mute(card, nid, ch, direction, idx) & ~mask;
 snd_hda_put_vol_mute(card, nid, ch, direction, idx, val);
 return 1;
}

static int snd_hda_codec_amp_stereo(struct intelhd_card_s *card, hda_nid_t nid,
                 int direction, int idx, int mask, int val)
{
 int ch, ret = 0;
 for (ch = 0; ch < 2; ch++)
  ret |= snd_hda_codec_amp_update(card, nid, ch, direction,idx, mask, val);
 return ret;
}

static void snd_hda_unmute_output(struct intelhd_card_s *card, struct hda_gnode *node)
{
 unsigned int val = (node->amp_out_caps & AC_AMPCAP_NUM_STEPS) >> AC_AMPCAP_NUM_STEPS_SHIFT;
 snd_hda_codec_amp_stereo(card, node->nid, HDA_OUTPUT, 0, 0xff, val);
}

static void snd_hda_unmute_input(struct intelhd_card_s *card, struct hda_gnode *node, unsigned int index)
{
 unsigned int val = (node->amp_in_caps & AC_AMPCAP_NUM_STEPS) >> AC_AMPCAP_NUM_STEPS_SHIFT;
 snd_hda_codec_amp_stereo(card, node->nid, HDA_INPUT, index, 0xff, val);
}

static void select_input_connection(struct intelhd_card_s *card, struct hda_gnode *node,
                   unsigned int index)
{
 snd_hda_codec_write(card, node->nid, 0,AC_VERB_SET_CONNECT_SEL, index);
}

static void clear_check_flags(struct intelhd_card_s *card)
{
 struct hda_gnode *node=card->afg_nodes;
 unsigned int i;

 for(i=0;i<card->afg_num_nodes;i++,node++)
  node->checked=0;
}

static int parse_output_path(struct intelhd_card_s *card,struct hda_gnode *node, int dac_idx)
{
 int i, err;
 struct hda_gnode *child;

 if(node->checked)
  return 0;

 node->checked = 1;
 if(node->type == AC_WID_AUD_OUT) {
  if(node->wid_caps & AC_WCAP_DIGITAL)
   return 0;
  if(card->dac_node[dac_idx])
   return (node==card->dac_node[dac_idx]);

  card->dac_node[dac_idx] = node;
  if((node->wid_caps&AC_WCAP_OUT_AMP) && (card->pcm_num_vols<MAX_PCM_VOLS)){
   card->pcm_vols[card->pcm_num_vols].node = node;
   card->pcm_vols[card->pcm_num_vols].index = 0;
   card->pcm_num_vols++;
  }
  return 1;
 }

 for(i=0; i<node->nconns; i++){
  child = hda_get_node(card, node->conn_list[i]);
  if(!child)
   continue;
  err = parse_output_path(card, child, dac_idx);
  if(err<0)
   return err;
  else if(err>0){
   if(node->nconns>1)
    select_input_connection(card, node, i);
   snd_hda_unmute_input(card, node, i);
   snd_hda_unmute_output(card, node);
   if(card->dac_node[dac_idx] &&
      (card->pcm_num_vols<MAX_PCM_VOLS) &&
      !(card->dac_node[dac_idx]->wid_caps&AC_WCAP_OUT_AMP))
   {
    if((node->wid_caps & AC_WCAP_IN_AMP) || (node->wid_caps & AC_WCAP_OUT_AMP)){
     int n = card->pcm_num_vols;
     card->pcm_vols[n].node = node;
     card->pcm_vols[n].index = i;
     card->pcm_num_vols++;
    }
   }
   return 1;
  }
 }
 return 0;
}

static struct hda_gnode *parse_output_jack(struct intelhd_card_s *card,int jack_type)
{
 struct hda_gnode *node=card->afg_nodes;
 int err,i;

 for(i=0;i<card->afg_num_nodes;i++,node++){
  if(node->type!=AC_WID_PIN)
   continue;
  if(!(node->pin_caps&AC_PINCAP_OUT))
   continue;
  if(defcfg_port_conn(node)==AC_JACK_PORT_NONE)
   continue;
  if(jack_type>=0){
   if(jack_type!=defcfg_type(node))
    continue;
   if(node->wid_caps&AC_WCAP_DIGITAL)
    continue;
  }else{
   if(!(node->pin_ctl&AC_PINCTL_OUT_EN))
    continue;
  }
  clear_check_flags(card);
  err = parse_output_path(card, node, 0);
  if(err<0)
   return NULL;
  if(!err && card->out_pin_node[0]){
   err = parse_output_path(card, node, 1);
   if(err<0)
    return NULL;
  }
  if(err>0){
   snd_hda_unmute_output(card, node);
   snd_hda_codec_write(card, node->nid, 0,
                       AC_VERB_SET_PIN_WIDGET_CONTROL,
               AC_PINCTL_OUT_EN |
               ((node->pin_caps & AC_PINCAP_HP_DRV)? AC_PINCTL_HP_EN : 0));
   return node;
  }
 }
 return NULL;
}

static void snd_hda_enable_eapd(struct intelhd_card_s *card, struct hda_gnode *node)
{
 if(node->pin_caps&AC_PINCAP_EAPD){
  unsigned int eapd_set = snd_hda_codec_read(card, node->nid, 0, AC_VERB_GET_EAPD_BTLENABLE, 0);
  funcbit_enable(eapd_set, AC_PINCTL_EAPD_EN);
  snd_hda_codec_write(card, node->nid, 0, AC_VERB_SET_EAPD_BTLENABLE, eapd_set);
 }
}

static int snd_hda_parse_output(struct intelhd_card_s *card)
{
 struct hda_gnode *node;
 int i = 0;
 int8_t *po,parseorder_line[] = {AC_JACK_LINE_OUT, AC_JACK_HP_OUT, -1};
 int8_t parseorder_speaker[] = {AC_JACK_SPEAKER, AC_JACK_HP_OUT, AC_JACK_LINE_OUT, -1};

 po=(card->config_select&AUCARDSCONFIG_IHD_USE_SPEAKEROUT)? &parseorder_speaker[0]:&parseorder_line[0];

 do{
  node = parse_output_jack(card, *po);
  if(node){
   card->out_pin_node[i++] = node;
   if((*po == AC_JACK_SPEAKER) || (*po == AC_JACK_HP_OUT))
    snd_hda_enable_eapd(card, node);
  }
  po++;
 }while((i<MAX_PCM_VOLS) && (*po>=0));

 if(!card->out_pin_node[0]){ // should not happen
  node = parse_output_jack(card, -1); // parse 1st output
  if(!node)
   return 0;
  card->out_pin_node[0] = node;
 }

 return 1;
}

//------------------------------------------------------------------------

static unsigned int azx_reset(struct intelhd_card_s *chip)
{
 int timeout;

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"azx reset_start: gctl1:%8.8X",azx_readl(chip, GCTL));

 azx_writeb(chip, STATESTS, STATESTS_INT_MASK);
 azx_writel(chip, GCTL, azx_readl(chip, GCTL) & ~ICH6_GCTL_RESET);

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"gctl2b:%8.8X gctl2d:%8.8X",(unsigned long)azx_readb(chip, GCTL),azx_readl(chip, GCTL));

 timeout = 500;
 while(((azx_readb(chip, GCTL)&ICH6_GCTL_RESET)!=0) && (--timeout))
  pds_delay_10us(100);

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"gctl3:%8.8X timeout:%d",azx_readl(chip, GCTL),timeout);

 pds_delay_10us(100);

 azx_writeb(chip, GCTL, azx_readb(chip, GCTL) | ICH6_GCTL_RESET);

 timeout = 500;
 while(((azx_readb(chip, GCTL)&ICH6_GCTL_RESET)==0) && (--timeout))
  pds_delay_10us(100);

 pds_delay_10us(100);

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"gctl4:%8.8X timeout:%d ",(unsigned long)azx_readb(chip, GCTL),timeout);

 if(!azx_readb(chip, GCTL)){
  mpxplay_debugf(IHD_DEBUG_OUTPUT,"HDA controller not ready!");
  return 0;
 }

 // disable unsolicited responses (single cmd mode)
 azx_writel(chip, GCTL, (azx_readl(chip, GCTL) & (~ICH6_GCTL_UREN)));

 // set CORB command DMA buffer
 azx_writel(chip, CORBLBASE, (unsigned long)pds_cardmem_physicalptr(chip->dm, chip->corb_buffer));
 //azx_writel(chip, CORBSIZE, 0);
 azx_writel(chip, RIRBLBASE, (unsigned long)pds_cardmem_physicalptr(chip->dm, chip->rirb_buffer));
 //azx_writel(chip, RIRBSIZE, 0); maybe only 1 supported
 azx_writew(chip, RINTCNT, 1); //1 response for one interrupt each time

 pds_delay_10us(100);

 chip->codec_mask = azx_readw(chip, STATESTS);

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"azx_reset_end: codec_mask:%8.8X",chip->codec_mask);

 return 1;
}

//-------------------------------------------------------------------------
static unsigned long snd_hda_get_max_freq(struct intelhd_card_s *card)
{
 unsigned long i,freq=0;
 for(i=0; rate_bits[i].hz; i++)
  if((card->supported_formats&(1<<i)) && (rate_bits[i].hz<0xffffffff))
   freq=rate_bits[i].hz;
 return freq;
}

static unsigned int snd_hda_get_max_bits(struct intelhd_card_s *card)
{
 unsigned int bits=16;
 if(card->supported_formats&AC_SUPPCM_BITS_32)
  bits=32;
 else if(card->supported_formats&AC_SUPPCM_BITS_24)
  bits=24;
 else if(card->supported_formats&AC_SUPPCM_BITS_20)
  bits=20;
 return bits;
}

//-------------------------------------------------------------------------
// init & close
static unsigned int snd_ihd_buffer_init(struct mpxplay_audioout_info_s *aui,struct intelhd_card_s *card)
{
 unsigned int bytes_per_sample=(aui->bits_set>16)? 4:2;
 unsigned long allbufsize=BDL_SIZE+1024 + (HDA_CORB_MAXSIZE+HDA_CORB_ALIGN+HDA_RIRB_MAXSIZE+HDA_RIRB_ALGIN), gcap, sdo_offset;
 unsigned int beginmem_aligned;

 allbufsize+=card->pcmout_bufsize=MDma_get_max_pcmoutbufsize(aui,0,AZX_PERIOD_SIZE,bytes_per_sample*aui->chan_card/2,aui->freq_set);
 card->dm=MDma_alloc_cardmem(allbufsize);
 if(!card->dm)
  return 0;

 beginmem_aligned=(((unsigned long)card->dm->linearptr+1023)&(~1023));
 card->table_buffer=(uint32_t *)beginmem_aligned;
 card->pcmout_buffer=(char *)(beginmem_aligned+BDL_SIZE);
 card->corb_buffer = (long*)(((uint32_t)card->pcmout_buffer + card->pcmout_bufsize + HDA_CORB_ALIGN-1)&(~(HDA_CORB_ALIGN-1)));
 card->rirb_buffer = (long long*)(((uint32_t)card->corb_buffer + HDA_CORB_MAXSIZE + HDA_RIRB_ALGIN-1)&(~(HDA_RIRB_ALGIN-1)));
 
 gcap = (unsigned long)azx_readw(card,GCAP);
 if(!(card->config_select & AUCARDSCONFIG_IHD_USE_FIXED_SDO) && (gcap & 0xF000)) // number of playback streams
  sdo_offset = ((gcap >> 8) & 0x0F) * 0x20 + 0x80; // number of capture streams
 else{
  switch(card->board_driver_type){
   case AZX_DRIVER_ATIHDMI:
   case AZX_DRIVER_ATIHDMI_NS: sdo_offset = 0x80; break;
   case AZX_DRIVER_TERA: sdo_offset = 0xe0; break;
   case AZX_DRIVER_ULI: sdo_offset = 0x120; break;
   default: sdo_offset = 0x100;break;
  }
 }

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"cs:%d csc:%d GCAP:%8.8X sdo: %8.8X",
    card->config_select, aui->card_select_config, gcap, sdo_offset);

 card->sd_addr = card->iobase + sdo_offset;
 card->pcmout_period_size = AZX_PERIOD_SIZE;
 card->pcmout_num_periods = card->pcmout_bufsize / card->pcmout_period_size;

 return 1;
}

static void snd_ihd_hw_init(struct intelhd_card_s *card)
{
 azx_init_pci(card);
 azx_reset(card);

 azx_sd_writeb(card, SD_STS, SD_INT_MASK);
 azx_writeb(card, STATESTS, STATESTS_INT_MASK);
 azx_writeb(card, RIRBSTS, RIRB_INT_MASK);
 azx_writel(card, INTSTS, ICH6_INT_CTRL_EN | ICH6_INT_ALL_STREAM);
 #ifdef SBEMU
 azx_writel(card, INTCTL, azx_readl(card, INTCTL) | ICH6_INT_CTRL_EN | ICH6_INT_GLOBAL_EN | ICH6_INT_ALL_STREAM);
 #else
 azx_writel(card, INTCTL, azx_readl(card, INTCTL) | ICH6_INT_CTRL_EN | ICH6_INT_GLOBAL_EN);
 #endif

 azx_writel(card, DPLBASE, 0);
 azx_writel(card, DPUBASE, 0);
}

static unsigned int snd_ihd_mixer_init(struct intelhd_card_s *card)
{
 unsigned int i;
 hda_nid_t nid;

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"snd_ihd_mixer_init start");

 card->codec_vendor_id = snd_hda_param_read(card, AC_NODE_ROOT,AC_PAR_VENDOR_ID);
 if(card->codec_vendor_id <=0)
  card->codec_vendor_id = snd_hda_param_read(card, AC_NODE_ROOT,AC_PAR_VENDOR_ID);
 mpxplay_debugf(IHD_DEBUG_OUTPUT,"codec vendor id:%8.8X",card->codec_vendor_id);

 snd_hda_search_audio_node(card);
 if(!card->afg_root_nodenum)
  goto err_out_mixinit;

 card->def_amp_out_caps = snd_hda_param_read(card, card->afg_root_nodenum, AC_PAR_AMP_OUT_CAP);
 card->def_amp_in_caps = snd_hda_param_read(card, card->afg_root_nodenum, AC_PAR_AMP_IN_CAP);
 card->afg_num_nodes = snd_hda_get_sub_nodes(card, card->afg_root_nodenum, &nid);
 if((card->afg_num_nodes<=0) || !nid)
  goto err_out_mixinit;

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"outcaps:%8.8X incaps:%8.8X afgsubnodes:%d anid:%d",card->def_amp_out_caps,card->def_amp_in_caps,card->afg_num_nodes,(int)nid);

 card->afg_nodes=(struct hda_gnode *)pds_calloc(card->afg_num_nodes,sizeof(struct hda_gnode));
 if(!card->afg_nodes)
  goto err_out_mixinit;

 for(i=0;i<card->afg_num_nodes;i++,nid++)
  snd_hda_add_new_node(card,&card->afg_nodes[i],nid);

 if(!snd_hda_parse_output(card))
  goto err_out_mixinit;

 if(card->dac_node[0]){
  card->supported_formats=card->dac_node[0]->supported_formats;
  if(!card->supported_formats)
   card->supported_formats=snd_hda_param_read(card, card->afg_root_nodenum, AC_PAR_PCM);
   //card->supported_formats=0xffffffff; // !!! then manual try
   
  card->supported_max_freq=snd_hda_get_max_freq(card);
  card->supported_max_bits=snd_hda_get_max_bits(card);
 }
 //printf("FORMATS: %x\n", card->supported_formats);

 for(i=0;i<MAX_PCM_VOLS;i++)
  if(card->pcm_vols[i].node){
   ihd_master_vol.submixerchans[i].submixch_register=card->pcm_vols[i].node->nid;
   ihd_master_vol.submixerchans[i].submixch_max=(card->pcm_vols[i].node->amp_out_caps&AC_AMPCAP_NUM_STEPS)>>AC_AMPCAP_NUM_STEPS_SHIFT;
  }

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"dac0:%d dac1:%d out0:%d out1:%d vol0:%d vol1:%d",
  (int)((card->dac_node[0])? card->dac_node[0]->nid:0),
  (int)((card->dac_node[1])? card->dac_node[1]->nid:0),
  (int)((card->out_pin_node[0])? card->out_pin_node[0]->nid:0),
  (int)((card->out_pin_node[1])? card->out_pin_node[1]->nid:0),
  (int)((card->pcm_vols[0].node)? card->pcm_vols[0].node->nid:0),
  (int)((card->pcm_vols[1].node)? card->pcm_vols[1].node->nid:0));

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"snd_ihd_mixer_init end with success");

 return 1;

err_out_mixinit:
 if(card->afg_nodes){
  pds_free(card->afg_nodes);
  card->afg_nodes=NULL;
 }
 mpxplay_debugf(IHD_DEBUG_OUTPUT,"snd_ihd_mixer_init failed");
 return 0;
}

static void snd_ihd_hw_close(struct intelhd_card_s *card)
{
 azx_writel(card, DPLBASE, 0);
 azx_writel(card, DPUBASE, 0);
 azx_sd_writel(card, SD_BDLPL, 0);
 azx_sd_writel(card, SD_BDLPU, 0);
 azx_sd_writel(card, SD_CTL, 0);
}

static void azx_setup_periods(struct intelhd_card_s *card)
{
 uint32_t *bdl=card->table_buffer;
 unsigned int i;

 card->pcmout_num_periods=card->pcmout_dmasize/card->pcmout_period_size;

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"setup_periods: dmasize:%d periods:%d prsize:%d",card->pcmout_dmasize,card->pcmout_num_periods,card->pcmout_period_size);

 azx_sd_writel(card, SD_BDLPL, 0);
 azx_sd_writel(card, SD_BDLPU, 0);

 for(i=0; i<card->pcmout_num_periods; i++){
  unsigned int off  = i << 2;
  unsigned int addr = ((unsigned int)pds_cardmem_physicalptr(card->dm,card->pcmout_buffer)) + i*card->pcmout_period_size;
  PDS_PUTB_LE32(&bdl[off  ],(uint32_t)addr);
  PDS_PUTB_LE32(&bdl[off+1],0);
  PDS_PUTB_LE32(&bdl[off+2],card->pcmout_period_size);
  #ifdef SBEMU
  PDS_PUTB_LE32(&bdl[off+3],0x01);
  #else
  PDS_PUTB_LE32(&bdl[off+3],0x00); // 0x01 enable interrupt
  #endif
 }
}

static void azx_setup_controller(struct intelhd_card_s *card)
{
 unsigned char val;
 unsigned int stream_tag=1;
 int timeout;

 azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) & ~SD_CTL_DMA_START);
 azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) | SD_CTL_STREAM_RESET);
 pds_delay_10us(100);

 timeout = 300;
 while(!((val = azx_sd_readb(card, SD_CTL)) & SD_CTL_STREAM_RESET) && --timeout)
  pds_delay_10us(100);

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"controller timeout1:%d ",timeout);

 val &= ~SD_CTL_STREAM_RESET;
 azx_sd_writeb(card, SD_CTL, val);
 pds_delay_10us(100);

 timeout = 300;
 while(((val = azx_sd_readb(card, SD_CTL)) & SD_CTL_STREAM_RESET) && --timeout)
  pds_delay_10us(100);

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"timeout2:%d format:%8.8X",timeout,(int)card->format_val);

 azx_sd_writel(card, SD_CTL,(azx_sd_readl(card, SD_CTL) & ~SD_CTL_STREAM_TAG_MASK)| (stream_tag << SD_CTL_STREAM_TAG_SHIFT));
 azx_sd_writel(card, SD_CBL, card->pcmout_dmasize);
 azx_sd_writew(card, SD_LVI, card->pcmout_num_periods - 1);
 azx_sd_writew(card, SD_FORMAT, card->format_val);
 azx_sd_writel(card, SD_BDLPL, (uint32_t)pds_cardmem_physicalptr(card->dm, card->table_buffer));
 azx_sd_writel(card, SD_BDLPU, 0); // upper 32 bit
 //azx_sd_writel(card, SD_CTL,azx_sd_readl(card, SD_CTL) | SD_INT_MASK);
 #ifdef SBEMU
 azx_sd_writel(card, SD_CTL,azx_sd_readl(card, SD_CTL) | SD_INT_COMPLETE);
 #endif
 pds_delay_10us(100);

 if(card->dac_node[0])
  snd_hda_codec_setup_stream(card, card->dac_node[0]->nid, stream_tag, 0, card->format_val);
 if(card->dac_node[1])
  snd_hda_codec_setup_stream(card, card->dac_node[1]->nid, stream_tag, 0, card->format_val);
}

static unsigned int snd_hda_calc_stream_format(struct mpxplay_audioout_info_s *aui,struct intelhd_card_s *card)
{
 unsigned int i,val = 0;
#if !defined(SBEMU)
 if((aui->freq_card<44100) && !aui->freq_set) // under 44100 it sounds terrible on my ALC888, rather we use the freq converter of Mpxplay
  aui->freq_card=44100;
 else
#endif
 if(card->supported_max_freq && (aui->freq_card>card->supported_max_freq))
  aui->freq_card=card->supported_max_freq;

 for(i=0; rate_bits[i].hz; i++)
  if((aui->freq_card<=rate_bits[i].hz) && (card->supported_formats&(1<<i))){
   aui->freq_card=rate_bits[i].hz;
   val = rate_bits[i].hda_fmt;
   break;
  }
  //printf("freq: %d\n", aui->freq_card);

 val |= aui->chan_card - 1;

 if(card->dacout_num_bits>card->supported_max_bits)
  card->dacout_num_bits=card->supported_max_bits;

 if((card->dacout_num_bits<=16) && (card->supported_formats&AC_SUPPCM_BITS_16)){
  val |= 0x10; card->dacout_num_bits=16; aui->bits_card=16;
 }else if((card->dacout_num_bits<=20) && (card->supported_formats&AC_SUPPCM_BITS_20)){
  val |= 0x20; card->dacout_num_bits=20; aui->bits_card=32;
 }else if((card->dacout_num_bits<=24) && (card->supported_formats&AC_SUPPCM_BITS_24)){
  val |= 0x30; card->dacout_num_bits=24; aui->bits_card=32;
 }else if((card->dacout_num_bits<=32) && (card->supported_formats&AC_SUPPCM_BITS_32)){
  val |= 0x40; card->dacout_num_bits=32; aui->bits_card=32;
 }

 return val;
}

//-------------------------------------------------------------------------
static pci_device_s intelhda_devices[]={
 {"Intel CPT6",                  0x8086, 0x1c20, AZX_DRIVER_PCH },
 {"Intel CPT7 (PBG)",            0x8086, 0x1d20, AZX_DRIVER_PCH },
 {"Intel PCH (Panther Point)",   0x8086, 0x1e20, AZX_DRIVER_PCH },
 {"Intel PCH (Lynx Point)",      0x8086, 0x8c20, AZX_DRIVER_PCH },
 {"Intel PCH (9 Series)",        0x8086, 0x8ca0, AZX_DRIVER_PCH },
 {"Intel PCH (Wellsburg)",       0x8086, 0x8d20, AZX_DRIVER_PCH },
 {"Intel PCH (Wellsburg)",       0x8086, 0x8d21, AZX_DRIVER_PCH },
 {"Intel PCH (Lewisburg)",       0x8086, 0xa1f0, AZX_DRIVER_PCH },
 {"Intel PCH (Lewisburg)",       0x8086, 0xa270, AZX_DRIVER_PCH },
 {"Intel PCH (Lynx Point-LP)",   0x8086, 0x9c20, AZX_DRIVER_PCH },
 {"Intel PCH (Lynx Point-LP)",   0x8086, 0x9c21, AZX_DRIVER_PCH },
 {"Intel PCH (Wildcat Point-LP)",0x8086, 0x9ca0, AZX_DRIVER_PCH },
 {"Intel SKL (Sunrise Point)",   0x8086, 0xa170, AZX_DRIVER_SKL },
 {"Intel SKL (Sunrise Point-LP)",0x8086, 0x9d70, AZX_DRIVER_SKL },
 {"Intel SKL (Kabylake)",        0x8086, 0xa171, AZX_DRIVER_SKL },
 {"Intel SKL (Kabylake-LP)",     0x8086, 0x9d71, AZX_DRIVER_SKL },
 {"Intel SKL (Kabylake-H)",      0x8086, 0xa2f0, AZX_DRIVER_SKL },
 {"Intel SKL (Coffelake)",       0x8086, 0xa348, AZX_DRIVER_SKL },
 {"Intel SKL (Cannonlake)",      0x8086, 0x9dc8, AZX_DRIVER_SKL },
 {"Intel SKL (CometLake-LP)",    0x8086, 0x02C8, AZX_DRIVER_SKL },
 {"Intel SKL (CometLake-H)",     0x8086, 0x06C8, AZX_DRIVER_SKL },
 {"Intel SKL (CometLake-H)",     0x8086, 0xf1c8, AZX_DRIVER_SKL },
 {"Intel SKL (CometLake-S)",     0x8086, 0xa3f0, AZX_DRIVER_SKL },
 {"Intel SKL (CometLake-R)",     0x8086, 0xf0c8, AZX_DRIVER_SKL },
 {"Intel SKL (Icelake)",         0x8086, 0x34c8, AZX_DRIVER_SKL },
 {"Intel SKL (Icelake-H)",       0x8086, 0x3dc8, AZX_DRIVER_SKL },
 {"Intel SKL (Jasperlake)",      0x8086, 0x38c8, AZX_DRIVER_SKL },
 {"Intel SKL (Jasperlake)",      0x8086, 0x4dc8, AZX_DRIVER_SKL },
 {"Intel SKL (Tigerlake)",       0x8086, 0xa0c8, AZX_DRIVER_SKL },
 {"Intel SKL (Tigerlake-H)",     0x8086, 0x43c8, AZX_DRIVER_SKL },
 {"Intel SKL (DG1)",             0x8086, 0x490d, AZX_DRIVER_SKL },
 {"Intel SKL (Alderlake-S)",     0x8086, 0x7ad0, AZX_DRIVER_SKL },
 {"Intel SKL (Alderlake-P)",     0x8086, 0x51c8, AZX_DRIVER_SKL },
 {"Intel SKL (Alderlake-M)",     0x8086, 0x51cc, AZX_DRIVER_SKL },
 {"Intel SKL (Elkhart Lake)",    0x8086, 0x4b55, AZX_DRIVER_SKL },
 {"Intel SKL (Elkhart Lake)",    0x8086, 0x4b58, AZX_DRIVER_SKL },
 {"Intel SKL (Broxton-P)",       0x8086, 0x5a98, AZX_DRIVER_SKL },
 {"Intel SKL (Broxton-T)",       0x8086, 0x1a98, AZX_DRIVER_SKL },
 {"Intel SKL (Gemini-Lake)",     0x8086, 0x3198, AZX_DRIVER_SKL },
 {"Intel HDMI (Haswell)",        0x8086, 0x0a0c, AZX_DRIVER_HDMI },
 {"Intel HDMI (Haswell)",        0x8086, 0x0c0c, AZX_DRIVER_HDMI },
 {"Intel HDMI (Haswell)",        0x8086, 0x0d0c, AZX_DRIVER_HDMI },
 {"Intel HDMI (Broadwell)",      0x8086, 0x160c, AZX_DRIVER_HDMI },
 {"Intel SCH (5 Series/3400)",   0x8086, 0x3b56, AZX_DRIVER_SCH },
 {"Intel SCH (Poulsbo)",         0x8086, 0x811b, AZX_DRIVER_SCH },
 {"Intel SCH (Oaktrail)",        0x8086, 0x080a, AZX_DRIVER_SCH },
 {"Intel PCH (BayTrail)",        0x8086, 0x0f04, AZX_DRIVER_PCH },
 {"Intel PCH (Braswell)",        0x8086, 0x2284, AZX_DRIVER_PCH },
 {"Intel ICH6",   0x8086, 0x2668, AZX_DRIVER_ICH },
 {"Intel ICH7",   0x8086, 0x27d8, AZX_DRIVER_ICH },
 {"Intel ESB2",   0x8086, 0x269a, AZX_DRIVER_ICH },
 {"Intel ICH8",   0x8086, 0x284b, AZX_DRIVER_ICH },
 {"Intel ICH",    0x8086, 0x2911, AZX_DRIVER_ICH },
 {"Intel ICH9",   0x8086, 0x293e, AZX_DRIVER_ICH },
 {"Intel ICH9",   0x8086, 0x293f, AZX_DRIVER_ICH },
 {"Intel ICH10",  0x8086, 0x3a3e, AZX_DRIVER_ICH },
 {"Intel ICH10",  0x8086, 0x3a6e, AZX_DRIVER_ICH },
 {"ATI SB450",    0x1002, 0x437b, AZX_DRIVER_ATI },
 {"ATI SB600",    0x1002, 0x4383, AZX_DRIVER_ATI },
 {"AMD Hudson",   0x1022, 0x780d, AZX_DRIVER_ATI }, // snoop type is ATI
 {"AMD X370 & co",0x1022, 0x1457, AZX_DRIVER_ATI }, //
 {"AMD X570 & co",0x1022, 0x1487, AZX_DRIVER_ATI }, //
 {"AMD Stoney",   0x1022, 0x157a, AZX_DRIVER_ATI }, //
 {"AMD Raven",    0x1022, 0x15e3, AZX_DRIVER_ATI }, //
 {"ATI HDNS",     0x1002, 0x0002, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0x1308, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0x157a, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0x15b3, AZX_DRIVER_ATIHDMI_NS },
 {"ATI RS600",    0x1002, 0x793b, AZX_DRIVER_ATIHDMI },
 {"ATI RS690",    0x1002, 0x7919, AZX_DRIVER_ATIHDMI },
 {"ATI RS780",    0x1002, 0x960f, AZX_DRIVER_ATIHDMI },
 {"ATI RS880",    0x1002, 0x970f, AZX_DRIVER_ATIHDMI },
 {"ATI HDNS",     0x1002, 0x9840, AZX_DRIVER_ATIHDMI_NS },
 {"ATI R600",     0x1002, 0xaa00, AZX_DRIVER_ATIHDMI },
 {"ATI RV630",    0x1002, 0xaa08, AZX_DRIVER_ATIHDMI },
 {"ATI RV610",    0x1002, 0xaa10, AZX_DRIVER_ATIHDMI },
 {"ATI RV670",    0x1002, 0xaa18, AZX_DRIVER_ATIHDMI },
 {"ATI RV635",    0x1002, 0xaa20, AZX_DRIVER_ATIHDMI },
 {"ATI RV620",    0x1002, 0xaa28, AZX_DRIVER_ATIHDMI },
 {"ATI RV770",    0x1002, 0xaa30, AZX_DRIVER_ATIHDMI },
 {"ATI RV710",    0x1002, 0xaa38, AZX_DRIVER_ATIHDMI },
 {"ATI HDMI",     0x1002, 0xaa40, AZX_DRIVER_ATIHDMI },
 {"ATI HDMI",     0x1002, 0xaa48, AZX_DRIVER_ATIHDMI },
 {"ATI R5800",    0x1002, 0xaa50, AZX_DRIVER_ATIHDMI },
 {"ATI R5700",    0x1002, 0xaa58, AZX_DRIVER_ATIHDMI },
 {"ATI R5600",    0x1002, 0xaa60, AZX_DRIVER_ATIHDMI },
 {"ATI R5000",    0x1002, 0xaa68, AZX_DRIVER_ATIHDMI },
 {"ATI R6xxx",    0x1002, 0xaa80, AZX_DRIVER_ATIHDMI },
 {"ATI R6800",    0x1002, 0xaa88, AZX_DRIVER_ATIHDMI },
 {"ATI R6xxx",    0x1002, 0xaa90, AZX_DRIVER_ATIHDMI },
 {"ATI R6400",    0x1002, 0xaa98, AZX_DRIVER_ATIHDMI },
 {"ATI HDNS",     0x1002, 0x9902, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaaa0, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaaa8, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaab0, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaac0, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaac8, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaad8, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaae0, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaae8, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaaf0, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaaf8, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab00, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab08, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab10, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab18, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab20, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab28, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab38, AZX_DRIVER_ATIHDMI_NS },
 {"VIA 82xx",     0x1106, 0x3288, AZX_DRIVER_VIA },
 {"VIA 7122",     0x1106, 0x9170, AZX_DRIVER_GENERIC },
 {"VIA 6122",     0x1106, 0x9140, AZX_DRIVER_GENERIC },
 {"SIS 966",      0x1039, 0x7502, AZX_DRIVER_SIS },
 {"ULI M5461",    0x10b9, 0x5461, AZX_DRIVER_ULI },
 {"Teradici",     0x6549, 0x1200, AZX_DRIVER_TERA },
 {"Teradici",     0x6549, 0x2200, AZX_DRIVER_TERA },
 {"CT HDA",       0x1102, 0x0010, AZX_DRIVER_CTHDA },
 {"CT HDA",       0x1102, 0x0012, AZX_DRIVER_CTHDA },
 {"Creative",     0x1102, 0x0009, AZX_DRIVER_GENERIC },
 {"CMedia",       0x13f6, 0x5011, AZX_DRIVER_CMEDIA },
 {"Vortex86MX",   0x17f3, 0x3010, AZX_DRIVER_GENERIC },
 {"VMwareHD",     0x15ad, 0x1977, AZX_DRIVER_GENERIC },
 {"Zhaoxin",      0x1d17, 0x3288, AZX_DRIVER_ZHAOXIN },

 {"NVidia MCP51", 0x10de, 0x026c, AZX_DRIVER_NVIDIA },
 {"NVidia MCP55", 0x10de, 0x0371, AZX_DRIVER_NVIDIA },
 {"NVidia MCP61", 0x10de, 0x03e4, AZX_DRIVER_NVIDIA },
 {"NVidia MCP61", 0x10de, 0x03f0, AZX_DRIVER_NVIDIA },
 {"NVidia MCP65", 0x10de, 0x044a, AZX_DRIVER_NVIDIA },
 {"NVidia MCP65", 0x10de, 0x044b, AZX_DRIVER_NVIDIA },
 {"NVidia MCP67", 0x10de, 0x055c, AZX_DRIVER_NVIDIA },
 {"NVidia MCP67", 0x10de, 0x055d, AZX_DRIVER_NVIDIA },
 {"NVidia MCP77", 0x10de, 0x0774, AZX_DRIVER_NVIDIA },
 {"NVidia MCP77", 0x10de, 0x0775, AZX_DRIVER_NVIDIA },
 {"NVidia MCP77", 0x10de, 0x0776, AZX_DRIVER_NVIDIA },
 {"NVidia MCP77", 0x10de, 0x0777, AZX_DRIVER_NVIDIA },
 {"NVidia MCP73", 0x10de, 0x07fc, AZX_DRIVER_NVIDIA },
 {"NVidia MCP73", 0x10de, 0x07fd, AZX_DRIVER_NVIDIA },
 {"NVidia MCP79", 0x10de, 0x0ac0, AZX_DRIVER_NVIDIA },
 {"NVidia MCP79", 0x10de, 0x0ac1, AZX_DRIVER_NVIDIA },
 {"NVidia MCP79", 0x10de, 0x0ac2, AZX_DRIVER_NVIDIA },
 {"NVidia MCP79", 0x10de, 0x0ac3, AZX_DRIVER_NVIDIA },
 {"NVidia MCP",   0x10de, 0x0d94, AZX_DRIVER_NVIDIA },
 {"NVidia MCP",   0x10de, 0x0d95, AZX_DRIVER_NVIDIA },
 {"NVidia MCP",   0x10de, 0x0d96, AZX_DRIVER_NVIDIA },
 {"NVidia MCP",   0x10de, 0x0d97, AZX_DRIVER_NVIDIA },
 //{"AMD Generic",     0x1002, 0x0000, AZX_DRIVER_GENERIC }, // TODO: cannot define these
 //{"NVidia Generic",  0x10de, 0x0000, AZX_DRIVER_GENERIC },

 {NULL,0,0,0}
};

static struct codec_vendor_list_s codecvendorlist[]={
 {0x1002,"ATI"},
 {0x1013,"Cirrus Logic"},
 {0x1057,"Motorola"},
 {0x1095,"Silicon Image"},
 {0x10de,"Nvidia"},
 {0x10ec,"Realtek"},
 {0x1022,"AMD"},
 {0x1102,"Creative"},
 {0x1106,"VIA"},
 {0x111d,"IDT"},
 {0x11c1,"LSI"},
 {0x11d4,"Analog Devices"},
 {0x1d17,"Zhaoxin"},
 {0x13f6,"C-Media"},
 {0x14f1,"Conexant"},
 {0x17e8,"Chrontel"},
 {0x1854,"LG"},
 {0x1aec,"Wolfson"},
 {0x434d,"C-Media"},
 {0x8086,"Intel"},
 {0x8384,"SigmaTel"},
 {0x0000,"Unknown"}
};

static void INTELHD_close(struct mpxplay_audioout_info_s *aui);

static char *ihd_search_vendorname(unsigned int vendorid)
{
 struct codec_vendor_list_s *cl=&codecvendorlist[0];
 do{
  if(cl->vendor_id==vendorid)
   break;
  cl++;
 }while(cl->vendor_id);
 return cl->vendor_name;
}

static void INTELHD_card_info(struct mpxplay_audioout_info_s *aui)
{
 struct intelhd_card_s *card=aui->card_private_data;
 char sout[100];
 sprintf(sout,"IHD : %s (%4.4X%4.4X) -> %s (%8.8X) (max %dkHz/%dbit%s/%dch)",
         card->pci_dev->device_name,
         (long)card->pci_dev->vendor_id,(long)card->pci_dev->device_id,
         ihd_search_vendorname(card->codec_vendor_id>>16),card->codec_vendor_id,
         (card->supported_max_freq/1000),card->supported_max_bits,
         ((card->supported_formats==0xffffffff)? "?":""),
         min(INTHD_MAX_CHANNELS,PCM_MAX_CHANNELS)
         );
 pds_textdisplay_printf(sout);
}

static int INTELHD_adetect(struct mpxplay_audioout_info_s *aui)
{
 struct intelhd_card_s *card;
 unsigned int i;
#ifdef MPXPLAY_USE_DEBUGF
 unsigned long prevtime = pds_gettimem();
#endif

 card=(struct intelhd_card_s *)pds_calloc(1,sizeof(struct intelhd_card_s));
 if(!card)
  return 0;
 aui->card_private_data=card;

 card->pci_dev=(struct pci_config_s *)pds_calloc(1,sizeof(struct pci_config_s));
 if(!card->pci_dev)
  goto err_adetect;
 if(pcibios_search_devices(intelhda_devices,card->pci_dev)!=PCI_SUCCESSFUL)
  goto err_adetect;

 card->iobase = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR);
 if(card->iobase&0x1) // we handle memory mapping only
  card->iobase=0;
 if(!card->iobase)
  goto err_adetect;
 card->iobase&=0xfffffff8;
 card->iobase = pds_dpmi_map_physical_memory(card->iobase,16384);
 if(!card->iobase)
  goto err_adetect;
 if(aui->card_select_config>=0)
  card->config_select=aui->card_select_config;

 card->board_driver_type=card->pci_dev->device_type;
 if(!snd_ihd_buffer_init(aui,card))
  goto err_adetect;

 aui->card_DMABUFF=card->pcmout_buffer;
 #ifdef SBEMU
 aui->card_irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
 #endif

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"IHD board type: %s (%4.4X%4.4X) ",card->pci_dev->device_name,(long)card->pci_dev->vendor_id,(long)card->pci_dev->device_id);

 snd_ihd_hw_init(card);

#ifdef MPXPLAY_USE_DEBUGF
 mpxplay_debugf(IHD_DEBUG_OUTPUT,"PCI & hw_init time: %d ms", pds_gettimem()-prevtime);
 prevtime = pds_gettimem();
#endif

 for(i=0;i<AZX_MAX_CODECS;i++){
  if(card->codec_mask&(1<<i)){
   card->codec_index=i;
   if(snd_ihd_mixer_init(card))
    break;
  }
 }

 mpxplay_debugf(IHD_DEBUG_OUTPUT,"MIXER init time: %d ms", pds_gettimem()-prevtime);

 return 1;

err_adetect:
 INTELHD_close(aui);
 return 0;
}

static void INTELHD_close(struct mpxplay_audioout_info_s *aui)
{
 struct intelhd_card_s *card=aui->card_private_data;
 if(card){
  if(card->iobase){
   snd_ihd_hw_close(card);
   pds_dpmi_unmap_physycal_memory(card->iobase);
  }
  if(card->afg_nodes)
   pds_free(card->afg_nodes);
  MDma_free_cardmem(card->dm);
  if(card->pci_dev)
   pds_free(card->pci_dev);
  pds_free(card);
  aui->card_private_data=NULL;
 }
}

static void INTELHD_setrate(struct mpxplay_audioout_info_s *aui)
{
 struct intelhd_card_s *card=aui->card_private_data;

 aui->card_wave_id=0x0001;
 aui->chan_card=(aui->chan_set)? aui->chan_set:PCM_CHANNELS_DEFAULT;
 if(aui->chan_card>INTHD_MAX_CHANNELS)
  aui->chan_card=INTHD_MAX_CHANNELS;
 if(!card->dacout_num_bits) // first initialization
  card->dacout_num_bits=(aui->bits_set)? aui->bits_set:16;

 card->format_val=snd_hda_calc_stream_format(aui,card);
 card->pcmout_dmasize=MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,AZX_PERIOD_SIZE,0);

 azx_setup_periods(card);
 azx_setup_controller(card);
}

static void INTELHD_start(struct mpxplay_audioout_info_s *aui)
{
 struct intelhd_card_s *card=aui->card_private_data;
 unsigned int timeout;
 //const unsigned int stream_index=0;
 //azx_writeb(card, INTCTL, azx_readb(card, INTCTL) | (1 << stream_index)); // enable SIE
 //azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) | SD_CTL_DMA_START | SD_INT_MASK); // start DMA

 azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) | SD_CTL_DMA_START); // start DMA

 timeout = 500;
 while(!(azx_sd_readb(card, SD_CTL) & SD_CTL_DMA_START) && --timeout) // wait for DMA start
  pds_delay_10us(100);

 pds_delay_10us(100);
}

static void INTELHD_stop(struct mpxplay_audioout_info_s *aui)
{
 struct intelhd_card_s *card=aui->card_private_data;
 unsigned int timeout;
 //const unsigned int stream_index=0;
 azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) & ~(SD_CTL_DMA_START | SD_INT_MASK)); // stop DMA

 timeout = 200;
 while((azx_sd_readb(card, SD_CTL) & SD_CTL_DMA_START) && --timeout) // wait for DMA stop
  pds_delay_10us(100);

 pds_delay_10us(100);

 //azx_sd_writeb(card, SD_STS, SD_INT_MASK); // to be sure
 //azx_writeb(card, INTCTL,azx_readb(card, INTCTL) & ~(1 << stream_index));
}

static long INTELHD_getbufpos(struct mpxplay_audioout_info_s *aui)
{
 struct intelhd_card_s *card=aui->card_private_data;
 unsigned long bufpos;

 bufpos=azx_sd_readl(card, SD_LPIB);

 //mpxplay_debugf(IHD_DEBUG_OUTPUT,"bufpos1:%d sts:%8.8X ctl:%8.8X cbl:%d ds:%d ps:%d pn:%d",bufpos,azx_sd_readb(card, SD_STS),azx_sd_readl(card, SD_CTL),azx_sd_readl(card, SD_CBL),aui->card_dmasize,
 // card->pcmout_period_size,card->pcmout_num_periods);

 if(bufpos<aui->card_dmasize)
  aui->card_dma_lastgoodpos=bufpos;

 return aui->card_dma_lastgoodpos;
}

//--------------------------------------------------------------------------
//mixer

static void INTELHD_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
{
 struct intelhd_card_s *card=aui->card_private_data;
 snd_hda_put_vol_mute(card,reg,0,HDA_OUTPUT,0,val);
 snd_hda_put_vol_mute(card,reg,1,HDA_OUTPUT,0,val);
}

static unsigned long INTELHD_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
{
 struct intelhd_card_s *card=aui->card_private_data;
 return snd_hda_get_vol_mute(card,reg,0,HDA_OUTPUT,0);
}

#ifdef SBEMU
static int INTELHD_IRQRoutine(mpxplay_audioout_info_s* aui)
{
  struct intelhd_card_s *card=aui->card_private_data;
  int status = azx_sd_readb(card, SD_STS)&SD_INT_MASK;
  if(status)
    azx_sd_writeb(card, SD_STS, status); //ack all

  //ack CORB/RIRB status
  int corbsts = azx_readb(card, CORBSTS)&0x1;
  int rirbsts = azx_readb(card, RIRBSTS)&RIRB_INT_MASK;
  if(corbsts)
    azx_writeb(card, CORBSTS, corbsts);
  if(rirbsts)
    azx_writeb(card, RIRBSTS, rirbsts);
  return status || corbsts || rirbsts;
}
#endif

static aucards_allmixerchan_s ihd_mixerset[]={
 &ihd_master_vol,
 NULL
};

one_sndcard_info IHD_sndcard_info={
 "Intel HDA",
 SNDCARD_LOWLEVELHAND|SNDCARD_INT08_ALLOWED,

 NULL,                  // card_config
 NULL,                  // no init
 &INTELHD_adetect,      // only autodetect
 &INTELHD_card_info,
 &INTELHD_start,
 &INTELHD_stop,
 &INTELHD_close,
 &INTELHD_setrate,

 &MDma_writedata,
 &INTELHD_getbufpos,
 &MDma_clearbuf,
 &MDma_interrupt_monitor,
 #if SBEMU
 &INTELHD_IRQRoutine,
 #else
 NULL,
 #endif

 &INTELHD_writeMIXER,
 &INTELHD_readMIXER,
 &ihd_mixerset[0]
};

#endif // AU_CARDS_LINK_IHD
