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
#if 0 // shouldn't DOS programs be doing this themselves if it was really necessary?
  if (idx == 0 && aui->mpu401_softread) {
    int timeout = 10000; // 100ms
    do {
      uint8_t st = hw_mpu_inb(1);
      if (!(st & 0x40)) break;
      // still full
      pds_delay_10us(1);
    } while (--timeout);
  }
#endif
  hw_mpu_outb(idx, data);
}

void ioport_mpu401_write_when_ready (struct mpxplay_audioout_info_s *aui, unsigned int idx, uint8_t data)
{
  if (idx == 0) {
    int timeout = 10000; // 100ms
    do {
      uint8_t st = hw_mpu_inb(1);
      if (!(st & 0x40)) break;
      // still full
      pds_delay_10us(1);
    } while (--timeout);
  }
  hw_mpu_outb(idx, data);
}

int ioport_detect_opl (uint16_t fmport)
{
#define OPL_write(reg, val) do { outp(fmport, reg); pds_delay_10us(1); outp(fmport+1, val); pds_delay_10us(3); } while (0)
#define OPL_status() (inp(fmport) & 0xe0)
  OPL_write(0x04, 0x60); // Reset Timer 1 and Timer 2
  OPL_write(0x04, 0x80); // Reset the IRQ
  uint8_t fmsts1 = OPL_status();
  //printf("fmsts1: %x\n", fmsts1);
  OPL_write(0x02, 0xff); // Set Timer 1 to ff
  OPL_write(0x04, 0x21); // Unmask and start Timer 1
  pds_delay_10us(8); // Delay at least 80us
  uint8_t fmsts2 = OPL_status();
  OPL_write(0x04, 0x60); // Reset Timer 1 and Timer 2
  OPL_write(0x04, 0x80); // Reset the IRQ
  //printf("fmsts2: %x\n", fmsts2);
  if (!(fmsts1 == 0 && fmsts2 == 0xc0)) {
    //printf("No OPL detected\n");
    return 0;
  } else {
    uint8_t fmsts3 = inp(fmport) & 0x06;
    //printf("fmsts3: %x\n", fmsts3);
    if (fmsts3 == 0) {
      return 3;
    }
  }
  return 2; // We found something, probably OPL2
}
