#ifndef pcibios_h
#define pcibios_h

#if defined(HAVE_INTTYPES_H) || (__WATCOMC__>=1240) || defined(DJGPP) // OW 1.4

#include <inttypes.h>

#else

#ifndef uint8_t
typedef unsigned char    uint8_t;
#endif
#ifndef uint16_t
typedef unsigned short    uint16_t;
#endif
#ifndef int32_t
typedef long            int32_t;
#endif
#ifndef uint32_t
typedef unsigned long    uint32_t;
#endif

#endif

//for Linux/GCC/ALSA routines
#ifndef outb
#define outb(reg,val) outp(reg,val)
#endif
#ifndef outw
#define outw(reg,val) outpw(reg,val)
#endif
#ifndef outl
#define outl(reg,val) outpd(reg,val)
#endif
#ifndef inb
#define inb(reg) inp(reg)
#endif
#ifndef inw
#define inw(reg) inpw(reg)
#endif
#ifndef inl
#define inl(reg) inpd(reg)
#endif

/*#define mpxp_outb(reg,val) outp(reg,val)
#define mpxp_outw(reg,val) outpw(reg,val)
#define mpxp_outl(reg,val) outpd(reg,val)
#define mpxp_inb(reg) inp(reg)
#define mpxp_inw(reg) inpw(reg)
#define mpxp_inl(reg) inpd(reg)*/

#define    HiW(dw)    (uint16_t)((uint32_t)dw >> 16)
#define    LoW(dw)    (uint16_t)((uint32_t)dw & 0xFFFF)

#define PCI_FUNCTION_ID        0xB1
#define PCI_BIOS_PRESENT    0x01
#define PCI_FIND_DEVICE        0x02
#define PCI_READ_BYTE        0x08
#define PCI_READ_WORD        0x09
#define PCI_READ_DWORD        0x0A
#define PCI_WRITE_BYTE        0x0B
#define PCI_WRITE_WORD        0x0C
#define PCI_WRITE_DWORD        0x0D
#define PCI_GET_ROUTING        0x0E
#define PCI_SET_INTERRUPT    0x0F
#define PCI_SERVICE        0x1A

#define PCI_SUCCESSFUL        0x00
#define PCI_NOT_SUPPORTED    0x81
#define PCI_BAD_VEMDERID    0x83
#define PCI_DEVICE_NOTFOUND    0x86
#define PCI_BAD_REGNUMBER    0x87
#define PCI_SET_FAILED        0x88
#define PCI_BUFFER_SMALL    0x98

#define PCIR_VID        0x00 // vendor id
#define    PCIR_DID        0x02 // device id
#define    PCIR_PCICMD        0x04 // command (read/write)
#define PCIR_STATUS             0x06 // read status (16 bits)
#define    PCIR_RID        0x08 // chiprev
#define    PCIR_CCODE        0x0A
#define    PCIR_HEADT        0x0E // head type
#define    PCIR_NAMBAR        0x10 // Native Audio Mixer Base Address
#define    PCIR_NABMBAR        0x14 // Native Audio Bus Mastering Base Address
#define    PCIR_SSVID        0x2C // subsystem id + vendor id (serial)
#define    PCIR_SSID        0x2E // subsystem id (model)
#define    PCIR_INTR_LN        0x3C // irq number
#define    PCIR_INTR_PIN        0x3D

typedef struct pci_config_s{
 uint8_t bBus;
 uint8_t bDev;
 uint8_t bFunc;
 uint16_t vendor_id;
 uint16_t device_id;
 char *device_name;     // from pci_device_s
 uint16_t device_type;  // from pci_device_s
}pci_config_s;

typedef struct pci_device_s{
 char *device_name;     // a short id string
 uint16_t vendor_id;    // pci vendor id
 uint16_t device_id;    // pci device id
 uint16_t device_type;  // internal device type
}pci_device_s;

//uint8_t  pcibios_GetBus(void);
uint8_t     pcibios_FindDevice(uint16_t, uint16_t, pci_config_s *);
uint8_t  pcibios_search_devices(pci_device_s devices[],pci_config_s *);
uint8_t     pcibios_ReadConfig_Byte (pci_config_s *, uint16_t);
uint16_t pcibios_ReadConfig_Word (pci_config_s *, uint16_t);
uint32_t pcibios_ReadConfig_Dword(pci_config_s *, uint16_t);
void     pcibios_WriteConfig_Byte(pci_config_s *, uint16_t, uint8_t);
void     pcibios_WriteConfig_Word(pci_config_s *, uint16_t, uint16_t);
void     pcibios_WriteConfig_Dword(pci_config_s *, uint16_t, uint32_t);
void     pcibios_set_master(pci_config_s *);
void     pcibios_enable_memmap_set_master(pci_config_s *);

#endif
