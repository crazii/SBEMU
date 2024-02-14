#include "au_cards.h"

#define hw_fm_outb(reg,data) outp(aui->fm_port+reg,data)
#define hw_fm_inb(reg) inp(aui->fm_port+reg)

uint8_t ioport_fm_read (struct mpxplay_audioout_info_s *aui, unsigned int idx)
{
  return hw_fm_inb(idx);
}

void ioport_fm_write (struct mpxplay_audioout_info_s *aui, unsigned int idx, uint8_t data)
{
  hw_fm_outb(idx, data);
}

#define hw_mpu_outb(reg,data) outp(aui->mpu401_port+reg,data)
#define hw_mpu_inb(reg) inp(aui->mpu401_port+reg)

uint8_t ioport_mpu401_read (struct mpxplay_audioout_info_s *aui, unsigned int idx)
{
  return hw_mpu_inb(idx);
}

void ioport_mpu401_write (struct mpxplay_audioout_info_s *aui, unsigned int idx, uint8_t data)
{
  hw_mpu_outb(idx, data);
}
