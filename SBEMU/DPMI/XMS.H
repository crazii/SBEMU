#ifndef _DPMI_XMS_H_
#define _DPMI_XMS_H_
#include <PLATFORM.H>

#ifdef __cplusplus
extern "C"
{
#endif

//return physical addr
uint16_t XMS_Alloc(uint16_t sizeKB, uint32_t* outputp addr);

BOOL XMS_Realloc(uint16_t handle, uint16_t newSizeKB, uint32_t* outputp addr);

BOOL XMS_Free(uint16_t handle);

//XMS3.0 optional
 //size in paragraphs (16 byte). return segmenet addr, or NULL if not supported or not UMB
uint16_t XMS_AllocUMB(uint16_t size16B);

BOOL XMS_FreeUMB(uint16_t segment);

#ifdef __cplusplus
}
#endif

#endif//_DPMI_XMS_H_